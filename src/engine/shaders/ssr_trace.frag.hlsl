// Screen-space reflections, half size: the view ray reflected about the normal rebuilt from
// the linear depth is traced in screen space through the minimum-depth pyramid (Hi-Z): a cell
// whose nearest surface lies behind the whole ray segment crossing it is skipped and the next
// cell is tested one level coarser; otherwise the trace descends, down to a level-0 texel where
// a ray behind the surface by less than a thickness is a hit. Depth along the ray is
// interpolated perspective-correctly (1/z). The HDR scene is read at the hit. Output: reflected light premultiplied by its confidence, which fades with
// roughness, screen edges, the travelled share of the ray and rays turning back to the camera.
// Pixels without specular weight, view models and the background reflect nothing.
// SDL_GPU fragment conventions: textures/samplers in space2, uniform buffers in space3.
Texture2D<float4> depthPyramid : register(t0, space2); // Half-size min linear depth mips (1e9: none).
SamplerState depthSampler : register(s0, space2);     // Nearest, clamped, nearest mip.
Texture2D<float4> specular : register(t1, space2);  // Full size: weight, roughness.
SamplerState specularSampler : register(s1, space2);
Texture2D<float4> scene : register(t2, space2); // Full-size HDR scene, display space.
SamplerState sceneSampler : register(s2, space2); // Bilinear, clamped.
cbuffer Ssr : register(b0, space3)
{
    float4 projection; // Scene x/y scales.
    float4 viewport;   // Scene viewport in full-size pixels: left, top, width, height.
    float4 size;       // xy: 1 / half width and height; zw: half width and height (level 0).
    float4 settings;   // Ray length (world units), iterations, thickness (world units), roughness cutoff.
    float4 extra;      // xy: 1 / full width and height; z: pyramid levels.
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
    return viewPosition(texel, depthPyramid.SampleLevel(depthSampler, texel * size.xy, 0.0).r);
}
// Nearest depth of a pyramid cell (level-0 texels [cell * 2^level, (cell + 1) * 2^level)).
float cellDepth(float2 cell, float level)
{
    float2 levelSize = max(floor(size.zw / exp2(level)), 1.0);
    return depthPyramid.SampleLevel(depthSampler, (cell + 0.5) / levelSize, level).r;
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
    float z = depthPyramid.SampleLevel(depthSampler, input.uv, 0.0).r;
    float4 weight = specular.SampleLevel(specularSampler, (floor(texel) * 2.0 + 0.5) * extra.xy, 0.0);
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
    float thickness = settings.z;
    float maxLevel = extra.z - 1.0, level = 0.0;
    float2 safe = float2(abs(d.x) > 1e-6 ? d.x : 1e-6, abs(d.y) > 1e-6 ? d.y : 1e-6);
    float nudge = 0.01 / length(d); // Past a cell boundary, in segment units.
    float t = 1.5 / length(d), hit = -1.0;
    uint iterations = uint(settings.y);
    for (uint i = 0u; i < iterations && t < tMax; ++i)
    {
        float scale = exp2(level);
        float2 cell = floor((s0 + d * t) / scale);
        float2 boundary = (cell + step(0.0, safe)) * scale;
        float2 crossing = (boundary - s0) / safe;
        float exit = min(min(crossing.x, crossing.y), tMax) + nudge;
        float z0 = 1.0 / lerp(inverse0, inverse1, t), z1 = 1.0 / lerp(inverse0, inverse1, min(exit, tMax));
        float surface = cellDepth(cell, level);
        if (max(z0, z1) < surface)
        {
            // The whole segment stays in front of the cell's nearest surface: skip it, look wider.
            t = exit;
            level = min(level + 1.0, maxLevel);
        }
        else if (level > 0.0)
            level -= 1.0;
        else if (min(z0, z1) < surface + thickness + 0.02 * surface)
        {
            hit = t; // The segment reaches the surface within the thickness.
            break;
        }
        else
            t = exit; // Entirely behind a thick surface: continue texel by texel.
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
