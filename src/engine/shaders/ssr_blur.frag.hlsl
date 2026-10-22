// Separable blur of the half-size reflections (premultiplied by confidence): seven taps along
// `direction`, spread with the surface roughness so glossy surfaces blur their reflection, and
// cut by the relative view-depth difference so reflections stay on their surface.
// SDL_GPU fragment conventions: textures/samplers in space2, uniform buffers in space3.
Texture2D<float4> reflection : register(t0, space2);
SamplerState reflectionSampler : register(s0, space2); // Nearest, clamped.
Texture2D<float4> viewDepth : register(t1, space2);
SamplerState depthSampler : register(s1, space2); // Nearest, clamped.
Texture2D<float4> specular : register(t2, space2); // Full size: weight, roughness.
SamplerState specularSampler : register(s2, space2);
cbuffer Blur : register(b0, space3)
{
    float4 direction; // xy: one texel along the blur axis (uv units); z: radius at the cutoff (texels); w: cutoff.
    float4 fullSize;  // xy: 1 / full width and height.
};
struct Input
{
    float2 uv : TEXCOORD0;
    float4 position : SV_Position;
};
static const float FAR = 1e8;
float4 main(Input input) : SV_Target0
{
    float4 center = reflection.SampleLevel(reflectionSampler, input.uv, 0.0);
    float z = viewDepth.SampleLevel(depthSampler, input.uv, 0.0).r;
    float2 pixel = floor(input.position.xy) * 2.0 + 0.5; // Top-left full-size pixel of this texel.
    float radius = saturate(specular.SampleLevel(specularSampler, pixel * fullSize.xy, 0.0).a / direction.w) * direction.z;
    if (z >= FAR || radius < 0.5)
        return center;
    static const float gaussian[4] = {1.0, 0.8, 0.45, 0.2};
    float4 sum = center;
    float total = 1.0;
    for (int i = -3; i <= 3; ++i)
    {
        if (i == 0)
            continue;
        float2 uv = input.uv + direction.xy * (float(i) * radius / 3.0);
        float tapZ = viewDepth.SampleLevel(depthSampler, uv, 0.0).r;
        float weight = gaussian[abs(i)] * saturate(1.0 - abs(tapZ - z) / (0.04 * z));
        sum += reflection.SampleLevel(reflectionSampler, uv, 0.0) * weight;
        total += weight;
    }
    return sum / total;
}
