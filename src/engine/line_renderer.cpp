#include "engine/line_renderer.h"
#include "engine/world_shaders.h"
#include <SDL3/SDL.h>
#include <algorithm>
#include <cstddef>

namespace OpenBlood2
{
    void add_line(std::vector<LineVertex>& v, std::array<float, 3> a, std::array<float, 3> b,
                  std::array<float, 4> color)
    {
        v.push_back({a, color});
        v.push_back({b, color});
    }

    void add_box(std::vector<LineVertex>& v, const xr::Pose& pose, std::array<float, 3> scale,
                 std::array<float, 4> color)
    {
        std::array<std::array<float, 3>, 8> corners{};
        for (int i = 0; i < 8; ++i)
        {
            const xr::Vec3 local{i & 1 ? scale[0] : -scale[0], i & 2 ? scale[1] : -scale[1],
                                 i & 4 ? scale[2] : -scale[2]};
            const auto r = xr::rotate(pose.orientation, local);
            corners[i] = {pose.position[0] + r[0], pose.position[1] + r[1], pose.position[2] + r[2]};
        }
        constexpr std::array<std::array<int, 2>, 12> edges{
            {{0, 1}, {2, 3}, {4, 5}, {6, 7}, {0, 2}, {1, 3}, {4, 6}, {5, 7}, {0, 4}, {1, 5}, {2, 6}, {3, 7}}};
        for (const auto& e : edges)
            add_line(v, corners[e[0]], corners[e[1]], color);
    }

    LineRenderer::LineRenderer(SDL_GPUDevice* device) : device_(device) {}

    LineRenderer::~LineRenderer()
    {
        if (!device_)
            return;
        if (pipeline_ || buffer_ || transfer_)
            SDL_WaitForGPUIdle(device_);
        if (transfer_)
            SDL_ReleaseGPUTransferBuffer(device_, transfer_);
        if (buffer_)
            SDL_ReleaseGPUBuffer(device_, buffer_);
        if (pipeline_)
            SDL_ReleaseGPUGraphicsPipeline(device_, pipeline_);
    }

    bool LineRenderer::prepare(SDL_GPUTextureFormat color, SDL_GPUTextureFormat depth, std::string& error,
                               SDL_GPUSampleCount samples, unsigned targets, bool blend)
    {
        if (targets != 1 && targets != 3 && targets != 5)
        {
            error = "Invalid line render target count";
            return false;
        }
        if (pipeline_ && color == color_ && depth == depth_ && samples == samples_ && targets == targets_ &&
            blend == blend_)
            return true;
        if (pipeline_)
        {
            SDL_ReleaseGPUGraphicsPipeline(device_, pipeline_);
            pipeline_ = nullptr;
        }
        const SDL_GPUShaderFormat formats = SDL_GetGPUShaderFormats(device_);
        SDL_GPUShaderCreateInfo vertex_info{}, fragment_info{};
        if (!world_shader_info(WorldShader::LineVertex, formats, vertex_info) ||
            !world_shader_info(targets == 5   ? WorldShader::LineAuxSsrFragment
                               : targets == 3 ? WorldShader::LineAuxFragment
                                              : WorldShader::LineFragment,
                               formats, fragment_info))
        {
            error = "Line shaders unavailable for this GPU";
            return false;
        }
        SDL_GPUShader* vertex = SDL_CreateGPUShader(device_, &vertex_info);
        SDL_GPUShader* fragment = SDL_CreateGPUShader(device_, &fragment_info);
        if (vertex && fragment)
        {
            const SDL_GPUVertexBufferDescription buffer{0, sizeof(LineVertex), SDL_GPU_VERTEXINPUTRATE_VERTEX, 0};
            const std::array<SDL_GPUVertexAttribute, 2> attributes{
                {{0, 0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3, 0},
                 {1, 0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4, offsetof(LineVertex, color)}}};
            std::array<SDL_GPUColorTargetDescription, 5> target{};
            for (unsigned i = 0; i < targets; ++i)
            {
                target[i].format = i == 0   ? color
                                   : i == 2 ? SDL_GPU_TEXTUREFORMAT_R32_FLOAT
                                            : SDL_GPU_TEXTUREFORMAT_R16G16B16A16_FLOAT;
                target[i].blend_state.enable_color_write_mask = i != 0;
                target[i].blend_state.color_write_mask = 0;
            }
            target[0].blend_state.enable_blend = blend;
            target[0].blend_state.src_color_blendfactor = SDL_GPU_BLENDFACTOR_SRC_ALPHA;
            target[0].blend_state.dst_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
            target[0].blend_state.color_blend_op = SDL_GPU_BLENDOP_ADD;
            target[0].blend_state.src_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
            target[0].blend_state.dst_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
            target[0].blend_state.alpha_blend_op = SDL_GPU_BLENDOP_ADD;
            if (targets > 1)
            {
                target[1].blend_state = target[0].blend_state;
                target[1].blend_state.src_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
                target[1].blend_state.src_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ZERO;
            }
            SDL_GPUGraphicsPipelineCreateInfo info{};
            info.vertex_shader = vertex;
            info.fragment_shader = fragment;
            info.vertex_input_state.vertex_buffer_descriptions = &buffer;
            info.vertex_input_state.num_vertex_buffers = 1;
            info.vertex_input_state.vertex_attributes = attributes.data();
            info.vertex_input_state.num_vertex_attributes = static_cast<Uint32>(attributes.size());
            info.primitive_type = SDL_GPU_PRIMITIVETYPE_LINELIST;
            info.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_NONE;
            info.depth_stencil_state.enable_depth_test = true;
            info.depth_stencil_state.enable_depth_write = !blend;
            info.depth_stencil_state.compare_op = SDL_GPU_COMPAREOP_LESS;
            info.multisample_state.sample_count = samples;
            info.target_info.color_target_descriptions = target.data();
            info.target_info.num_color_targets = targets;
            info.target_info.depth_stencil_format = depth;
            info.target_info.has_depth_stencil_target = true;
            pipeline_ = SDL_CreateGPUGraphicsPipeline(device_, &info);
        }
        if (vertex)
            SDL_ReleaseGPUShader(device_, vertex);
        if (fragment)
            SDL_ReleaseGPUShader(device_, fragment);
        if (!pipeline_)
        {
            error = std::string("Line pipeline: ") + SDL_GetError();
            return false;
        }
        samples_ = samples;
        targets_ = targets;
        blend_ = blend;
        color_ = color;
        depth_ = depth;
        return true;
    }

    bool LineRenderer::upload(SDL_GPUCommandBuffer* commands, std::span<const LineVertex> vertices, std::string& error)
    {
        uploaded_ = 0;
        if (vertices.empty())
            return true;
        const auto count = static_cast<std::uint32_t>(vertices.size());
        if (count > capacity_)
        {
            // Grow geometrically; the old buffers may still be read by frames in flight.
            if (transfer_)
                SDL_ReleaseGPUTransferBuffer(device_, transfer_);
            if (buffer_)
                SDL_ReleaseGPUBuffer(device_, buffer_);
            transfer_ = nullptr;
            buffer_ = nullptr;
            capacity_ = std::max<std::uint32_t>(count, std::max<std::uint32_t>(256, capacity_ * 2));
            const auto bytes = static_cast<Uint32>(capacity_ * sizeof(LineVertex));
            const SDL_GPUBufferCreateInfo buffer_info{SDL_GPU_BUFFERUSAGE_VERTEX, bytes, 0};
            const SDL_GPUTransferBufferCreateInfo transfer_info{SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD, bytes, 0};
            buffer_ = SDL_CreateGPUBuffer(device_, &buffer_info);
            transfer_ = buffer_ ? SDL_CreateGPUTransferBuffer(device_, &transfer_info) : nullptr;
            if (!transfer_)
            {
                capacity_ = 0;
                error = std::string("Line buffers: ") + SDL_GetError();
                return false;
            }
        }
        const auto bytes = static_cast<Uint32>(count * sizeof(LineVertex));
        void* mapped = SDL_MapGPUTransferBuffer(device_, transfer_, true);
        if (!mapped)
        {
            error = std::string("Line upload: ") + SDL_GetError();
            return false;
        }
        SDL_memcpy(mapped, vertices.data(), bytes);
        SDL_UnmapGPUTransferBuffer(device_, transfer_);
        SDL_GPUCopyPass* copy = SDL_BeginGPUCopyPass(commands);
        if (!copy)
        {
            error = std::string("Line upload: ") + SDL_GetError();
            return false;
        }
        const SDL_GPUTransferBufferLocation source{transfer_, 0};
        const SDL_GPUBufferRegion destination{buffer_, 0, bytes};
        SDL_UploadToGPUBuffer(copy, &source, &destination, true);
        SDL_EndGPUCopyPass(copy);
        uploaded_ = count;
        return true;
    }

    void LineRenderer::draw(SDL_GPURenderPass* pass, SDL_GPUCommandBuffer* commands, const xr::LineUniform& uniform,
                            Uint32 width, Uint32 height, float min_depth, float max_depth, std::uint32_t first,
                            std::uint32_t count, Uint32 top) const
    {
        if (!pipeline_ || !buffer_ || !count || first > uploaded_ || count > uploaded_ - first)
            return;
        const SDL_GPUViewport viewport{0,         float(top), static_cast<float>(width), static_cast<float>(height),
                                       min_depth, max_depth};
        SDL_SetGPUViewport(pass, &viewport);
        SDL_BindGPUGraphicsPipeline(pass, pipeline_);
        const SDL_GPUBufferBinding binding{buffer_, 0};
        SDL_BindGPUVertexBuffers(pass, 0, &binding, 1);
        SDL_PushGPUVertexUniformData(commands, 0, &uniform, sizeof(uniform));
        SDL_DrawGPUPrimitives(pass, count, 1, first, 0);
    }
} // namespace OpenBlood2
