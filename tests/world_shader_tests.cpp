#include "engine/world_shaders.h"
#include "engine/world_view.h"
#include <array>
#include <cstring>
#include <iostream>
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
