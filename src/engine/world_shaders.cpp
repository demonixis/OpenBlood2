#include "engine/world_shaders.h"
#include "engine/shaders/skinned_vert.h"
#include "engine/shaders/skinned_vert_msl.h"
#include "engine/shaders/sprite_frag.h"
#include "engine/shaders/sprite_frag_msl.h"
#include "engine/shaders/world_frag.h"
#include "engine/shaders/world_frag_msl.h"
#include "engine/shaders/world_vert.h"
#include "engine/shaders/world_vert_msl.h"

namespace OpenBlood2
{
    bool world_shader_info(WorldShader shader, SDL_GPUShaderFormat available, SDL_GPUShaderCreateInfo& info)
    {
        info = {};
        const bool spirv = (available & SDL_GPU_SHADERFORMAT_SPIRV) != 0;
        if (!spirv && !(available & SDL_GPU_SHADERFORMAT_MSL))
            return false;
        info.format = spirv ? SDL_GPU_SHADERFORMAT_SPIRV : SDL_GPU_SHADERFORMAT_MSL;
        info.entrypoint = spirv ? "main" : "main0";
        switch (shader)
        {
            case WorldShader::Vertex:
                info.stage = SDL_GPU_SHADERSTAGE_VERTEX;
                info.code =
                    spirv ? reinterpret_cast<const Uint8*>(world_vert) : reinterpret_cast<const Uint8*>(world_vert_msl);
                info.code_size = spirv ? sizeof(world_vert) : sizeof(world_vert_msl) - 1;
                info.num_uniform_buffers = world_vert_uniforms;
                info.num_samplers = world_vert_samplers;
                info.num_storage_buffers = world_vert_storage_buffers;
                break;
            case WorldShader::SkinnedVertex:
                info.stage = SDL_GPU_SHADERSTAGE_VERTEX;
                info.code = spirv ? reinterpret_cast<const Uint8*>(skinned_vert)
                                  : reinterpret_cast<const Uint8*>(skinned_vert_msl);
                info.code_size = spirv ? sizeof(skinned_vert) : sizeof(skinned_vert_msl) - 1;
                info.num_uniform_buffers = skinned_vert_uniforms;
                info.num_samplers = skinned_vert_samplers;
                info.num_storage_buffers = skinned_vert_storage_buffers;
                break;
            case WorldShader::OpaqueFragment:
                info.stage = SDL_GPU_SHADERSTAGE_FRAGMENT;
                info.code =
                    spirv ? reinterpret_cast<const Uint8*>(world_frag) : reinterpret_cast<const Uint8*>(world_frag_msl);
                info.code_size = spirv ? sizeof(world_frag) : sizeof(world_frag_msl) - 1;
                info.num_uniform_buffers = world_frag_uniforms;
                info.num_samplers = world_frag_samplers;
                info.num_storage_buffers = world_frag_storage_buffers;
                break;
            case WorldShader::AlphaFragment:
                info.stage = SDL_GPU_SHADERSTAGE_FRAGMENT;
                info.code = spirv ? reinterpret_cast<const Uint8*>(sprite_frag)
                                  : reinterpret_cast<const Uint8*>(sprite_frag_msl);
                info.code_size = spirv ? sizeof(sprite_frag) : sizeof(sprite_frag_msl) - 1;
                info.num_uniform_buffers = sprite_frag_uniforms;
                info.num_samplers = sprite_frag_samplers;
                info.num_storage_buffers = sprite_frag_storage_buffers;
                break;
            default:
                info = {};
                return false;
        }
        return true;
    }
} // namespace OpenBlood2
