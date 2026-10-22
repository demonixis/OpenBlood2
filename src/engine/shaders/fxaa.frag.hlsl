// Fast approximate anti-aliasing (Lottes 2011, the FXAA 3.11 quality algorithm) on the final
// display-space image: local luma contrast finds an edge and its orientation, the edge is
// followed in both directions to its ends, and the pixel is re-sampled across the edge by its
// distance to the nearer end, or by the subpixel blend when that is larger.
// SDL_GPU fragment conventions: textures/samplers in space2, uniform buffers in space3.
Texture2D<float4> source : register(t0, space2);
SamplerState sourceSampler : register(s0, space2);
cbuffer Pass : register(b0, space3)
{
    float4 texel; // xy: 1 / target size.
};
struct Input
{
    float2 uv : TEXCOORD0;
};
static const float EDGE_THRESHOLD = 0.125, EDGE_THRESHOLD_MIN = 0.0312, SUBPIXEL = 0.75;
static const uint STEPS = 10u;
static const float STEP_SIZE[10] = {1.0, 1.5, 2.0, 2.0, 2.0, 2.0, 2.0, 2.0, 4.0, 8.0};
float luma(float3 c)
{
    return dot(c, float3(0.299, 0.587, 0.114));
}
float lumaAt(float2 uv)
{
    return luma(source.SampleLevel(sourceSampler, uv, 0.0).rgb);
}
float4 main(Input input) : SV_Target0
{
    float2 uv = input.uv, t = texel.xy;
    float4 center = source.SampleLevel(sourceSampler, uv, 0.0);
    float m = luma(center.rgb);
    float n = lumaAt(uv + float2(0.0, -t.y)), s = lumaAt(uv + float2(0.0, t.y));
    float w = lumaAt(uv + float2(-t.x, 0.0)), e = lumaAt(uv + float2(t.x, 0.0));
    float highest = max(m, max(max(n, s), max(w, e))), lowest = min(m, min(min(n, s), min(w, e)));
    float range = highest - lowest;
    if (range < max(EDGE_THRESHOLD_MIN, highest * EDGE_THRESHOLD))
        return center;
    float nw = lumaAt(uv + float2(-t.x, -t.y)), ne = lumaAt(uv + float2(t.x, -t.y));
    float sw = lumaAt(uv + float2(-t.x, t.y)), se = lumaAt(uv + float2(t.x, t.y));
    // A horizontal edge changes vertically: compare the vertical and horizontal second differences.
    float vertical = 2.0 * abs(n + s - 2.0 * m) + abs(nw + sw - 2.0 * w) + abs(ne + se - 2.0 * e);
    float horizontal = 2.0 * abs(w + e - 2.0 * m) + abs(nw + ne - 2.0 * n) + abs(sw + se - 2.0 * s);
    bool alongX = vertical >= horizontal;
    float before = alongX ? n : w, after = alongX ? s : e;
    float gradientBefore = abs(before - m), gradientAfter = abs(after - m);
    bool steeperBefore = gradientBefore >= gradientAfter;
    float gradient = 0.25 * max(gradientBefore, gradientAfter);
    float across = alongX ? t.y : t.x;
    float average = 0.5 * ((steeperBefore ? before : after) + m);
    if (steeperBefore)
        across = -across;
    // Walk along the edge, half a pixel toward the steeper side.
    float2 edge = uv + (alongX ? float2(0.0, across * 0.5) : float2(across * 0.5, 0.0));
    float2 along = alongX ? float2(t.x, 0.0) : float2(0.0, t.y);
    float2 uv1 = edge - along * STEP_SIZE[0], uv2 = edge + along * STEP_SIZE[0];
    float end1 = lumaAt(uv1) - average, end2 = lumaAt(uv2) - average;
    bool reached1 = abs(end1) >= gradient, reached2 = abs(end2) >= gradient;
    [loop] for (uint i = 1u; i < STEPS && !(reached1 && reached2); ++i)
    {
        if (!reached1)
        {
            uv1 -= along * STEP_SIZE[i];
            end1 = lumaAt(uv1) - average;
            reached1 = abs(end1) >= gradient;
        }
        if (!reached2)
        {
            uv2 += along * STEP_SIZE[i];
            end2 = lumaAt(uv2) - average;
            reached2 = abs(end2) >= gradient;
        }
    }
    float distance1 = alongX ? uv.x - uv1.x : uv.y - uv1.y;
    float distance2 = alongX ? uv2.x - uv.x : uv2.y - uv.y;
    bool nearer1 = distance1 < distance2;
    float offset = 0.5 - min(distance1, distance2) / (distance1 + distance2);
    // Only blend when the luma at the nearer end varies the opposite way from the centre.
    if (((nearer1 ? end1 : end2) < 0.0) == (m < average))
        offset = 0.0;
    float neighbours = (2.0 * (n + s + w + e) + nw + ne + sw + se) / 12.0;
    float subpixel = saturate(abs(neighbours - m) / range);
    subpixel = (-2.0 * subpixel + 3.0) * subpixel * subpixel;
    offset = max(offset, subpixel * subpixel * SUBPIXEL);
    float2 blended = uv + (alongX ? float2(0.0, offset * across) : float2(offset * across, 0.0));
    return float4(source.SampleLevel(sourceSampler, blended, 0.0).rgb, 1.0);
}
