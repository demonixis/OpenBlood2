// Screen-space reflections, half size: the view ray reflected about the normal rebuilt from
// the linear depth is marched in screen space (perspective-correct depth, jittered steps) until
// it passes behind the depth within a thickness, refined by bisection, and the HDR scene is
// read at the hit. Output: reflected light premultiplied by its confidence, which fades with
// roughness, screen edges, the travelled share of the ray and rays turning back to the camera.
// Pixels without specular weight, view models and the background reflect nothing.
// SDL_GPU fragment conventions: textures/samplers in space2, uniform buffers in space3.
Texture2D<float4> viewDepth : register(t0, space2); // Half-size linear depth (1e9: none).
SamplerState depthSampler : register(s0, space2);  // Nearest, clamped.
Texture2D<float4> specular : register(t1, space2);  // Full size: weight, roughness.
SamplerState specularSampler : register(s1, space2);
Texture2D<float4> scene : register(t2, space2); // Full-size HDR scene, display space.
SamplerState sceneSampler : register(s2, space2); // Bilinear, clamped.
cbuffer Ssr : register(b0, space3)
{
    float4 projection; // Scene x/y scales.
    float4 viewport;   // Scene viewport in full-size pixels: left, top, width, height.
    float4 size;       // xy: 1 / half width and height; zw: 1 / full width and height.
    float4 settings;   // Ray length (world units), steps, thickness (world units), roughness cutoff.
};
struct Input
{
    float2 uv : TEXCOORD0;
    float4 position : SV_Position;
};
static const float FAR = 1e8;
float3 viewPosition(float2 texel, float z)
{
    float2 pixel = floor(texel) * 2.0 + 0.5;
    float2 ndc = float2((pixel.x - viewport.x) / viewport.z * 2.0 - 1.0, 1.0 - (pixel.y - viewport.y) / viewport.w * 2.0);
    return float3(ndc.x * z / projection.x, ndc.y * z / projection.y, z);
}
float3 positionAt(float2 texel)
{
    return viewPosition(texel, viewDepth.SampleLevel(depthSampler, texel * size.xy, 0.0).r);
}
// View-space point to continuous half-size texel coordinates.
float2 project(float3 p)
{
    float2 ndc = float2(p.x * projection.x, p.y * projection.y) / p.z;
    return float2(viewport.x + (ndc.x + 1.0) * 0.5 * viewport.z, viewport.y + (1.0 - ndc.y) * 0.5 * viewport.w) * 0.5;
}
float noise(float2 pixel)
{
    return frac(52.9829189 * frac(dot(pixel, float2(0.06711056, 0.00583715))));
}
float4 main(Input input) : SV_Target0
{
    float2 texel = input.position.xy;
    float z = viewDepth.SampleLevel(depthSampler, input.uv, 0.0).r;
    float4 weight = specular.SampleLevel(specularSampler, (floor(texel) * 2.0 + 0.5) * size.zw, 0.0);
    float cutoff = settings.w;
    if (z >= FAR || dot(weight.rgb, float3(1.0, 1.0, 1.0)) <= 0.0 || weight.a >= cutoff)
        return float4(0.0, 0.0, 0.0, 0.0);
    float3 p = viewPosition(texel, z);
    float3 left = positionAt(texel - float2(1.0, 0.0)), right = positionAt(texel + float2(1.0, 0.0));
    float3 up = positionAt(texel - float2(0.0, 1.0)), down = positionAt(texel + float2(0.0, 1.0));
    float3 dx = abs(right.z - p.z) < abs(p.z - left.z) ? right - p : p - left;
    float3 dy = abs(down.z - p.z) < abs(p.z - up.z) ? down - p : p - up;
    float3 v = normalize(-p);
    float3 n = cross(dx, dy);
    n *= rsqrt(max(dot(n, n), 1e-20));
    if (dot(n, v) < 0.0)
        n = -n;
    float3 r = reflect(-v, n);
    // Rays turning back to the camera rarely meet anything visible.
    float facing = saturate(1.0 + r.z * 2.5);
    if (facing <= 0.0)
        return float4(0.0, 0.0, 0.0, 0.0);
    float rayLength = settings.x;
    if (r.z < 0.0)
        rayLength = min(rayLength, (p.z - 2.0) / -r.z); // Stay in front of the near plane.
    float3 end = p + r * rayLength;
    float2 s0 = project(p), s1 = project(end);
    float2 d = s1 - s0;
    // Clip the segment to the scene viewport (half-size texels).
    float2 low = viewport.xy * 0.5, high = (viewport.xy + viewport.zw) * 0.5;
    float tMax = 1.0;
    if (abs(d.x) > 1e-6)
        tMax = min(tMax, ((d.x > 0.0 ? high.x : low.x) - s0.x) / d.x);
    if (abs(d.y) > 1e-6)
        tMax = min(tMax, ((d.y > 0.0 ? high.y : low.y) - s0.y) / d.y);
    float span = length(d) * tMax; // Texels.
    if (tMax <= 0.0 || span < 2.0)
        return float4(0.0, 0.0, 0.0, 0.0);
    float inverse0 = 1.0 / p.z, inverse1 = 1.0 / end.z;
    uint steps = uint(settings.y);
    float jitter = noise(texel);
    float previous = 1.5 / span * tMax, hit = -1.0;
    for (uint i = 0u; i < steps; ++i)
    {
        float t = max((float(i) + jitter) / float(steps), 1.5 / span) * tMax;
        float rayZ = 1.0 / lerp(inverse0, inverse1, t);
        float sceneZ = viewDepth.SampleLevel(depthSampler, (s0 + d * t) * size.xy, 0.0).r;
        float behind = rayZ - sceneZ;
        if (sceneZ < FAR && behind > 0.0 && behind < settings.z + 0.02 * rayZ)
        {
            // Bisection between the last point in front and this one.
            float a = previous, b = t;
            for (int k = 0; k < 4; ++k)
            {
                float m = 0.5 * (a + b);
                float mz = 1.0 / lerp(inverse0, inverse1, m);
                if (mz > viewDepth.SampleLevel(depthSampler, (s0 + d * m) * size.xy, 0.0).r)
                    b = m;
                else
                    a = m;
            }
            hit = b;
            break;
        }
        previous = t;
    }
    if (hit < 0.0)
        return float4(0.0, 0.0, 0.0, 0.0);
    float2 hitTexel = s0 + d * hit;
    float2 ndc = (hitTexel * 2.0 - viewport.xy) / viewport.zw * 2.0 - 1.0;
    float edge = 1.0 - smoothstep(0.8, 1.0, max(abs(ndc.x), abs(ndc.y)));
    float travelled = 1.0 - smoothstep(0.6, 1.0, hit);
    float glossy = 1.0 - smoothstep(cutoff * 0.6, cutoff, weight.a);
    float confidence = edge * travelled * glossy * facing;
    float3 light = scene.SampleLevel(sceneSampler, hitTexel * size.xy, 0.0).rgb;
    return float4(light * confidence, confidence);
}
