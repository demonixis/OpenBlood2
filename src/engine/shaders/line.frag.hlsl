// Unlit lines: the interpolated vertex colour.
struct Input
{
    float4 color : TEXCOORD0;
};
#ifdef AUX_TARGETS
struct Output
{
    float4 color : SV_Target0;
    float4 glow : SV_Target1;
    float4 depth : SV_Target2;
#ifdef REFLECTION_TARGETS
    float4 specular : SV_Target3;
    float4 environment : SV_Target4;
#endif
};
Output main(Input input)
{
    Output output;
    output.color = input.color;
    output.glow = float4(0.0, 0.0, 0.0, input.color.a);
    output.depth = 0.0; // Translucent pipeline masks depth/reflection writes.
#ifdef REFLECTION_TARGETS
    output.specular = 0.0;
    output.environment = 0.0;
#endif
    return output;
}
#else
float4 main(Input input) : SV_Target0
{
    return input.color;
}

#endif
