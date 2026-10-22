// Final composite of the enhanced renderer's HDR scene. The scene holds display-space values
// that may exceed 1 (specular, emission); they are decoded to linear light, ambient occlusion
// darkens the part that is not emission (weighted by the opaque share of the pixel), the bloom
// chain is added, and the result is tonemapped and encoded for the UNORM output.
// Tonemaps (settings.x): 0 clamps like the original framebuffer; 1 neutral: the highlight
// compression of Khronos PBR Neutral without its toe, so everything below 0.76 linear keeps its
// value; 2 filmic: Hill's fit of the ACES RRT/ODT, exposed so mid-grey keeps its value.
// SDL_GPU fragment conventions: textures/samplers in space2, uniform buffers in space3.
Texture2D<float4> scene : register(t0, space2);
SamplerState sceneSampler : register(s0, space2);
Texture2D<float4> bloom : register(t1, space2);
SamplerState bloomSampler : register(s1, space2);
Texture2D<float4> aux : register(t2, space2); // Glow (linear) and the occlusion mask, full size.
SamplerState auxSampler : register(s2, space2);
Texture2D<float4> occlusion : register(t3, space2);
SamplerState occlusionSampler : register(s3, space2);
cbuffer Composite : register(b0, space3)
{
    float4 settings;   // x: tonemap, y: bloom scale (0: none), z: filmic exposure, w: 1 with occlusion.
    float4 bloomTexel; // xy: bloom texel size.
};
struct Input
{
    float2 uv : TEXCOORD0;
};
float3 toLinear(float3 c)
{
    c = max(c, float3(0.0, 0.0, 0.0));
    return lerp(pow((c + 0.055) / 1.055, 2.4), c / 12.92, step(c, 0.04045));
}
float3 toDisplay(float3 c)
{
    c = saturate(c);
    return lerp(1.055 * pow(c, 1.0 / 2.4) - 0.055, c * 12.92, step(c, 0.0031308));
}
float3 neutral(float3 color)
{
    const float start = 0.76, desaturation = 0.15;
    float peak = max(color.r, max(color.g, color.b));
    if (peak < start)
        return color;
    const float d = 1.0 - start;
    float compressed = 1.0 - d * d / (peak + d - start);
    color *= compressed / peak;
    float g = 1.0 - 1.0 / (desaturation * (peak - compressed) + 1.0);
    return lerp(color, float3(compressed, compressed, compressed), g);
}
float3 filmic(float3 color)
{
    // sRGB to the RRT working space (with the RRT saturation), then back to sRGB.
    const float3x3 toWorking = {0.59719, 0.35458, 0.04823, 0.07600, 0.90834, 0.01566, 0.02840, 0.13383, 0.83777};
    const float3x3 fromWorking = {1.60475, -0.53108, -0.07367, -0.10208, 1.10813, -0.00605, -0.00327, -0.07276, 1.07602};
    float3 v = mul(toWorking, color * settings.z);
    v = (v * (v + 0.0245786) - 0.000090537) / (v * (0.983729 * v + 0.4329510) + 0.238081);
    return mul(fromWorking, v);
}
float3 bloomTap(float2 uv, float2 offset)
{
    return bloom.SampleLevel(bloomSampler, uv + offset * bloomTexel.xy, 0.0).rgb;
}
float4 main(Input input) : SV_Target0
{
    // Same size as the output, nearest filtering: each pixel reads its own scene texel.
    float3 color = toLinear(scene.SampleLevel(sceneSampler, input.uv, 0.0).rgb);
    if (settings.w > 0.5)
    {
        float4 glow = aux.SampleLevel(auxSampler, input.uv, 0.0);
        float ao = occlusion.SampleLevel(occlusionSampler, input.uv, 0.0).r;
        float3 lit = max(color - glow.rgb, float3(0.0, 0.0, 0.0));
        color += lit * (lerp(1.0, ao, saturate(glow.a)) - 1.0);
    }
    if (settings.y > 0.0)
    {
        float2 uv = input.uv;
        float3 sum = (bloomTap(uv, float2(-1.0, -1.0)) + bloomTap(uv, float2(1.0, -1.0)) +
                      bloomTap(uv, float2(-1.0, 1.0)) + bloomTap(uv, float2(1.0, 1.0))) +
                     (bloomTap(uv, float2(0.0, -1.0)) + bloomTap(uv, float2(-1.0, 0.0)) +
                      bloomTap(uv, float2(1.0, 0.0)) + bloomTap(uv, float2(0.0, 1.0))) *
                         2.0 +
                     bloomTap(uv, float2(0.0, 0.0)) * 4.0;
        color += sum * (settings.y / 16.0);
    }
    if (settings.x > 1.5)
        color = filmic(color);
    else if (settings.x > 0.5)
        color = neutral(color);
    return float4(toDisplay(color), 1.0);
}
