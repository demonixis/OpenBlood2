// Unlit lines (XR floor grid, controller boxes, selection laser): object scale, rotation and
// translation, then an asymmetric projection. SDL_GPU vertex conventions: uniform buffers in space1.
cbuffer View : register(b0, space1)
{
    float4 eye, right, up, forward, projection, projectionOffset, translation, quaternion, scale;
};
struct Input
{
    float3 position : TEXCOORD0;
    float4 color : TEXCOORD1;
};
struct Output
{
    float4 color : TEXCOORD0;
    float4 position : SV_Position;
};
Output main(Input input)
{
    Output output;
    float3 local = input.position * scale.xyz;
    float3 q = quaternion.xyz;
    float3 world = local + 2.0 * cross(q, cross(q, local) + quaternion.w * local) + translation.xyz;
    float3 relative = world - eye.xyz;
    float z = dot(relative, forward.xyz);
    output.position = float4(dot(relative, right.xyz) * projection.x + z * projectionOffset.x,
                             dot(relative, up.xyz) * projection.y + z * projectionOffset.y,
                             z * projection.z + projection.w, z);
    output.color = input.color;
    return output;
}
