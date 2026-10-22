#pragma once
#include <SDL3/SDL_gpu.h>

namespace OpenBlood2
{
    // Only advertise formats actually bundled with the world/model/FX renderer.
    inline constexpr SDL_GPUShaderFormat WorldShaderFormats = SDL_GPU_SHADERFORMAT_SPIRV | SDL_GPU_SHADERFORMAT_MSL;
    enum class WorldShader
    {
        Vertex,
        OpaqueFragment,
        AlphaFragment,
        SkinnedVertex, // Rigid ABC node palette and packed deformation from storage buffers.
        ShadowFragment, // Caster height for vertical model shadow tiles.
        // Enhanced renderer (physically based): standard/high quality, opaque/translucent.
        PbrFragment,
        PbrHighFragment,
        PbrAlphaFragment,
        PbrHighAlphaFragment,
        // Unlit lines with an asymmetric projection (XR grid, controller boxes, laser).
        LineVertex,
        LineFragment,
        // Post-processing scene passes: the same shading plus target 1 (glow, occlusion mask) and
        // target 2 (window depth copy).
        OpaqueAuxFragment,
        AlphaAuxFragment,
        PbrAuxFragment,
        PbrHighAuxFragment,
        PbrAlphaAuxFragment,
        PbrHighAlphaAuxFragment,
        // With screen-space reflections: also target 3 (environment specular weight, roughness)
        // and target 4 (the environment light it multiplied).
        OpaqueAuxSsrFragment,
        AlphaAuxSsrFragment,
        PbrAuxSsrFragment,
        PbrHighAuxSsrFragment,
        PbrAlphaAuxSsrFragment,
        PbrHighAlphaAuxSsrFragment,
        // Post-processing: a buffer-free full-target triangle, the bloom chain, the HDR composite,
        // FXAA, ground-truth ambient occlusion and its depth-aware blur.
        PostVertex,
        BloomDownFragment,
        BloomUpFragment,
        CompositeFragment,
        FxaaFragment,
        AoDepthFragment,
        GtaoFragment,
        AoBlurFragment,
        SsrTraceFragment,
        SsrBlurFragment
    };
    // Static owned bytecode/source and reflected resource counts. No device or
    // filesystem needed; MSL's generated entry point differs from SPIR-V's.
    bool world_shader_info(WorldShader, SDL_GPUShaderFormat available, SDL_GPUShaderCreateInfo&);
} // namespace OpenBlood2
