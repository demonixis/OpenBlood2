// Half-size linear view depth for ambient occlusion and reflections: each texel takes the
// top-left pixel of its 2x2 block from the full-size window depth copy (no mixing across
// edges). The background and view models (the reserved near depth slice) become 1e9, beyond any
// occlusion radius or reflection ray. Written twice: the half-size buffer and level 0 of the
// depth pyramid.
// SDL_GPU fragment conventions: textures/samplers in space2, uniform buffers in space3.
Texture2D<float4> depthCopy : register(t0, space2);
SamplerState depthSampler : register(s0, space2); // Nearest, clamped.
cbuffer Depth : register(b0, space3)
{
    float4 projection; // z/w: scene depth terms (z_ndc = z + w / view depth).
    float4 depthRange; // x/y: 1 / full width and height, z: scene slice start, w: 1 / slice span.
};
struct Input
{
    float2 uv : TEXCOORD0;
    float4 position : SV_Position;
};
struct Output
{
    float4 depth : SV_Target0;
    float4 pyramid : SV_Target1;
};
Output main(Input input)
{
    float2 pixel = floor(input.position.xy) * 2.0 + 0.5;
    float depth = depthCopy.SampleLevel(depthSampler, pixel * depthRange.xy, 0.0).r;
    float z = depth < depthRange.z || depth >= 1.0
                  ? 1e9
                  : projection.w / ((depth - depthRange.z) * depthRange.w - projection.z);
    Output output;
    output.depth = output.pyramid = float4(z, z, z, z);
    return output;
}
