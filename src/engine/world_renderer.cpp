#include "engine/world_renderer.h"
#include "engine/shaders/world_frag.h"
#include "engine/shaders/world_vert.h"
#include <cstring>

namespace OpenBlood2
{
    WorldRenderer::~WorldRenderer()
    {
        clear_geometry();
        if (depth_)
            SDL_ReleaseGPUTexture(device_, depth_);
        if (pipeline_)
            SDL_ReleaseGPUGraphicsPipeline(device_, pipeline_);
    }
    void WorldRenderer::clear_geometry()
    {
        if (vertices_)
            SDL_ReleaseGPUBuffer(device_, vertices_);
        vertices_ = nullptr;
        revision_ = UINT64_MAX;
        stats.triangles = stats.draws = 0;
    }
    bool WorldRenderer::pipeline(SDL_GPUTextureFormat format, std::string& error)
    {
        if (pipeline_ && color_format_ == format)
            return true;
        if (!(SDL_GetGPUShaderFormats(device_) & SDL_GPU_SHADERFORMAT_SPIRV))
        {
            error = "Diagnostic world renderer currently requires the Vulkan/SPIR-V backend";
            return false;
        }
        SDL_GPUShaderCreateInfo info{};
        info.entrypoint = "main";
        info.format = SDL_GPU_SHADERFORMAT_SPIRV;
        info.stage = SDL_GPU_SHADERSTAGE_VERTEX;
        info.code = reinterpret_cast<const Uint8*>(world_vert);
        info.code_size = sizeof(world_vert);
        info.num_uniform_buffers = 1;
        auto* vertex = SDL_CreateGPUShader(device_, &info);
        info.stage = SDL_GPU_SHADERSTAGE_FRAGMENT;
        info.code = reinterpret_cast<const Uint8*>(world_frag);
        info.code_size = sizeof(world_frag);
        info.num_uniform_buffers = 0;
        auto* fragment = SDL_CreateGPUShader(device_, &info);
        if (!vertex || !fragment)
        {
            error = SDL_GetError();
            if (vertex)
                SDL_ReleaseGPUShader(device_, vertex);
            if (fragment)
                SDL_ReleaseGPUShader(device_, fragment);
            return false;
        }
        SDL_GPUVertexBufferDescription buffer{};
        buffer.pitch = sizeof(WorldVertex);
        buffer.input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX;
        SDL_GPUVertexAttribute attributes[2]{};
        attributes[0].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3;
        attributes[1].location = 1;
        attributes[1].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3;
        attributes[1].offset = offsetof(WorldVertex, color);
        SDL_GPUColorTargetDescription target{};
        target.format = format;
        SDL_GPUGraphicsPipelineCreateInfo create{};
        create.vertex_shader = vertex;
        create.fragment_shader = fragment;
        create.vertex_input_state = {&buffer, 1, attributes, 2};
        create.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
        create.rasterizer_state.fill_mode = SDL_GPU_FILLMODE_FILL;
        create.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_NONE;
        create.rasterizer_state.enable_depth_clip = true;
        create.multisample_state.sample_count = SDL_GPU_SAMPLECOUNT_1;
        create.depth_stencil_state.enable_depth_test = true;
        create.depth_stencil_state.enable_depth_write = true;
        create.depth_stencil_state.compare_op = SDL_GPU_COMPAREOP_LESS;
        create.target_info.color_target_descriptions = &target;
        create.target_info.num_color_targets = 1;
        create.target_info.has_depth_stencil_target = true;
        create.target_info.depth_stencil_format = SDL_GPU_TEXTUREFORMAT_D32_FLOAT;
        auto* next = SDL_CreateGPUGraphicsPipeline(device_, &create);
        SDL_ReleaseGPUShader(device_, vertex);
        SDL_ReleaseGPUShader(device_, fragment);
        if (!next)
        {
            error = SDL_GetError();
            return false;
        }
        if (pipeline_)
            SDL_ReleaseGPUGraphicsPipeline(device_, pipeline_);
        pipeline_ = next;
        color_format_ = format;
        return true;
    }
    bool WorldRenderer::prepare(const WorldMesh& mesh, std::uint64_t revision, SDL_GPUTextureFormat format,
                                Uint32 width, Uint32 height, std::string& error)
    {
        error.clear();
        if (!width || !height || width > 8192 || height > 8192 || mesh.vertices.size() > 3000000)
        {
            error = "Diagnostic GPU dimensions/geometry exceed limits";
            return false;
        }
        if (!pipeline(format, error))
            return false;
        if (!depth_ || width_ != width || height_ != height)
        {
            SDL_GPUTextureCreateInfo info{};
            info.type = SDL_GPU_TEXTURETYPE_2D;
            info.format = SDL_GPU_TEXTUREFORMAT_D32_FLOAT;
            info.usage = SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET;
            info.width = width;
            info.height = height;
            info.layer_count_or_depth = 1;
            info.num_levels = 1;
            auto* next = SDL_CreateGPUTexture(device_, &info);
            if (!next)
            {
                error = SDL_GetError();
                return false;
            }
            if (depth_)
                SDL_ReleaseGPUTexture(device_, depth_);
            depth_ = next;
            width_ = width;
            height_ = height;
        }
        if (revision_ == revision)
            return true;
        clear_geometry();
        if (mesh.vertices.empty())
        {
            revision_ = revision;
            return true;
        }
        const auto bytes = static_cast<Uint32>(mesh.vertices.size() * sizeof(WorldVertex));
        SDL_GPUBufferCreateInfo info{SDL_GPU_BUFFERUSAGE_VERTEX, bytes, 0};
        auto* next = SDL_CreateGPUBuffer(device_, &info);
        SDL_GPUTransferBufferCreateInfo transfer_info{SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD, bytes, 0};
        auto* transfer = SDL_CreateGPUTransferBuffer(device_, &transfer_info);
        bool success = false;
        if (next && transfer)
        {
            if (auto* data = SDL_MapGPUTransferBuffer(device_, transfer, false))
            {
                std::memcpy(data, mesh.vertices.data(), bytes);
                SDL_UnmapGPUTransferBuffer(device_, transfer);
                if (auto* commands = SDL_AcquireGPUCommandBuffer(device_))
                {
                    if (auto* copy = SDL_BeginGPUCopyPass(commands))
                    {
                        SDL_GPUTransferBufferLocation source{transfer, 0};
                        SDL_GPUBufferRegion target{next, 0, bytes};
                        SDL_UploadToGPUBuffer(copy, &source, &target, false);
                        SDL_EndGPUCopyPass(copy);
                        success = SDL_SubmitGPUCommandBuffer(commands);
                    }
                    else
                        SDL_CancelGPUCommandBuffer(commands);
                }
            }
        }
        if (transfer)
            SDL_ReleaseGPUTransferBuffer(device_, transfer);
        if (!success)
        {
            error = SDL_GetError();
            if (next)
                SDL_ReleaseGPUBuffer(device_, next);
            return false;
        }
        vertices_ = next;
        revision_ = revision;
        ++stats.uploads;
        return true;
    }
    bool WorldRenderer::draw(SDL_GPUCommandBuffer* commands, SDL_GPUTexture* target, const WorldMesh& mesh,
                             std::span<const WorldDraw> draws, DVector eye, DRotation view, bool letterbox,
                             std::string& error)
    {
        if (!pipeline_ || !depth_ || (!vertices_ && !mesh.vertices.empty()))
        {
            error = "Diagnostic renderer not prepared";
            return false;
        }
        const Uint32 top = letterbox ? height_ / 6 : 0, height = height_ - 2 * top;
        const float aspect = float(width_) / float(height);
        WorldViewUniform uniform;
        for (const auto& d : draws)
        {
            if (d.range >= mesh.ranges.size() || !world_view_uniform(eye, view, aspect, d, uniform))
            {
                error = "Invalid diagnostic draw/camera";
                return false;
            }
            const auto& r = mesh.ranges[d.range];
            if (r.first > mesh.vertices.size() || r.count > mesh.vertices.size() - r.first)
            {
                error = "Invalid diagnostic GPU range";
                return false;
            }
        }
        SDL_GPUColorTargetInfo color{};
        color.texture = target;
        color.clear_color = {.025f, .035f, .05f, 1};
        color.load_op = SDL_GPU_LOADOP_CLEAR;
        color.store_op = SDL_GPU_STOREOP_STORE;
        SDL_GPUDepthStencilTargetInfo depth{};
        depth.texture = depth_;
        depth.clear_depth = 1;
        depth.load_op = SDL_GPU_LOADOP_CLEAR;
        depth.store_op = SDL_GPU_STOREOP_DONT_CARE;
        depth.stencil_load_op = SDL_GPU_LOADOP_DONT_CARE;
        depth.stencil_store_op = SDL_GPU_STOREOP_DONT_CARE;
        depth.cycle = true;
        auto* pass = SDL_BeginGPURenderPass(commands, &color, 1, &depth);
        if (!pass)
        {
            error = SDL_GetError();
            return false;
        }
        SDL_GPUViewport viewport{0, float(top), float(width_), float(height), 0, 1};
        SDL_SetGPUViewport(pass, &viewport);
        SDL_BindGPUGraphicsPipeline(pass, pipeline_);
        if (vertices_)
        {
            SDL_GPUBufferBinding binding{vertices_, 0};
            SDL_BindGPUVertexBuffers(pass, 0, &binding, 1);
            for (const auto& d : draws)
            {
                const auto& r = mesh.ranges[d.range];
                if (!r.count)
                    continue;
                world_view_uniform(eye, view, aspect, d, uniform);
                SDL_PushGPUVertexUniformData(commands, 0, &uniform, sizeof(uniform));
                SDL_DrawGPUPrimitives(pass, r.count, 1, r.first, 0);
            }
        }
        SDL_EndGPURenderPass(pass);
        ++stats.frames;
        return true;
    }
} // namespace OpenBlood2
