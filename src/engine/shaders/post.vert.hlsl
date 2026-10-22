// Post-processing passes: one triangle covering the target, without vertex buffers.
// uv (0, 0) is the top-left texel, as SDL_GPU texture coordinates.
struct Output
{
    float2 uv : TEXCOORD0;
    float4 position : SV_Position;
};
Output main(uint id : SV_VertexID)
{
    Output output;
    float2 uv = float2(float((id << 1u) & 2u), float(id & 2u));
    output.uv = uv;
    output.position = float4(uv.x * 2.0 - 1.0, 1.0 - uv.y * 2.0, 0.0, 1.0);
    return output;
}
