// Authored albedo times the face light (baked or model light) plus the radial point light.
// SDL_GPU fragment conventions: textures/samplers in space2, uniform buffers in space3.
Texture2D<float4> albedo : register(t0, space2);
SamplerState albedoSampler : register(s0, space2);
Texture2D<float4> lightmap : register(t1, space2);
SamplerState lightmapSampler : register(s1, space2);
Texture2D<float4> shadowAtlas : register(t2, space2);
SamplerState shadowSampler : register(s2, space2);
cbuffer Light : register(b0, space3)
{
    // modulation: alpha, lightmap scale, shadow receiver, per-pixel model light.
    float4 modulation, positionRadius, lightColor;
    // Per-pixel model light (modulation.w): ambient (w: source count), instance color (w: light
    // scale) and three float4 per source, as the skinned vertex light.
    float4 modelAmbient, modelColor;
    float4 modelSources[12];
};
// Model shadows, pushed once per record. Per caster: tile (u0, v0, size, box top), footprint at
// the top (min x, min z, 1/size x, 1/size z), (feet, bias, slope x, slope z): a point projects
// along the light onto the top as xz + slope * (y - top). Stored texels are caster heights.
cbuffer Shadows : register(b1, space3)
{
    float4 shadowInfo;   // x: caster count, y: strength, z: fade range below the feet, w: atlas texel.
    float4 shadowEye;    // xyz: camera, orienting the face normal toward the viewer.
    float4 shadowBounds; // Union of every caster box: min x, min z, max x, max z.
    float4 shadowHeight; // x: union top; receivers above it skip the casters.
    float4 casters[96];
};
struct Input
{
    float3 faceColor : TEXCOORD0;
    float2 texCoord : TEXCOORD1;
    float3 worldPosition : TEXCOORD2;
    float2 lightmapCoord : TEXCOORD3; // Negative: no lightmap on this face.
    float3 worldNormal : TEXCOORD4;
};
// Fraction of the baked light removed by model shadows on an upward-facing receiver:
// 2x2 coverage of tile texels whose caster surface lies above this point, faded with the
// distance below the caster's feet. Only receivers (modulation.z > 0) evaluate it.
float shadowAmount(float3 p)
{
    if (modulation.z <= 0.0 || shadowInfo.x < 1.0)
        return 0.0;
    float3 normal = cross(ddx(p), ddy(p));
    normal *= rsqrt(max(dot(normal, normal), 1e-20));
    if (dot(normal, shadowEye.xyz - p) < 0.0)
        normal = -normal;
    float facing = saturate((normal.y - 0.25) * 2.0);
    if (facing <= 0.0 || p.y > shadowHeight.x || any(p.xz < shadowBounds.xy) || any(p.xz > shadowBounds.zw))
        return 0.0;
    float amount = 0.0;
    uint count = min(uint(shadowInfo.x), 32u);
    for (uint i = 0u; i < count; ++i)
    {
        float4 tile = casters[i * 3u], footprint = casters[i * 3u + 1u], heights = casters[i * 3u + 2u];
        float2 t = (p.xz + heights.zw * (p.y - tile.w) - footprint.xy) * footprint.zw;
        float fade = saturate(1.0 - (heights.x - p.y) / shadowInfo.z);
        if (any(t < 0.0) || any(t > 1.0) || fade <= 0.0)
            continue;
        float2 uv = tile.xy + float2(t.x, 1.0 - t.y) * tile.z;
        float covered = 0.0;
        for (uint k = 0u; k < 4u; ++k)
        {
            float2 offset = float2((k & 1u) != 0u ? 0.5 : -0.5, (k & 2u) != 0u ? 0.5 : -0.5) * shadowInfo.w;
            covered += shadowAtlas.SampleLevel(shadowSampler, uv + offset, 0.0).r - p.y > heights.y ? 0.25 : 0.0;
        }
        amount = max(amount, covered * fade);
    }
    return amount * facing * shadowInfo.y;
}
// The skinned vertex light term evaluated per pixel with the interpolated normal: the original
// light term times Lambert N.L, clamped, then the instance color and light scale.
float3 modelLight(Input input)
{
    float3 normal = input.worldNormal * rsqrt(max(dot(input.worldNormal, input.worldNormal), 1e-12));
    float3 light = modelAmbient.rgb;
    uint count = min(uint(modelAmbient.w), 4u);
    for (uint i = 0u; i < count; ++i)
    {
        float3 toLight = modelSources[i * 3u].xyz - input.worldPosition;
        float distanceToLight = max(length(toLight), 1e-4);
        float3 direction = toLight / distanceToLight;
        float attenuation = saturate(1.0 - distanceToLight / modelSources[i * 3u].w);
        float coneCosine = modelSources[i * 3u + 1u].w;
        float cone = coneCosine >= -1.0
                         ? saturate((dot(-direction, modelSources[i * 3u + 2u].xyz) - coneCosine) / (1.0 - coneCosine))
                         : 1.0;
        light += modelSources[i * 3u + 1u].rgb * (attenuation * cone * max(0.0, dot(normal, direction)));
    }
    return modelColor.rgb * min(light, float3(1.0, 1.0, 1.0)) * modelColor.w;
}
// Baked light, darkened by model shadows, plus the bounded radial point light, added before the
// albedo multiply as the original renderer adds dynamic lights to baked light. A lightmapped
// face reads its lightmap: the sum saturates like the original framebuffer, then scales by
// modulation.y (2 under the shipped "Saturate" default). Other faces keep their face color
// (vertex light, model light or sprite tint) unclamped.
float3 illumination(Input input, float shadow)
{
    float radial = positionRadius.w > 0.0
                       ? max(0.0, 1.0 - distance(input.worldPosition, positionRadius.xyz) / positionRadius.w)
                       : 0.0;
    float3 dynamicLight = lightColor.xyz * radial;
    float3 baked = lightmap.SampleLevel(lightmapSampler, max(input.lightmapCoord, float2(0.0, 0.0)), 0.0).rgb;
    return input.lightmapCoord.x >= 0.0
               ? modulation.y * min(baked * (1.0 - shadow) + dynamicLight, float3(1.0, 1.0, 1.0))
               : (modulation.w > 0.5 ? modelLight(input) : input.faceColor) * (1.0 - shadow) + dynamicLight;
}
float4 main(Input input) : SV_Target0
{
    float shadow = shadowAmount(input.worldPosition);
    return float4(albedo.Sample(albedoSampler, input.texCoord).rgb * illumination(input, shadow), 1.0);
}
