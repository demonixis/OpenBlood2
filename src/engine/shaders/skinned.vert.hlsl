// Rigid ABC skinning: one node per vertex, optional CPU-sampled packed deformation.
// SDL_GPU vertex conventions: storage buffers in space0, uniform buffers in space1.
// Per node: rotation quaternion (xyzw), then translation (xyz) with w = 1 when hidden.
StructuredBuffer<float4> palette : register(t0, space0);
StructuredBuffer<float4> deformation : register(t1, space0);
cbuffer View : register(b0, space1)
{
    float4 eye, right, up, forward, projection, translation, quaternion, textureOffset, scale, color;
    uint4 bases; // x: palette base (float4 index), y: deformation base.
};
struct Input
{
    float3 position : TEXCOORD0;
    float2 uv : TEXCOORD1;
    uint4 nodes : TEXCOORD2; // x: vertex node slot; yzw: the face's three corner slots.
    uint deform : TEXCOORD3; // Deformation slot, or 0xffffffff for the authored position.
};
struct Output
{
    float3 faceColor : TEXCOORD0;
    float2 texCoord : TEXCOORD1;
    float3 worldPosition : TEXCOORD2;
    float2 lightmapCoord : TEXCOORD3; // Models have no lightmap.
    float4 position : SV_Position;
};
float3 rotate(float4 q, float3 v)
{
    return v + 2.0 * cross(q.xyz, cross(q.xyz, v) + q.w * v);
}
Output main(Input input)
{
    Output output;
    uint base = bases.x;
    float4 rotation = palette[base + input.nodes.x * 2u];
    float3 bone = palette[base + input.nodes.x * 2u + 1u].xyz;
    float3 local = input.deform == 0xffffffffu ? input.position : deformation[bases.y + input.deform].xyz;
    float3 model = (bone + rotate(rotation, local)) * scale.xyz;
    float3 world = rotate(quaternion, model) + translation.xyz;
    float3 relative = world - eye.xyz;
    float z = dot(relative, forward.xyz);
    output.position = float4(dot(relative, right.xyz) * projection.x, dot(relative, up.xyz) * projection.y,
                             z * projection.z + projection.w, z);
    // A face with any hidden corner node is omitted: every corner collapses identically.
    if (palette[base + input.nodes.y * 2u + 1u].w + palette[base + input.nodes.z * 2u + 1u].w +
            palette[base + input.nodes.w * 2u + 1u].w >
        0.0)
        output.position = float4(0.0, 0.0, 0.0, 0.0);
    output.worldPosition = world;
    output.faceColor = color.xyz;
    output.texCoord = input.uv + textureOffset.xy;
    output.lightmapCoord = float2(-1.0, -1.0);
    return output;
}
