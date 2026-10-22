// Separable depth-aware blur of the half-size ambient occlusion: nine taps along `direction`
// (one texel per tap), Gaussian weights cut by the relative view-depth difference, so the noise
// of the slice rotation averages out without bleeding across depth edges. The composite then
// upsamples the result with the full-size depth.
// SDL_GPU fragment conventions: textures/samplers in space2, uniform buffers in space3.
Texture2D<float4> occlusion : register(t0, space2);
SamplerState occlusionSampler : register(s0, space2); // Nearest, clamped.
Texture2D<float4> viewDepth : register(t1, space2);
SamplerState viewDepthSampler : register(s1, space2); // Nearest, clamped.
cbuffer Blur : register(b0, space3)
{
    float4 direction; // xy: one texel along the blur axis (uv units).
};
struct Input
{
    float2 uv : TEXCOORD0;
};
static const float FAR = 1e8;
float4 main(Input input) : SV_Target0
{
    float center = viewDepth.SampleLevel(viewDepthSampler, input.uv, 0.0).r;
    if (center >= FAR)
        return float4(1.0, 1.0, 1.0, 1.0);
    static const float gaussian[5] = {1.0, 0.92, 0.72, 0.49, 0.29};
    float sum = 0.0, total = 0.0;
    for (int i = -4; i <= 4; ++i)
    {
        float2 uv = input.uv + direction.xy * float(i);
        float z = viewDepth.SampleLevel(viewDepthSampler, uv, 0.0).r;
        float weight = gaussian[abs(i)] * saturate(1.0 - abs(z - center) / (0.04 * center));
        sum += occlusion.SampleLevel(occlusionSampler, uv, 0.0).r * weight;
        total += weight;
    }
    float ao = sum / total; // The centre tap always weighs 1.
    return float4(ao, ao, ao, 1.0);
}
