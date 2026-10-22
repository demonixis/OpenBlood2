#include "engine/world_shaders.h"
#include "engine/shaders/ao_blur_frag.h"
#include "engine/shaders/ao_blur_frag_msl.h"
#include "engine/shaders/ao_depth_frag.h"
#include "engine/shaders/ao_depth_frag_msl.h"
#include "engine/shaders/bloom_down_frag.h"
#include "engine/shaders/bloom_down_frag_msl.h"
#include "engine/shaders/bloom_up_frag.h"
#include "engine/shaders/bloom_up_frag_msl.h"
#include "engine/shaders/composite_frag.h"
#include "engine/shaders/composite_frag_msl.h"
#include "engine/shaders/fxaa_frag.h"
#include "engine/shaders/fxaa_frag_msl.h"
#include "engine/shaders/gtao_frag.h"
#include "engine/shaders/gtao_frag_msl.h"
#include "engine/shaders/line_frag.h"
#include "engine/shaders/line_frag_msl.h"
#include "engine/shaders/line_vert.h"
#include "engine/shaders/line_vert_msl.h"
#include "engine/shaders/pbr_alpha_aux_frag.h"
#include "engine/shaders/pbr_alpha_aux_frag_msl.h"
#include "engine/shaders/pbr_alpha_frag.h"
#include "engine/shaders/pbr_alpha_frag_msl.h"
#include "engine/shaders/pbr_aux_frag.h"
#include "engine/shaders/pbr_aux_frag_msl.h"
#include "engine/shaders/pbr_frag.h"
#include "engine/shaders/pbr_frag_msl.h"
#include "engine/shaders/pbr_high_alpha_aux_frag.h"
#include "engine/shaders/pbr_high_alpha_aux_frag_msl.h"
#include "engine/shaders/pbr_high_alpha_frag.h"
#include "engine/shaders/pbr_high_alpha_frag_msl.h"
#include "engine/shaders/pbr_high_aux_frag.h"
#include "engine/shaders/pbr_high_aux_frag_msl.h"
#include "engine/shaders/pbr_high_frag.h"
#include "engine/shaders/pbr_high_frag_msl.h"
#include "engine/shaders/post_vert.h"
#include "engine/shaders/post_vert_msl.h"
#include "engine/shaders/shadow_frag.h"
#include "engine/shaders/shadow_frag_msl.h"
#include "engine/shaders/skinned_vert.h"
#include "engine/shaders/skinned_vert_msl.h"
#include "engine/shaders/sprite_aux_frag.h"
#include "engine/shaders/sprite_aux_frag_msl.h"
#include "engine/shaders/sprite_frag.h"
#include "engine/shaders/sprite_frag_msl.h"
#include "engine/shaders/world_aux_frag.h"
#include "engine/shaders/world_aux_frag_msl.h"
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
            case WorldShader::ShadowFragment:
                info.stage = SDL_GPU_SHADERSTAGE_FRAGMENT;
                info.code = spirv ? reinterpret_cast<const Uint8*>(shadow_frag)
                                  : reinterpret_cast<const Uint8*>(shadow_frag_msl);
                info.code_size = spirv ? sizeof(shadow_frag) : sizeof(shadow_frag_msl) - 1;
                info.num_uniform_buffers = shadow_frag_uniforms;
                info.num_samplers = shadow_frag_samplers;
                info.num_storage_buffers = shadow_frag_storage_buffers;
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
#define BLOOD2_FRAGMENT_SHADER(kind, name)                                                                             \
    case WorldShader::kind:                                                                                            \
        info.stage = SDL_GPU_SHADERSTAGE_FRAGMENT;                                                                     \
        info.code = spirv ? reinterpret_cast<const Uint8*>(name) : reinterpret_cast<const Uint8*>(name##_msl);         \
        info.code_size = spirv ? sizeof(name) : sizeof(name##_msl) - 1;                                                \
        info.num_uniform_buffers = name##_uniforms;                                                                    \
        info.num_samplers = name##_samplers;                                                                           \
        info.num_storage_buffers = name##_storage_buffers;                                                             \
        break;
                BLOOD2_FRAGMENT_SHADER(PbrFragment, pbr_frag)
                BLOOD2_FRAGMENT_SHADER(PbrHighFragment, pbr_high_frag)
                BLOOD2_FRAGMENT_SHADER(PbrAlphaFragment, pbr_alpha_frag)
                BLOOD2_FRAGMENT_SHADER(PbrHighAlphaFragment, pbr_high_alpha_frag)
                BLOOD2_FRAGMENT_SHADER(LineFragment, line_frag)
                BLOOD2_FRAGMENT_SHADER(OpaqueAuxFragment, world_aux_frag)
                BLOOD2_FRAGMENT_SHADER(AlphaAuxFragment, sprite_aux_frag)
                BLOOD2_FRAGMENT_SHADER(PbrAuxFragment, pbr_aux_frag)
                BLOOD2_FRAGMENT_SHADER(PbrHighAuxFragment, pbr_high_aux_frag)
                BLOOD2_FRAGMENT_SHADER(PbrAlphaAuxFragment, pbr_alpha_aux_frag)
                BLOOD2_FRAGMENT_SHADER(PbrHighAlphaAuxFragment, pbr_high_alpha_aux_frag)
                BLOOD2_FRAGMENT_SHADER(BloomDownFragment, bloom_down_frag)
                BLOOD2_FRAGMENT_SHADER(BloomUpFragment, bloom_up_frag)
                BLOOD2_FRAGMENT_SHADER(CompositeFragment, composite_frag)
                BLOOD2_FRAGMENT_SHADER(FxaaFragment, fxaa_frag)
                BLOOD2_FRAGMENT_SHADER(AoDepthFragment, ao_depth_frag)
                BLOOD2_FRAGMENT_SHADER(GtaoFragment, gtao_frag)
                BLOOD2_FRAGMENT_SHADER(AoBlurFragment, ao_blur_frag)
#undef BLOOD2_FRAGMENT_SHADER
            case WorldShader::PostVertex:
                info.stage = SDL_GPU_SHADERSTAGE_VERTEX;
                info.code =
                    spirv ? reinterpret_cast<const Uint8*>(post_vert) : reinterpret_cast<const Uint8*>(post_vert_msl);
                info.code_size = spirv ? sizeof(post_vert) : sizeof(post_vert_msl) - 1;
                info.num_uniform_buffers = post_vert_uniforms;
                info.num_samplers = post_vert_samplers;
                info.num_storage_buffers = post_vert_storage_buffers;
                break;
            case WorldShader::LineVertex:
                info.stage = SDL_GPU_SHADERSTAGE_VERTEX;
                info.code =
                    spirv ? reinterpret_cast<const Uint8*>(line_vert) : reinterpret_cast<const Uint8*>(line_vert_msl);
                info.code_size = spirv ? sizeof(line_vert) : sizeof(line_vert_msl) - 1;
                info.num_uniform_buffers = line_vert_uniforms;
                info.num_samplers = line_vert_samplers;
                info.num_storage_buffers = line_vert_storage_buffers;
                break;
            default:
                info = {};
                return false;
        }
        return true;
    }
} // namespace OpenBlood2
