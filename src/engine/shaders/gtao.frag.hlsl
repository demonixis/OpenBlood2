// Ground-truth ambient occlusion (Jimenez, Wu, Pesce, Jarabo 2016, "Practical Realtime
// Strategies for Accurate Indirect Occlusion") at half size on the linear view depth: per
// pixel, a few screen-space slices rotated by interleaved gradient noise; on each side of a
// slice the highest horizon within the world-space radius (with a linear falloff) bounds the
// cosine-weighted visibility, integrated analytically around the normal projected into the
// slice. The normal is rebuilt from the nearer depth neighbours. Taps far from the texel read a
// coarser level of the minimum-depth pyramid, which keeps them in cache (XeGTAO's depth mips).
// View models and the background (depth 1e9) are not occluded and occlude nothing.
// SDL_GPU fragment conventions: textures/samplers in space2, uniform buffers in space3.
Texture2D<float4> depthPyramid : register(t0, space2); // Half-size min linear depth mips.
SamplerState depthSampler : register(s0, space2);     // Nearest, clamped, nearest mip.
cbuffer Gtao : register(b0, space3)
{
    float4 projection; // Scene x/y scales.
    float4 viewport;   // Scene viewport in full-size pixels: left, top, width, height.
    float4 size;       // xy: 1 / half width and height; z: largest screen radius (half-size pixels); w: levels.
    float4 settings;   // Radius (world units), slices, steps per side, final power.
};
struct Input
{
    float2 uv : TEXCOORD0;
    float4 position : SV_Position;
};
static const float PI = 3.14159265;
static const float FAR = 1e8;
// View-space position of a half-size texel (the top-left pixel of its full-size block).
float3 viewPosition(float2 texel, float z)
{
    float2 pixel = floor(texel) * 2.0 + 0.5;
    float2 ndc = float2((pixel.x - viewport.x) / viewport.z * 2.0 - 1.0, 1.0 - (pixel.y - viewport.y) / viewport.w * 2.0);
    return float3(ndc.x * z / projection.x, ndc.y * z / projection.y, z);
}
float3 positionAt(float2 texel, float level)
{
    return viewPosition(texel, depthPyramid.SampleLevel(depthSampler, texel * size.xy, level).r);
}
// Polynomial acos approximation (Eberly), as GTAO implementations use: max error about 1e-4.
float fastAcos(float x)
{
    float a = abs(x);
    float r = (-0.156583 * a + PI * 0.5) * sqrt(1.0 - a);
    return x >= 0.0 ? r : PI - r;
}
float noise(float2 pixel)
{
    return frac(52.9829189 * frac(dot(pixel, float2(0.06711056, 0.00583715))));
}
float4 main(Input input) : SV_Target0
{
    float2 texel = input.position.xy;
    float z = depthPyramid.SampleLevel(depthSampler, input.uv, 0.0).r;
    if (z >= FAR)
        return float4(1.0, 1.0, 1.0, 1.0);
    float3 p = viewPosition(texel, z);
    float3 left = positionAt(texel - float2(1.0, 0.0), 0.0), right = positionAt(texel + float2(1.0, 0.0), 0.0);
    float3 up = positionAt(texel - float2(0.0, 1.0), 0.0), down = positionAt(texel + float2(0.0, 1.0), 0.0);
    float3 dx = abs(right.z - p.z) < abs(p.z - left.z) ? right - p : p - left;
    float3 dy = abs(down.z - p.z) < abs(p.z - up.z) ? down - p : p - up;
    float3 v = normalize(-p);
    float3 n = cross(dx, dy);
    n *= rsqrt(max(dot(n, n), 1e-20));
    if (dot(n, v) < 0.0)
        n = -n;
    float radius = settings.x;
    // Half-size pixels covered by the radius, bounded to keep the taps near the texel.
    float screenRadius = min(radius * projection.x * 0.25 * viewport.z / p.z, size.z);
    if (screenRadius < 1.0)
        return float4(1.0, 1.0, 1.0, 1.0);
    uint slices = uint(settings.y), steps = uint(settings.z);
    float sliceNoise = noise(texel), stepNoise = noise(texel + float2(17.0, 31.0));
    float visibility = 0.0;
    for (uint slice = 0u; slice < slices; ++slice)
    {
        float phi = (float(slice) + sliceNoise) * PI / float(slices);
        float2 omega = float2(cos(phi), sin(phi));         // Pixels, y down.
        float3 direction = float3(omega.x, -omega.y, 0.0); // View space, y up.
        float3 ortho = direction - dot(direction, v) * v;
        float3 axis = normalize(cross(ortho, v));
        float3 projected = n - axis * dot(n, axis);
        float projectedLength = length(projected);
        float cosN = saturate(dot(projected, v) / max(projectedLength, 1e-6));
        float sign = dot(ortho, projected) < 0.0 ? -1.0 : 1.0;
        float angle = sign * fastAcos(cosN), sinN = sign * sqrt(1.0 - cosN * cosN);
        // Side 0 follows +omega (positive angles), side 1 the opposite way; their lowest horizons
        // are cos(angle +- pi/2).
        float low0 = -sinN, low1 = sinN;
        float horizon0 = low0, horizon1 = low1;
        for (uint step = 0u; step < steps; ++step)
        {
            float s = (float(step) + stepNoise) / float(steps);
            float reach = s * s * screenRadius + 1.0;
            float2 offset = omega * reach;
            float level = clamp(floor(log2(reach)) - 2.0, 0.0, size.w - 1.0);
            float3 delta0 = positionAt(texel + offset, level) - p, delta1 = positionAt(texel - offset, level) - p;
            float distance0 = length(delta0), distance1 = length(delta1);
            float weight0 = saturate((radius - distance0) / (0.6 * radius));
            float weight1 = saturate((radius - distance1) / (0.6 * radius));
            horizon0 = max(horizon0, lerp(low0, dot(delta0, v) / max(distance0, 1e-4), weight0));
            horizon1 = max(horizon1, lerp(low1, dot(delta1, v) / max(distance1, 1e-4), weight1));
        }
        float h0 = angle + min(fastAcos(clamp(horizon0, -1.0, 1.0)) - angle, PI * 0.5);
        float h1 = angle + max(-fastAcos(clamp(horizon1, -1.0, 1.0)) - angle, -PI * 0.5);
        visibility += projectedLength * 0.25 *
                      ((cosN + 2.0 * h0 * sinN - cos(2.0 * h0 - angle)) + (cosN + 2.0 * h1 * sinN - cos(2.0 * h1 - angle)));
    }
    float ao = pow(saturate(visibility / float(slices)), settings.w);
    return float4(ao, ao, ao, 1.0);
}
