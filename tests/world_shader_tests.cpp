#include "engine/world_shaders.h"
#include "engine/world_view.h"
#include <array>
#include <cstring>
#include <iostream>
#include <string>
#include <string_view>
using namespace OpenBlood2;
#define CHECK(x)                                                                                                       \
    do                                                                                                                 \
    {                                                                                                                  \
        if (!(x))                                                                                                      \
        {                                                                                                              \
            std::cerr << __LINE__ << ": " #x "\n";                                                                     \
            return 1;                                                                                                  \
        }                                                                                                              \
    } while (false)
int main()
{
    static_assert(sizeof(WorldViewUniform) == 8 * 16); // std140/Metal float4 uniform layout.
    CHECK(WorldShaderFormats == (SDL_GPU_SHADERFORMAT_SPIRV | SDL_GPU_SHADERFORMAT_MSL));
    SDL_GPUShaderCreateInfo info{};
    for (auto formats : {SDL_GPU_SHADERFORMAT_SPIRV, SDL_GPU_SHADERFORMAT_MSL, WorldShaderFormats})
        for (auto shader : {WorldShader::Vertex, WorldShader::OpaqueFragment, WorldShader::AlphaFragment,
                            WorldShader::SkinnedVertex, WorldShader::ShadowFragment})
        {
            const bool vertex = shader == WorldShader::Vertex || shader == WorldShader::SkinnedVertex;
            const bool caster = shader == WorldShader::ShadowFragment;
            CHECK(world_shader_info(shader, formats, info));
            CHECK(info.code && info.code_size > 128 && info.props == 0);
            // Only the skinned vertex stage reads storage: its node palette and packed deformation.
            CHECK(info.num_storage_buffers == (shader == WorldShader::SkinnedVertex ? 2u : 0u) &&
                  info.num_storage_textures == 0);
            // Fragments sample the albedo (slot 0), the lightmap page (slot 1) and the model shadow
            // tiles (slot 2), with per-draw light and per-record shadow uniforms; the shadow tile
            // fragment only writes the caster height.
            CHECK(info.num_samplers == (vertex || caster ? 0u : 3u));
            CHECK(info.num_uniform_buffers == (vertex ? 1u : caster ? 0u : 2u));
            CHECK(info.stage == (vertex ? SDL_GPU_SHADERSTAGE_VERTEX : SDL_GPU_SHADERSTAGE_FRAGMENT));
            if (formats & SDL_GPU_SHADERFORMAT_SPIRV)
            {
                CHECK(info.format == SDL_GPU_SHADERFORMAT_SPIRV && std::strcmp(info.entrypoint, "main") == 0);
                const unsigned char magic[] = {3, 2, 35, 7};
                CHECK(std::memcmp(info.code, magic, 4) == 0 && info.code_size % 4 == 0);
            }
            else
            {
                CHECK(info.format == SDL_GPU_SHADERFORMAT_MSL && std::strcmp(info.entrypoint, "main0") == 0);
                const std::string_view source(reinterpret_cast<const char*>(info.code), info.code_size);
                CHECK(source.find("#include <metal_stdlib>") != source.npos);
                CHECK(info.code[info.code_size] == 0); // Runtime Metal compilation also gets terminated source.
                CHECK(source.find(vertex ? "vertex main0_out main0(" : "fragment main0_out main0(") != source.npos);
                if (info.num_uniform_buffers)
                    CHECK(source.find("[[buffer(0)]]") != source.npos);
                if (info.num_samplers)
                    CHECK(
                        source.find("[[texture(0)]]") != source.npos && source.find("[[sampler(0)]]") != source.npos &&
                        source.find("[[texture(1)]]") != source.npos && source.find("[[sampler(1)]]") != source.npos &&
                        source.find("[[texture(2)]]") != source.npos && source.find("[[sampler(2)]]") != source.npos &&
                        source.find("[[buffer(1)]]") != source.npos);
                if (shader == WorldShader::SkinnedVertex)
                    CHECK(source.find("[[buffer(1)]]") != source.npos && source.find("[[buffer(2)]]") != source.npos &&
                          source.find("uint4 bases;") != source.npos);
                // HLSL cbuffer members keep their names; varyings are matched by location.
                if (vertex)
                    CHECK(source.find("View._textureOffset.xy") != source.npos &&
                          source.find("[[user(locn2)]]") != source.npos &&
                          source.find("[[user(locn3)]]") != source.npos);
                else if (caster)
                    CHECK(source.find("in_var_TEXCOORD2 [[user(locn2)]]") != source.npos);
                else
                    CHECK(source.find("float4 positionRadius;") != source.npos &&
                          source.find("float4 lightColor;") != source.npos &&
                          source.find("float3 in_var_TEXCOORD2 [[user(locn2)]]") != source.npos);
            }
            const auto* owned = info.code;
            SDL_GPUShaderCreateInfo repeated{};
            CHECK(world_shader_info(shader, formats, repeated) && repeated.code == owned &&
                  repeated.code_size == info.code_size);
        }
    for (auto kind : {WorldShader::LineFragment, WorldShader::LineAuxFragment, WorldShader::LineAuxSsrFragment})
        for (auto format : {SDL_GPU_SHADERFORMAT_SPIRV, SDL_GPU_SHADERFORMAT_MSL})
        {
            CHECK(world_shader_info(kind, format, info));
            CHECK(info.num_samplers == 0 && info.num_uniform_buffers == 0 && info.num_storage_buffers == 0);
            if (format == SDL_GPU_SHADERFORMAT_MSL)
            {
                const std::string_view text(reinterpret_cast<const char*>(info.code), info.code_size);
                CHECK((text.find("[[color(1)]]") != text.npos) == (kind != WorldShader::LineFragment));
                CHECK((text.find("[[color(4)]]") != text.npos) == (kind == WorldShader::LineAuxSsrFragment));
            }
        }
    // Post-processing scene variants keep their base shader's resources and add colour targets 1
    // and 2; the post-processing passes draw a buffer-free triangle and sample up to four textures.
    struct Expected
    {
        WorldShader shader;
        unsigned samplers, uniforms, storage_buffers;
        unsigned outputs; // Fragment colour outputs.
    };
    for (auto formats : {SDL_GPU_SHADERFORMAT_SPIRV, SDL_GPU_SHADERFORMAT_MSL})
        for (const auto& expected : {Expected{WorldShader::OpaqueAuxFragment, 3, 2, 0, 3},
                                     Expected{WorldShader::AlphaAuxFragment, 3, 2, 0, 3},
                                     Expected{WorldShader::PbrAuxFragment, 4, 4, 2, 3},
                                     Expected{WorldShader::PbrHighAuxFragment, 4, 4, 2, 3},
                                     Expected{WorldShader::PbrAlphaAuxFragment, 4, 4, 2, 3},
                                     Expected{WorldShader::PbrHighAlphaAuxFragment, 4, 4, 2, 3},
                                     Expected{WorldShader::PostVertex, 0, 0, 0, 0},
                                     Expected{WorldShader::BloomDownFragment, 1, 1, 0, 1},
                                     Expected{WorldShader::BloomUpFragment, 1, 1, 0, 1},
                                     Expected{WorldShader::CompositeFragment, 9, 1, 0, 1},
                                     Expected{WorldShader::FxaaFragment, 1, 1, 0, 1},
                                     Expected{WorldShader::AoDepthFragment, 1, 1, 0, 2},
                                     Expected{WorldShader::GtaoFragment, 1, 1, 0, 1},
                                     Expected{WorldShader::AoBlurFragment, 2, 1, 0, 1},
                                     Expected{WorldShader::SsrTraceFragment, 3, 1, 0, 1},
                                     Expected{WorldShader::SsrBlurFragment, 3, 1, 0, 1},
                                     Expected{WorldShader::HizDownFragment, 1, 1, 0, 2},
                                     Expected{WorldShader::OpaqueAuxSsrFragment, 3, 2, 0, 5},
                                     Expected{WorldShader::AlphaAuxSsrFragment, 3, 2, 0, 5},
                                     Expected{WorldShader::PbrAuxSsrFragment, 4, 4, 2, 5},
                                     Expected{WorldShader::PbrHighAuxSsrFragment, 4, 4, 2, 5},
                                     Expected{WorldShader::PbrAlphaAuxSsrFragment, 4, 4, 2, 5},
                                     Expected{WorldShader::PbrHighAlphaAuxSsrFragment, 4, 4, 2, 5}})
        {
            const bool vertex = expected.shader == WorldShader::PostVertex;
            CHECK(world_shader_info(expected.shader, formats, info));
            CHECK(info.code && info.code_size > 128 && info.num_samplers == expected.samplers &&
                  info.num_uniform_buffers == expected.uniforms &&
                  info.num_storage_buffers == expected.storage_buffers && info.num_storage_textures == 0);
            CHECK(info.stage == (vertex ? SDL_GPU_SHADERSTAGE_VERTEX : SDL_GPU_SHADERSTAGE_FRAGMENT));
            if (formats == SDL_GPU_SHADERFORMAT_MSL)
            {
                const std::string_view source(reinterpret_cast<const char*>(info.code), info.code_size);
                CHECK(source.find(vertex ? "vertex main0_out main0(" : "fragment main0_out main0(") != source.npos);
                const auto output = [&](unsigned i)
                { return source.find("[[color(" + std::to_string(i) + ")]]") != source.npos; };
                CHECK(vertex || (output(expected.outputs - 1) && !output(expected.outputs)));
                if (vertex)
                    CHECK(source.find("[[vertex_id]]") != source.npos);
            }
        }
    for (SDL_GPUShaderFormat formats : std::array<SDL_GPUShaderFormat, 3>{
             SDL_GPU_SHADERFORMAT_INVALID, SDL_GPU_SHADERFORMAT_DXIL, SDL_GPU_SHADERFORMAT_METALLIB})
    {
        CHECK(!world_shader_info(WorldShader::Vertex, formats, info));
        CHECK(!info.code && !info.code_size && !info.entrypoint);
    }
    CHECK(!world_shader_info(static_cast<WorldShader>(999), WorldShaderFormats, info) && !info.code);
    std::cout << "Shader selection: SPIR-V/MSL stages, generated entry points, reflected bindings, static lifetime and "
                 "unsupported formats pass; native Metal compilation not exercised\n";
    return 0;
}
