// DTX alpha multiplied by the original effect opacity. The original SDK explicitly
// disables FLAG_SPRITECHROMAKEY. SDL_GPU fragment conventions: space2 resources, space3 uniforms.
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
// Bounded radial brightening over the existing unlit base. No shadow/lightmap claim.
float3 illumination(float3 worldPosition)
{
    float radial =
        positionRadius.w > 0.0 ? max(0.0, 1.0 - distance(worldPosition, positionRadius.xyz) / positionRadius.w) : 0.0;
    return float3(1.0, 1.0, 1.0) + lightColor.xyz * radial;
}
float4 main(Input input) : SV_Target0
{
    float4 texel = albedo.Sample(albedoSampler, input.texCoord);
    if (texel.a == 0.0)
        discard;
    return float4(texel.rgb * input.faceColor * illumination(input.worldPosition), texel.a * modulation.x);
}
