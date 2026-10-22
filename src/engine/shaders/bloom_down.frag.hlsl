// Bloom downsample (Jimenez 2014, "Next Generation Post Processing in Call of Duty: Advanced
// Warfare"): 13 bilinear taps of the finer level around the centre of each coarser texel.
// The first pass (texel.z = 1) reads the scene's linear glow target and weights its five 2x2
// boxes by 1 / (1 + luma) (Karis average), so isolated bright texels do not flicker.
// SDL_GPU fragment conventions: textures/samplers in space2, uniform buffers in space3.
Texture2D<float4> source : register(t0, space2);
SamplerState sourceSampler : register(s0, space2);
cbuffer Pass : register(b0, space3)
{
    float4 texel; // xy: source texel size; z: 1 on the first pass.
};
struct Input
{
    float2 uv : TEXCOORD0;
};
float3 tap(float2 uv, float2 offset)
{
    return source.SampleLevel(sourceSampler, uv + offset * texel.xy, 0.0).rgb;
}
float3 weighted(float3 a, float3 b, float3 c, float3 d, float weight, inout float total)
{
    float3 box = (a + b + c + d) * 0.25;
    float w = weight / (1.0 + dot(box, float3(0.2126, 0.7152, 0.0722)));
    total += w;
    return box * w;
}
float4 main(Input input) : SV_Target0
{
    float2 uv = input.uv;
    float3 a = tap(uv, float2(-2.0, -2.0)), b = tap(uv, float2(0.0, -2.0)), c = tap(uv, float2(2.0, -2.0));
    float3 d = tap(uv, float2(-2.0, 0.0)), e = tap(uv, float2(0.0, 0.0)), f = tap(uv, float2(2.0, 0.0));
    float3 g = tap(uv, float2(-2.0, 2.0)), h = tap(uv, float2(0.0, 2.0)), i = tap(uv, float2(2.0, 2.0));
    float3 j = tap(uv, float2(-1.0, -1.0)), k = tap(uv, float2(1.0, -1.0));
    float3 l = tap(uv, float2(-1.0, 1.0)), m = tap(uv, float2(1.0, 1.0));
    if (texel.z > 0.5)
    {
        float total = 0.0;
        float3 sum = weighted(j, k, l, m, 0.5, total) + weighted(a, b, d, e, 0.125, total) +
                     weighted(b, c, e, f, 0.125, total) + weighted(d, e, g, h, 0.125, total) +
                     weighted(e, f, h, i, 0.125, total);
        return float4(sum / total, 1.0);
    }
    return float4(e * 0.125 + (a + c + g + i) * 0.03125 + (b + d + f + h) * 0.0625 + (j + k + l + m) * 0.125, 1.0);
}
