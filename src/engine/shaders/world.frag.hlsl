// Authored albedo times the face light (baked or model light) plus the radial point light.
// SDL_GPU fragment conventions: textures/samplers in space2, uniform buffers in space3.
Texture2D<float4> albedo : register(t0, space2);
SamplerState albedoSampler : register(s0, space2);
cbuffer Light : register(b0, space3)
{
    float4 modulation, positionRadius, lightColor;
};
struct Input
{
    float3 faceColor : TEXCOORD0;
    float2 texCoord : TEXCOORD1;
    float3 worldPosition : TEXCOORD2;
};
// Face light plus the bounded radial point light, added before the albedo multiply as
// the original renderer adds dynamic lights to baked light. No shadowing.
float3 illumination(float3 faceColor, float3 worldPosition)
{
    float radial =
        positionRadius.w > 0.0 ? max(0.0, 1.0 - distance(worldPosition, positionRadius.xyz) / positionRadius.w) : 0.0;
    return faceColor + lightColor.xyz * radial;
}
float4 main(Input input) : SV_Target0
{
    return float4(albedo.Sample(albedoSampler, input.texCoord).rgb * illumination(input.faceColor, input.worldPosition),
                  1.0);
}
