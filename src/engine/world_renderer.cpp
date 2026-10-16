#include "engine/world_renderer.h"
#include "engine/shaders/world_frag.h"
#include "engine/shaders/world_vert.h"
#include <cstring>

namespace OpenBlood2
{
    WorldRenderer::~WorldRenderer()
    {
        clear_geometry();
        if (sampler_)
            SDL_ReleaseGPUSampler(device_, sampler_);
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
        for (auto* texture : textures_)
            if (texture)
                SDL_ReleaseGPUTexture(device_, texture);
        textures_.clear();
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
        info.num_samplers = 1;
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
        SDL_GPUVertexAttribute attributes[3]{};
        attributes[0].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3;
        attributes[1].location = 1;
        attributes[1].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3;
        attributes[1].offset = offsetof(WorldVertex, color);
        attributes[2].location = 2;
        attributes[2].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2;
        attributes[2].offset = offsetof(WorldVertex, uv);
        SDL_GPUColorTargetDescription target{};
        target.format = format;
        SDL_GPUGraphicsPipelineCreateInfo create{};
        create.vertex_shader = vertex;
        create.fragment_shader = fragment;
        create.vertex_input_state = {&buffer, 1, attributes, 3};
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
        if (!upload_materials(mesh, error))
        {
            clear_geometry();
            return false;
        }
        revision_ = revision;
        ++stats.uploads;
        return true;
    }
    bool WorldRenderer::upload_materials(const WorldMesh& mesh, std::string& error)
    {
        if (!sampler_)
        {
            SDL_GPUSamplerCreateInfo info{};
            info.min_filter = info.mag_filter = SDL_GPU_FILTER_NEAREST;
            info.mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_NEAREST;
            info.address_mode_u = info.address_mode_v = info.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_REPEAT;
            sampler_ = SDL_CreateGPUSampler(device_, &info);
            if (!sampler_)
            {
                error = SDL_GetError();
                return false;
            }
        }
        const Texture white{1, 1, 1, 0, 0, {255, 255, 255, 255}};
        const auto count = std::max(std::size_t(1), mesh.materials.size());
        if (count > 4097)
        {
            error = "GPU texture count limit exceeded";
            return false;
        }
        std::vector<const Texture*> images(count, &white);
        std::size_t bytes = 0;
        for (std::size_t i = 0; i < count; ++i)
        {
            if (!mesh.materials.empty())
                images[i] = mesh.materials[i].error.empty() ? &mesh.materials[i].image : nullptr;
            if (const auto* image = images[i])
            {
                if (!image->width || !image->height || image->width > 1024 || image->height > 1024 ||
                    image->rgba.size() != std::size_t(image->width) * image->height * 4 ||
                    image->rgba.size() > 128 * 1024 * 1024 - bytes)
                {
                    error = "Invalid GPU texture dimensions/payload/budget";
                    return false;
                }
                bytes += image->rgba.size();
            }
        }
        if (!images[0])
        {
            error = "Missing diagnostic GPU texture";
            return false;
        }
        SDL_GPUTransferBufferCreateInfo transfer_info{SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD, static_cast<Uint32>(bytes),
                                                      0};
        auto* transfer = SDL_CreateGPUTransferBuffer(device_, &transfer_info);
        if (!transfer)
        {
            error = SDL_GetError();
            return false;
        }
        textures_.resize(count);
        bool success = false;
        if (auto* mapped = static_cast<Uint8*>(SDL_MapGPUTransferBuffer(device_, transfer, false)))
        {
            std::size_t offset = 0;
            bool allocated = true;
            for (std::size_t i = 0; i < count; ++i)
            {
                if (const auto* image = images[i])
                {
                    SDL_GPUTextureCreateInfo info{};
                    info.type = SDL_GPU_TEXTURETYPE_2D;
                    info.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
                    info.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER;
                    info.width = image->width;
                    info.height = image->height;
                    info.layer_count_or_depth = 1;
                    info.num_levels = 1;
                    textures_[i] = SDL_CreateGPUTexture(device_, &info);
                    if (!textures_[i])
                    {
                        allocated = false;
                        break;
                    }
                    std::memcpy(mapped + offset, image->rgba.data(), image->rgba.size());
                    offset += image->rgba.size();
                }
            }
            SDL_UnmapGPUTransferBuffer(device_, transfer);
            if (allocated)
                if (auto* commands = SDL_AcquireGPUCommandBuffer(device_))
                {
                    if (auto* copy = SDL_BeginGPUCopyPass(commands))
                    {
                        Uint32 offset = 0;
                        for (std::size_t i = 0; i < count; ++i)
                        {
                            if (const auto* image = images[i])
                            {
                                SDL_GPUTextureTransferInfo source{};
                                source.transfer_buffer = transfer;
                                source.offset = offset;
                                source.pixels_per_row = image->width;
                                source.rows_per_layer = image->height;
                                SDL_GPUTextureRegion target{};
                                target.texture = textures_[i];
                                target.w = image->width;
                                target.h = image->height;
                                target.d = 1;
                                SDL_UploadToGPUTexture(copy, &source, &target, false);
                                offset += static_cast<Uint32>(image->rgba.size());
                            }
                        }
                        SDL_EndGPUCopyPass(copy);
                        success = SDL_SubmitGPUCommandBuffer(commands);
                    }
                    else
                        SDL_CancelGPUCommandBuffer(commands);
                }
        }
        SDL_ReleaseGPUTransferBuffer(device_, transfer);
        if (!success)
            error = SDL_GetError();
        return success;
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
            for (const auto& b : r.batches)
            {
                if (b.material >= textures_.size() || !textures_[b.material] || b.first < r.first ||
                    b.first > std::size_t(r.first) + r.count || b.count > std::size_t(r.first) + r.count - b.first)
                {
                    error = "Invalid diagnostic GPU material batch";
                    return false;
                }
            }
            if (r.count && (textures_.empty() || !textures_[0]))
            {
                error = "GPU materials not prepared";
                return false;
            }
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
                auto batch = [&](std::uint32_t first, std::uint32_t count, std::uint32_t material)
                {
                    SDL_GPUTextureSamplerBinding texture{textures_[material], sampler_};
                    SDL_BindGPUFragmentSamplers(pass, 0, &texture, 1);
                    SDL_DrawGPUPrimitives(pass, count, 1, first, 0);
                };
                if (r.batches.empty())
                    batch(r.first, r.count, 0);
                else
                    for (const auto& b : r.batches)
                        batch(b.first, b.count, b.material);
            }
        }
        SDL_EndGPURenderPass(pass);
        ++stats.frames;
        return true;
    }
} // namespace OpenBlood2
