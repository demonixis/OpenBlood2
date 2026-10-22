// Enhanced renderer: physically based shading of world faces and lit models on top of the
// original light data. Diffuse light is the original term (lightmap, baked vertex light or
// model light, model shadows and the radial point light) so rough dielectrics keep the
// original look; GGX/Smith/Schlick specular comes from the static light objects (world: the
// light grid; models: their four sources) and the radial light, and the baked light stands in
// for the environment through an analytic split-sum approximation. No probe or baked data.
// Manifest emission adds a light-independent glow to bright texels (the input of M6 bloom).
// PBR_HIGH: eight grid lights and specular cavity. PBR_ALPHA: translucent output.
// GLOW_TARGET (bloom passes): target 1 receives the linear light the emission adds.
// SDL_GPU fragment conventions: textures/samplers then storage buffers in space2, uniforms in space3.
Texture2D<float4> albedo : register(t0, space2);
SamplerState albedoSampler : register(s0, space2);
Texture2D<float4> lightmap : register(t1, space2);
SamplerState lightmapSampler : register(s1, space2);
Texture2D<float4> shadowAtlas : register(t2, space2);
SamplerState shadowSampler : register(s2, space2);
Texture2D<float4> normalMap : register(t3, space2); // RGB tangent normal, A height (cavity).
SamplerState normalSampler : register(s3, space2);
StructuredBuffer<float4> gridLights : register(t4, space2); // Three float4 per static light.
StructuredBuffer<uint> gridCells : register(t5, space2);    // Cells, then their light indices.
cbuffer Light : register(b0, space3)
{
    float4 modulation, positionRadius, lightColor;
    float4 modelAmbient, modelColor;
    float4 modelSources[12];
    float4 sceneColor;
};
cbuffer Shadows : register(b1, space3)
{
    float4 shadowInfo;
    float4 shadowEye;
    float4 shadowBounds;
    float4 shadowHeight;
    float4 casters[96];
};
cbuffer Frame : register(b2, space3)
{
    float4 frameEye;   // xyz: camera.
    float4 gridOrigin; // xyz: first cell corner; w: 1 / cell size.
    uint4 gridSize;    // xyz: cells per axis; w: cell count (first light index entry).
};
cbuffer Material : register(b3, space3)
{
    float4 material; // Roughness, metallic, F0 scale, flags (1 lit model, 2 normal map).
    float4 emission; // Strength, luminance threshold.
};
struct Input
{
    float3 faceColor : TEXCOORD0;
    float2 texCoord : TEXCOORD1;
    float3 worldPosition : TEXCOORD2;
    float2 lightmapCoord : TEXCOORD3;
    float3 worldNormal : TEXCOORD4;
};
#ifdef PBR_HIGH
#define GRID_LIGHTS 8u
#else
#define GRID_LIGHTS 4u
#endif
static const float PI = 3.14159265;
static const float SATURATE = 2.0; // Shipped "Saturate": N.L light terms are doubled.

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
// The original light term of one source: color * (1 - d/R) * linear cone; direction in `l`.
float3 sourceTerm(float4 positionRange, float4 colorCone, float3 axis, float3 p, out float3 l)
{
    float3 toLight = positionRange.xyz - p;
    float distanceToLight = max(length(toLight), 1e-4);
    l = toLight / distanceToLight;
    float attenuation = saturate(1.0 - distanceToLight / positionRange.w);
    float cone = colorCone.w >= -1.0 ? saturate((dot(-l, axis) - colorCone.w) / (1.0 - colorCone.w)) : 1.0;
    return colorCone.rgb * (attenuation * cone);
}
// Cook-Torrance GGX with height-correlated Smith visibility (Hammon) and Schlick Fresnel,
// times N.L. Multiplied by PI so a light's 0-1 color keeps the original Lambert scale.
float3 specular(float3 n, float3 v, float3 l, float3 f0, float alpha)
{
    float nl = saturate(dot(n, l));
    if (nl <= 0.0)
        return float3(0.0, 0.0, 0.0);
    float3 h = normalize(v + l);
    float nv = max(dot(n, v), 1e-4), nh = saturate(dot(n, h)), vh = saturate(dot(v, h));
    float a2 = alpha * alpha;
    float d = nh * nh * (a2 - 1.0) + 1.0;
    float distribution = a2 / (PI * d * d);
    float visibility = 0.5 / lerp(2.0 * nl * nv, nl + nv, alpha);
    float3 fresnel = f0 + (1.0 - f0) * pow(1.0 - vh, 5.0);
    return distribution * visibility * fresnel * (PI * nl);
}
// Karis' analytic approximation of the preintegrated environment BRDF (mobile split sum).
float3 environmentBrdf(float3 f0, float roughness, float nv)
{
    const float4 c0 = float4(-1.0, -0.0275, -0.572, 0.022);
    const float4 c1 = float4(1.0, 0.0425, 1.04, -0.04);
    float4 r = roughness * c0 + c1;
    float a004 = min(r.x * r.x, exp2(-9.28 * nv)) * r.x + r.y;
    float2 ab = float2(-1.04, 1.04) * a004 + r.zw;
    return f0 * ab.x + ab.y;
}
// The original per-pixel model light (world.frag modelLight).
float3 originalModelLight(Input input)
{
    float3 normal = input.worldNormal * rsqrt(max(dot(input.worldNormal, input.worldNormal), 1e-12));
    float3 light = modelAmbient.rgb;
    uint count = min(uint(modelAmbient.w), 4u);
    for (uint i = 0u; i < count; ++i)
    {
        float3 l;
        float3 term =
            sourceTerm(modelSources[i * 3u], modelSources[i * 3u + 1u], modelSources[i * 3u + 2u].xyz, input.worldPosition, l);
        light += term * max(0.0, dot(normal, l));
    }
    return modelColor.rgb * min(light, float3(1.0, 1.0, 1.0)) * modelColor.w;
}
float luminance(float3 c)
{
    return dot(c, float3(0.299, 0.587, 0.114));
}
// Emission: texels above the class's luminance threshold glow with their own color, ramping
// from the threshold to white, independent of any light.
float3 emitted(float3 color)
{
    if (emission.x <= 0.0)
        return float3(0.0, 0.0, 0.0);
    return color * (emission.x * saturate((luminance(color) - emission.y) / max(1.0 - emission.y, 1e-3)));
}
#ifdef GLOW_TARGET
struct Output
{
    float4 color : SV_Target0;
    float4 glow : SV_Target1; // Linear glow, premultiplied by coverage (translucent faces).
};
Output shaded(float4 color, float4 glow)
{
    Output output;
    output.color = color;
    output.glow = glow;
    return output;
}
float3 toLinear(float3 c)
{
    c = max(c, float3(0.0, 0.0, 0.0));
    return lerp(pow((c + 0.055) / 1.055, 2.4), c / 12.92, step(c, 0.04045));
}
// The linear light that emission e adds to the display-space color c (c includes e).
float3 linearGlow(float3 c, float3 e)
{
    return max(toLinear(c) - toLinear(c - e), float3(0.0, 0.0, 0.0));
}
#define SHADED(color, glow) shaded(color, glow)
Output main(Input input)
#else
#define SHADED(color, glow) (color)
float4 main(Input input) : SV_Target0
#endif
{
    float shadow = shadowAmount(input.worldPosition); // Before discard: screen derivatives.
    float3 dp1 = ddx(input.worldPosition), dp2 = ddy(input.worldPosition);
    float2 duv1 = ddx(input.texCoord), duv2 = ddy(input.texCoord);
    float4 texel = albedo.Sample(albedoSampler, input.texCoord);
    float4 relief = normalMap.Sample(normalSampler, input.texCoord);
#ifdef PBR_ALPHA
    if (texel.a == 0.0)
        discard;
#endif
    uint flags = uint(material.w);
    if ((flags & 4u) != 0u)
    {
        // Unlit, flat and FLAG_NOLIGHT models: the original per-pixel model light or face color.
        float3 light = modulation.w > 0.5 ? originalModelLight(input) : input.faceColor;
        float3 dynamicLight = positionRadius.w > 0.0
                                  ? lightColor.xyz * max(0.0, 1.0 - distance(input.worldPosition, positionRadius.xyz) /
                                                                       positionRadius.w)
                                  : float3(0.0, 0.0, 0.0);
#ifdef PBR_ALPHA
        return SHADED(float4(texel.rgb * (light * (1.0 - shadow) + dynamicLight) * sceneColor.rgb,
                             texel.a * modulation.x),
                      float4(0.0, 0.0, 0.0, texel.a * modulation.x));
#else
        return SHADED(float4(texel.rgb * (light * (1.0 - shadow) + dynamicLight) * sceneColor.rgb, 1.0),
                      float4(0.0, 0.0, 0.0, 1.0));
#endif
    }
    bool model = (flags & 1u) != 0u;
    float3 p = input.worldPosition;
    float3 v = normalize(frameEye.xyz - p);
    // Geometric normal (world faces: the plane normal), facing the viewer.
    float3 geometric = input.worldNormal * rsqrt(max(dot(input.worldNormal, input.worldNormal), 1e-12));
    if (dot(geometric, v) < 0.0)
        geometric = -geometric;
    float3 n = geometric;
    if ((flags & 2u) != 0u)
    {
        // Cotangent frame from screen derivatives (no stored tangents).
        float3 dp2perp = cross(dp2, geometric), dp1perp = cross(geometric, dp1);
        float3 t = dp2perp * duv1.x + dp1perp * duv2.x;
        float3 b = dp2perp * duv1.y + dp1perp * duv2.y;
        float scale = rsqrt(max(max(dot(t, t), dot(b, b)), 1e-30));
        float3 tangentNormal = relief.xyz * 2.0 - 1.0;
        n = normalize(t * (tangentNormal.x * scale) + b * (tangentNormal.y * scale) + geometric * tangentNormal.z);
    }
    float roughness = material.x, metallic = material.y;
    float alpha = roughness * roughness;
    float3 baseColor = texel.rgb;
    float3 f0 = lerp(float3(0.04, 0.04, 0.04) * material.z, baseColor, metallic);
    float nv = max(dot(n, v), 1e-4);
#ifdef PBR_HIGH
    float cavity = lerp(0.6, 1.0, (flags & 2u) != 0u ? relief.a : 1.0);
#else
    float cavity = 1.0;
#endif
    // Radial point light, as the original adds it to baked light.
    float radial = positionRadius.w > 0.0 ? max(0.0, 1.0 - distance(p, positionRadius.xyz) / positionRadius.w) : 0.0;
    float3 dynamicLight = lightColor.xyz * radial;
    float3 direct = float3(0.0, 0.0, 0.0);
    if (radial > 0.0)
        direct += dynamicLight * specular(n, v, normalize(positionRadius.xyz - p), f0, alpha);
    float3 diffuse;
    float bump = 1.0;
    if (model)
    {
        // Model light per pixel with the mapped normal, clamped like fixed-function light.
        float3 light = modelAmbient.rgb;
        uint count = min(uint(modelAmbient.w), 4u);
        for (uint i = 0u; i < count; ++i)
        {
            float3 l;
            float3 term = sourceTerm(modelSources[i * 3u], modelSources[i * 3u + 1u], modelSources[i * 3u + 2u].xyz, p, l);
            light += term * max(0.0, dot(n, l));
            direct += term * modelColor.w * specular(n, v, l, f0, alpha);
        }
        diffuse = modelColor.rgb * min(light, float3(1.0, 1.0, 1.0)) * modelColor.w * (1.0 - shadow) + dynamicLight;
    }
    else
    {
        bool lightmapped = input.lightmapCoord.x >= 0.0;
        float3 baked = lightmapped
                           ? lightmap.SampleLevel(lightmapSampler, max(input.lightmapCoord, float2(0.0, 0.0)), 0.0).rgb
                           : input.faceColor;
        diffuse = lightmapped ? modulation.y * min(baked * (1.0 - shadow) + dynamicLight, float3(1.0, 1.0, 1.0))
                              : baked * (1.0 - shadow) + dynamicLight;
        // SURF_BRIGHT and unlit faces carry no light information: original output.
        if (!lightmapped && all(input.faceColor >= 0.999))
#ifdef PBR_ALPHA
            return SHADED(float4(texel.rgb * diffuse * sceneColor.rgb, texel.a * modulation.x),
                          float4(0.0, 0.0, 0.0, texel.a * modulation.x));
#else
            return SHADED(float4(texel.rgb * diffuse * sceneColor.rgb, 1.0), float4(0.0, 0.0, 0.0, 1.0));
#endif
        float3 grid = (p - gridOrigin.xyz) * gridOrigin.w;
        int3 cell = int3(floor(grid));
        float3 sourceSpecular = float3(0.0, 0.0, 0.0);
        float predicted = 0.0, change = 0.0;
        if (all(cell >= int3(0, 0, 0)) && all(cell < int3(gridSize.xyz)))
        {
            uint packed = gridCells[(uint(cell.z) * gridSize.y + uint(cell.y)) * gridSize.x + uint(cell.x)];
            uint first = gridSize.w + (packed >> 4);
            uint count = min(packed & 15u, GRID_LIGHTS);
            for (uint i = 0u; i < count; ++i)
            {
                uint light = gridCells[first + i] * 3u;
                float3 l;
                float3 term = sourceTerm(gridLights[light], gridLights[light + 1u], gridLights[light + 2u].xyz, p, l);
                float flat = max(0.0, dot(geometric, l));
                // Lightmaps include N.L; the preprocessor's vertex light does not.
                predicted += luminance(term) * (lightmapped ? flat : 1.0);
                change += luminance(term) * (max(0.0, dot(n, l)) - flat);
                sourceSpecular += term * specular(n, v, l, f0, alpha);
            }
        }
        // Static lights are not occluded here: the baked light, which includes the
        // preprocessor's shadows, bounds how much of their predicted light reaches the face.
        float bakedLuminance = luminance(baked);
        float visibility = predicted > 1e-4 ? saturate(bakedLuminance / predicted) : 1.0;
        direct += sourceSpecular * (SATURATE * visibility * (1.0 - shadow));
        // Normal-map relief: the change of the lights' N.L, plus, for the baked light that no
        // listed light explains, a soft overhead light so relief stays readable in ambient areas.
        float unexplained = saturate(1.0 - predicted * visibility / max(bakedLuminance, 1e-4));
        bump = clamp(1.0 + change * visibility / max(bakedLuminance, 0.05) +
                         unexplained * 0.5 * (n.y - geometric.y),
                     0.25, 2.0);
    }
    // Baked light as a uniform environment: diffuse for dielectrics, split-sum reflection.
    // Multiple-scattering energy compensation (Fdez-Aguera): rough metals keep the light that
    // single-scattering GGX loses, so they do not darken against the original renderer.
    float3 split = environmentBrdf(f0, roughness, nv);
    float3 white = environmentBrdf(float3(1.0, 1.0, 1.0), roughness, nv);
    float3 compensation = 1.0 + f0 * (1.0 / max(white, 0.05) - 1.0);
    float3 body = (baseColor * diffuse * bump * (1.0 - metallic) + emitted(baseColor)) * sceneColor.rgb;
    float3 reflected = (diffuse * split + direct) * compensation * cavity * sceneColor.rgb;
#ifdef PBR_ALPHA
    // Highlights survive on nearly transparent glass: coverage rises to carry them.
    float coverage = texel.a * modulation.x;
    float outAlpha = saturate(max(coverage, luminance(reflected)));
    float3 straight = (body * coverage + reflected) / max(outAlpha, 1e-4);
    return SHADED(float4(straight, outAlpha),
                  float4(linearGlow(straight, emitted(baseColor) * sceneColor.rgb * coverage / max(outAlpha, 1e-4)) *
                             outAlpha,
                         outAlpha));
#else
    return SHADED(float4(body + reflected, 1.0),
                  float4(linearGlow(body + reflected, emitted(baseColor) * sceneColor.rgb), 1.0));
#endif
}
