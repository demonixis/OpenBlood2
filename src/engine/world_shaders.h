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
        SkinnedVertex // Rigid ABC node palette and packed deformation from storage buffers.
    };
    // Static owned bytecode/source and reflected resource counts. No device or
    // filesystem needed; MSL's generated entry point differs from SPIR-V's.
    bool world_shader_info(WorldShader, SDL_GPUShaderFormat available, SDL_GPUShaderCreateInfo&);
} // namespace OpenBlood2
