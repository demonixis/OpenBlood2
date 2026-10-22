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
        // Bloom scene passes (post-processing): the same shading plus the glow target 1.
        OpaqueGlowFragment,
        AlphaGlowFragment,
        PbrGlowFragment,
        PbrHighGlowFragment,
        PbrAlphaGlowFragment,
        PbrHighAlphaGlowFragment,
        // Post-processing: a buffer-free full-target triangle, the bloom chain, the HDR composite
        // and FXAA.
        PostVertex,
        BloomDownFragment,
        BloomUpFragment,
        CompositeFragment,
        FxaaFragment
    };
    // Static owned bytecode/source and reflected resource counts. No device or
    // filesystem needed; MSL's generated entry point differs from SPIR-V's.
    bool world_shader_info(WorldShader, SDL_GPUShaderFormat available, SDL_GPUShaderCreateInfo&);
} // namespace OpenBlood2
