// World, model and FX geometry: object rotation/translation, then the diagnostic projection.
// SDL_GPU vertex conventions: uniform buffers in space1.
cbuffer View : register(b0, space1)
{
    float4 eye, right, up, forward, projection, translation, quaternion, textureOffset;
};
struct Input
{
    float3 position : TEXCOORD0;
    float3 color : TEXCOORD1;
    float2 uv : TEXCOORD2;
    float2 lightmap : TEXCOORD3;
};
struct Output
{
    float3 faceColor : TEXCOORD0;
    float2 texCoord : TEXCOORD1;
    float3 worldPosition : TEXCOORD2;
    float2 lightmapCoord : TEXCOORD3;
    float4 position : SV_Position;
};
Output main(Input input)
{
    Output output;
    float3 q = quaternion.xyz;
    float3 world =
        input.position + 2.0 * cross(q, cross(q, input.position) + quaternion.w * input.position) + translation.xyz;
    float3 relative = world - eye.xyz;
    float z = dot(relative, forward.xyz);
    output.position = float4(dot(relative, right.xyz) * projection.x, dot(relative, up.xyz) * projection.y,
                             z * projection.z + projection.w, z);
    output.worldPosition = world;
    output.faceColor = input.color;
    output.texCoord = input.uv + textureOffset.xy;
    output.lightmapCoord = input.lightmap;
    return output;
}
