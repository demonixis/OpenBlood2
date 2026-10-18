// Authored albedo times the face light (baked or model light) plus the radial point light.
// SDL_GPU fragment conventions: textures/samplers in space2, uniform buffers in space3.
Texture2D<float4> albedo : register(t0, space2);
SamplerState albedoSampler : register(s0, space2);
Texture2D<float4> lightmap : register(t1, space2);
SamplerState lightmapSampler : register(s1, space2);
cbuffer Light : register(b0, space3)
{
    float4 modulation, positionRadius, lightColor;
};
struct Input
{
    float3 faceColor : TEXCOORD0;
    float2 texCoord : TEXCOORD1;
    float3 worldPosition : TEXCOORD2;
    float2 lightmapCoord : TEXCOORD3; // Negative: no lightmap on this face.
};
// Baked light plus the bounded radial point light, added before the albedo multiply as
// the original renderer adds dynamic lights to baked light. A lightmapped face reads its
// lightmap instead of the face color: the sum saturates like the original framebuffer, then
// scales by modulation.y (2 under the shipped "Saturate" default). No shadowing.
float3 illumination(Input input)
{
    float radial = positionRadius.w > 0.0
                       ? max(0.0, 1.0 - distance(input.worldPosition, positionRadius.xyz) / positionRadius.w)
                       : 0.0;
    float3 dynamicLight = lightColor.xyz * radial;
    float3 baked = lightmap.SampleLevel(lightmapSampler, max(input.lightmapCoord, float2(0.0, 0.0)), 0.0).rgb;
    return input.lightmapCoord.x >= 0.0 ? modulation.y * min(baked + dynamicLight, float3(1.0, 1.0, 1.0))
                                        : input.faceColor + dynamicLight;
}
float4 main(Input input) : SV_Target0
{
    return float4(albedo.Sample(albedoSampler, input.texCoord).rgb * illumination(input), 1.0);
}
