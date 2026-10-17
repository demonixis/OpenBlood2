#include "engine/world_renderer.h"
#include "engine/collision.h"
#include "engine/world_shaders.h"
#include <cmath>
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
        if (alpha_pipeline_)
            SDL_ReleaseGPUGraphicsPipeline(device_, alpha_pipeline_);
        if (sky_pipeline_)
            SDL_ReleaseGPUGraphicsPipeline(device_, sky_pipeline_);
        if (sky_alpha_pipeline_)
            SDL_ReleaseGPUGraphicsPipeline(device_, sky_alpha_pipeline_);
    }
    void WorldRenderer::clear_geometry()
    {
        if (vertices_)
            SDL_ReleaseGPUBuffer(device_, vertices_);
        vertices_ = nullptr;
        if (vertex_transfer_)
            SDL_ReleaseGPUTransferBuffer(device_, vertex_transfer_);
        vertex_transfer_ = nullptr;
        vertex_capacity_ = 0;
        for (auto* texture : textures_)
            if (texture)
                SDL_ReleaseGPUTexture(device_, texture);
        textures_.clear();
        revision_ = UINT64_MAX;
        stats.triangles = stats.draws = stats.alpha_batches = stats.pan_batches = stats.alpha_pan_batches = 0;
        stats.surface_time = 0;
        stats.sky_batches = 0;
        alpha_order_.clear();
    }
    bool WorldRenderer::pipeline(SDL_GPUTextureFormat format, std::string& error, bool translucent, bool sky)
    {
        if (color_format_ != format)
        {
            if (pipeline_)
                SDL_ReleaseGPUGraphicsPipeline(device_, pipeline_);
            if (alpha_pipeline_)
                SDL_ReleaseGPUGraphicsPipeline(device_, alpha_pipeline_);
            if (sky_pipeline_)
                SDL_ReleaseGPUGraphicsPipeline(device_, sky_pipeline_);
            if (sky_alpha_pipeline_)
                SDL_ReleaseGPUGraphicsPipeline(device_, sky_alpha_pipeline_);
            sky_pipeline_ = sky_alpha_pipeline_ = nullptr;
            pipeline_ = alpha_pipeline_ = nullptr;
            color_format_ = format;
        }
        auto*& selected =
            sky ? (translucent ? sky_alpha_pipeline_ : sky_pipeline_) : (translucent ? alpha_pipeline_ : pipeline_);
        if (selected)
            return true;
        const bool alpha = translucent || style_ == DrawStyle::SpriteAlpha;
        SDL_GPUShaderCreateInfo vertex_info{}, fragment_info{};
        const auto formats = SDL_GetGPUShaderFormats(device_);
        if (!world_shader_info(WorldShader::Vertex, formats, vertex_info) ||
            !world_shader_info(alpha ? WorldShader::AlphaFragment : WorldShader::OpaqueFragment, formats,
                               fragment_info))
        {
            error = "No bundled world shader for this GPU backend (SPIR-V or MSL required)";
            return false;
        }
        auto* vertex = SDL_CreateGPUShader(device_, &vertex_info);
        if (!vertex)
        {
            error = SDL_GetError();
            return false;
        }
        auto* fragment = SDL_CreateGPUShader(device_, &fragment_info);
        if (!fragment)
        {
            error = SDL_GetError();
            SDL_ReleaseGPUShader(device_, vertex);
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
        if (alpha)
        {
            target.blend_state.enable_blend = true;
            target.blend_state.src_color_blendfactor = SDL_GPU_BLENDFACTOR_SRC_ALPHA;
            target.blend_state.dst_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
            target.blend_state.color_blend_op = SDL_GPU_BLENDOP_ADD;
            target.blend_state.src_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
            target.blend_state.dst_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
            target.blend_state.alpha_blend_op = SDL_GPU_BLENDOP_ADD;
        }
        SDL_GPUGraphicsPipelineCreateInfo create{};
        create.vertex_shader = vertex;
        create.fragment_shader = fragment;
        create.vertex_input_state = {&buffer, 1, attributes, 3};
        create.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
        create.rasterizer_state.fill_mode = SDL_GPU_FILLMODE_FILL;
        create.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_NONE;
        create.rasterizer_state.enable_depth_clip = true;
        create.multisample_state.sample_count = SDL_GPU_SAMPLECOUNT_1;
        create.depth_stencil_state.enable_depth_test = !sky;
        create.depth_stencil_state.enable_depth_write = !alpha && !sky;
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
        selected = next;
        color_format_ = format;
        return true;
    }
    bool WorldRenderer::prepare(const WorldMesh& mesh, std::uint64_t revision, SDL_GPUTextureFormat format,
                                Uint32 width, Uint32 height, std::string& error, bool dynamic)
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
            return !dynamic || upload_vertices(mesh, error);
        clear_geometry();
        if (!upload_vertices(mesh, error))
            return false;
        if (!upload_materials(mesh, error))
        {
            clear_geometry();
            return false;
        }
        revision_ = revision;
        ++stats.uploads;
        return true;
    }
    bool WorldRenderer::upload_vertices(const WorldMesh& mesh, std::string& error)
    {
        if (mesh.vertices.size() > 3000000)
        {
            error = "GPU vertex budget exceeded";
            return false;
        }
        const auto bytes = static_cast<Uint32>(mesh.vertices.size() * sizeof(WorldVertex));
        if (!bytes)
            return true;
        if (bytes > vertex_capacity_)
        {
            SDL_GPUBufferCreateInfo info{SDL_GPU_BUFFERUSAGE_VERTEX, bytes, 0};
            auto* next = SDL_CreateGPUBuffer(device_, &info);
            SDL_GPUTransferBufferCreateInfo transfer_info{SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD, bytes, 0};
            auto* transfer = SDL_CreateGPUTransferBuffer(device_, &transfer_info);
            if (!next || !transfer)
            {
                if (next)
                    SDL_ReleaseGPUBuffer(device_, next);
                if (transfer)
                    SDL_ReleaseGPUTransferBuffer(device_, transfer);
                error = SDL_GetError();
                return false;
            }
            if (vertices_)
                SDL_ReleaseGPUBuffer(device_, vertices_);
            if (vertex_transfer_)
                SDL_ReleaseGPUTransferBuffer(device_, vertex_transfer_);
            vertices_ = next;
            vertex_transfer_ = transfer;
            vertex_capacity_ = bytes;
        }
        bool success = false;
        if (auto* data = SDL_MapGPUTransferBuffer(device_, vertex_transfer_, true))
        {
            std::memcpy(data, mesh.vertices.data(), bytes);
            SDL_UnmapGPUTransferBuffer(device_, vertex_transfer_);
            if (auto* commands = SDL_AcquireGPUCommandBuffer(device_))
            {
                if (auto* copy = SDL_BeginGPUCopyPass(commands))
                {
                    SDL_GPUTransferBufferLocation source{vertex_transfer_, 0};
                    SDL_GPUBufferRegion target{vertices_, 0, bytes};
                    SDL_UploadToGPUBuffer(copy, &source, &target, true);
                    SDL_EndGPUCopyPass(copy);
                    success = SDL_SubmitGPUCommandBuffer(commands);
                }
                else
                    SDL_CancelGPUCommandBuffer(commands);
            }
        }
        if (!success)
            error = SDL_GetError();
        return success;
    }
    bool WorldRenderer::upload_materials(const WorldMesh& mesh, std::string& error)
    {
        if (!sampler_)
        {
            SDL_GPUSamplerCreateInfo info{};
            info.min_filter = info.mag_filter = SDL_GPU_FILTER_NEAREST;
            info.mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_NEAREST;
            info.address_mode_u = info.address_mode_v = info.address_mode_w =
                style_ == DrawStyle::SpriteAlpha ? SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE
                                                 : SDL_GPU_SAMPLERADDRESSMODE_REPEAT;
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
                             std::string& error, DrawLoad load, SDL_GPUTexture* shared_depth, WorldPass world_pass,
                             double surface_time, const PointLight& light)
    {
        if (!physics_vector(light.position) || !physics_vector(light.color) || !std::isfinite(light.radius) ||
            light.radius < 0 || light.radius > 1e7 || light.color.x < 0 || light.color.y < 0 || light.color.z < 0 ||
            light.color.x > 1 || light.color.y > 1 || light.color.z > 1)
        {
            error = "Invalid point light";
            return false;
        }
        if (!pipeline_ || !depth_ || (!vertices_ && !mesh.vertices.empty()))
        {
            error = "Diagnostic renderer not prepared";
            return false;
        }
        std::array<float, 4> texture_offset{};
        if (!world_texture_offset({}, surface_time, texture_offset))
        {
            error = "Invalid surface effect clock";
            return false;
        }
        std::size_t pan_batches = 0;
        const bool translucent = world_pass == WorldPass::Translucent;
        const bool sky = world_pass == WorldPass::Sky;
        if (sky && (style_ != DrawStyle::Opaque || load != DrawLoad::Clear ||
                    !pipeline(color_format_, error, false, true) || !pipeline(color_format_, error, true, true)))
        {
            if (error.empty())
                error = "Sky requires an opaque renderer and clear pass";
            return false;
        }
        if (sky || world_pass == WorldPass::Opaque)
            stats.sky_batches = 0;
        if (translucent && (style_ != DrawStyle::Opaque || load != DrawLoad::Load))
        {
            error = "Transparent world pass requires loaded opaque depth";
            return false;
        }
        if (translucent &&
            (!world_alpha_order(mesh, draws, eye, view, alpha_order_, error) || !pipeline(color_format_, error, true)))
            return false;
        const Uint32 top = letterbox ? height_ / 6 : 0, height = height_ - 2 * top;
        const float aspect = float(width_) / float(height);
        WorldViewUniform uniform;
        for (const auto& d : draws)
        {
            if ((sky || translucent || style_ == DrawStyle::SpriteAlpha) &&
                (!std::isfinite(d.alpha) || d.alpha < 0 || d.alpha > 1))
            {
                error = "Invalid sprite draw alpha";
                return false;
            }
            if (d.range >= mesh.ranges.size() || !world_view_uniform(eye, view, aspect, d, uniform))
            {
                error = "Invalid diagnostic draw/camera";
                return false;
            }
            const auto& r = mesh.ranges[d.range];
            for (const auto& b : r.batches)
            {
                if (b.material >= textures_.size() || !textures_[b.material] || b.first < r.first ||
                    b.first > std::size_t(r.first) + r.count || b.count > std::size_t(r.first) + r.count - b.first ||
                    !world_texture_offset(b.uv_rate, surface_time, texture_offset))
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
        color.load_op = load == DrawLoad::Clear ? SDL_GPU_LOADOP_CLEAR : SDL_GPU_LOADOP_LOAD;
        color.store_op = SDL_GPU_STOREOP_STORE;
        SDL_GPUDepthStencilTargetInfo depth{};
        depth.texture = shared_depth ? shared_depth : depth_;
        depth.clear_depth = 1;
        depth.load_op = load == DrawLoad::Load ? SDL_GPU_LOADOP_LOAD : SDL_GPU_LOADOP_CLEAR;
        depth.store_op = SDL_GPU_STOREOP_STORE;
        depth.stencil_load_op = SDL_GPU_LOADOP_DONT_CARE;
        depth.stencil_store_op = SDL_GPU_STOREOP_DONT_CARE;
        depth.cycle = load != DrawLoad::Load;
        auto* pass = SDL_BeginGPURenderPass(commands, &color, 1, &depth);
        if (!pass)
        {
            error = SDL_GetError();
            return false;
        }
        SDL_GPUViewport viewport{0, float(top), float(width_), float(height), 0, 1};
        SDL_SetGPUViewport(pass, &viewport);
        SDL_BindGPUGraphicsPipeline(pass, sky ? sky_pipeline_ : (translucent ? alpha_pipeline_ : pipeline_));
        if (vertices_)
        {
            SDL_GPUBufferBinding binding{vertices_, 0};
            SDL_BindGPUVertexBuffers(pass, 0, &binding, 1);
            auto emit = [&](const WorldDraw& d, const WorldBatch* face)
            {
                const auto& r = mesh.ranges[d.range];
                if (!r.count)
                    return;
                world_view_uniform(eye, view, aspect, d, uniform);
                if (sky || load == DrawLoad::ClearDepth)
                {
                    constexpr float near = .01f, far = 65536;
                    uniform.projection[2] = far / (far - near);
                    uniform.projection[3] = -near * far / (far - near);
                }
                const bool illuminate = !sky && style_ == DrawStyle::Opaque && load != DrawLoad::ClearDepth;
                const std::array<float, 12> fragment{d.alpha,
                                                     0,
                                                     0,
                                                     0,
                                                     light.position.x,
                                                     light.position.y,
                                                     light.position.z,
                                                     illuminate ? light.radius : 0,
                                                     light.color.x,
                                                     light.color.y,
                                                     light.color.z,
                                                     0};
                SDL_PushGPUFragmentUniformData(commands, 0, fragment.data(), sizeof(fragment));
                auto batch =
                    [&](std::uint32_t first, std::uint32_t count, std::uint32_t material, std::array<double, 2> rate)
                {
                    world_texture_offset(rate, surface_time, uniform.texture_offset);
                    SDL_PushGPUVertexUniformData(commands, 0, &uniform, sizeof(uniform));
                    pan_batches += rate[0] != 0 || rate[1] != 0;
                    SDL_GPUTextureSamplerBinding texture{textures_[material], sampler_};
                    SDL_BindGPUFragmentSamplers(pass, 0, &texture, 1);
                    SDL_DrawGPUPrimitives(pass, count, 1, first, 0);
                };
                if (face)
                    batch(face->first, face->count, face->material, face->uv_rate);
                else if (r.batches.empty())
                {
                    if (sky)
                    {
                        SDL_BindGPUGraphicsPipeline(pass, sky_pipeline_);
                        ++stats.sky_batches;
                    }
                    batch(r.first, r.count, 0, {});
                }
                else
                    for (const auto& b : r.batches)
                    {
                        if (sky)
                        {
                            SDL_BindGPUGraphicsPipeline(pass, b.translucent ? sky_alpha_pipeline_ : sky_pipeline_);
                            batch(b.first, b.count, b.material, b.uv_rate);
                            ++stats.sky_batches;
                        }
                        else if ((style_ == DrawStyle::SpriteAlpha || !b.translucent) &&
                                 !(world_pass == WorldPass::OpaqueWithSky && b.sky_portal))
                            batch(b.first, b.count, b.material, b.uv_rate);
                    }
            };
            if (translucent)
                for (const auto& ordered : alpha_order_)
                {
                    const auto& d = draws[ordered.draw];
                    emit(d, &mesh.ranges[d.range].batches[ordered.batch]);
                }
            else
                for (const auto& d : draws)
                    emit(d, nullptr);
        }
        SDL_EndGPURenderPass(pass);
        if (sky)
            ++stats.sky_frames;
        else if (translucent)
        {
            ++stats.alpha_frames;
            stats.alpha_batches = alpha_order_.size();
            stats.alpha_pan_batches = pan_batches;
        }
        else
        {
            ++stats.frames;
            stats.pan_batches = pan_batches;
        }
        stats.surface_time = surface_time;
        return true;
    }
} // namespace OpenBlood2
