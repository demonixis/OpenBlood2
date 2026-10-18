// Vertical model shadow tiles: the caster's world height at each texel. The depth test
// keeps the topmost surface; uncovered texels keep the cleared sentinel.
struct Input
{
    float3 faceColor : TEXCOORD0;
    float2 texCoord : TEXCOORD1;
    float3 worldPosition : TEXCOORD2;
};
float4 main(Input input) : SV_Target0
{
    return float4(input.worldPosition.y, 0.0, 0.0, 1.0);
}
