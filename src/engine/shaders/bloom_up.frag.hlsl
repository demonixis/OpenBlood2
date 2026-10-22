// Bloom upsample: a 3x3 tent of the coarser level, added (blend ONE, ONE) to the finer level.
// SDL_GPU fragment conventions: textures/samplers in space2, uniform buffers in space3.
Texture2D<float4> source : register(t0, space2);
SamplerState sourceSampler : register(s0, space2);
cbuffer Pass : register(b0, space3)
{
    float4 texel; // xy: source texel size times the filter radius.
};
struct Input
{
    float2 uv : TEXCOORD0;
};
float3 tap(float2 uv, float2 offset)
{
    return source.SampleLevel(sourceSampler, uv + offset * texel.xy, 0.0).rgb;
}
float4 main(Input input) : SV_Target0
{
    float2 uv = input.uv;
    float3 sum = (tap(uv, float2(-1.0, -1.0)) + tap(uv, float2(1.0, -1.0)) + tap(uv, float2(-1.0, 1.0)) +
                  tap(uv, float2(1.0, 1.0))) +
                 (tap(uv, float2(0.0, -1.0)) + tap(uv, float2(-1.0, 0.0)) + tap(uv, float2(1.0, 0.0)) +
                  tap(uv, float2(0.0, 1.0))) *
                     2.0 +
                 tap(uv, float2(0.0, 0.0)) * 4.0;
    return float4(sum / 16.0, 0.0);
}
