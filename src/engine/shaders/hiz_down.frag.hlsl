// One level of the minimum-depth pyramid (Hi-Z) shared by reflections and ambient occlusion:
// each texel keeps the nearest linear depth of its 2x2 children in the previous level; on an
// odd previous size the last texel also takes the extra row or column, so every texel of
// level i covers level-0 texels [k * 2^i, (k + 1) * 2^i) and the border ones beyond. Written
// twice: a separate texture the next level reads, and this mip of the pyramid.
// SDL_GPU fragment conventions: textures/samplers in space2, uniform buffers in space3.
Texture2D<float4> previous : register(t0, space2);
SamplerState previousSampler : register(s0, space2); // Nearest, clamped.
cbuffer Level : register(b0, space3)
{
    float4 sizes; // xy: previous level size in texels; zw: this level's size.
};
struct Input
{
    float2 uv : TEXCOORD0;
    float4 position : SV_Position;
};
struct Output
{
    float4 level : SV_Target0;
    float4 pyramid : SV_Target1;
};
float tap(float2 texel)
{
    return previous.SampleLevel(previousSampler, (texel + 0.5) / sizes.xy, 0.0).r;
}
Output main(Input input)
{
    float2 texel = floor(input.position.xy);
    float2 base = texel * 2.0;
    float z = min(min(tap(base), tap(base + float2(1.0, 0.0))), min(tap(base + float2(0.0, 1.0)), tap(base + 1.0)));
    bool lastX = texel.x == sizes.z - 1.0 && fmod(sizes.x, 2.0) > 0.5;
    bool lastY = texel.y == sizes.w - 1.0 && fmod(sizes.y, 2.0) > 0.5;
    if (lastX)
        z = min(z, min(tap(base + float2(2.0, 0.0)), tap(base + float2(2.0, 1.0))));
    if (lastY)
        z = min(z, min(tap(base + float2(0.0, 2.0)), tap(base + float2(1.0, 2.0))));
    if (lastX && lastY)
        z = min(z, tap(base + 2.0));
    Output output;
    output.level = output.pyramid = float4(z, z, z, z);
    return output;
}
