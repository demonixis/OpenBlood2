#include "engine/world_renderer.h"
#include "engine/collision.h"
#include "engine/world_shaders.h"
#include <cmath>
#include <cstring>

namespace OpenBlood2
{
    namespace
    {
        bool in_frustum(const WorldRange& range, const WorldDraw& draw, const ViewFrustum& frustum)
        {
            if (!range.bounded)
                return true;
            const DVector local{(range.min.x + range.max.x) * .5f, (range.min.y + range.max.y) * .5f,
                                (range.min.z + range.max.z) * .5f};
            const double dx = range.max.x - range.min.x, dy = range.max.y - range.min.y, dz = range.max.z - range.min.z;
            return sphere_visible(frustum, vector_add(draw.position, rotate_vector(local, draw.rotation)),
                                  .5 * std::sqrt(dx * dx + dy * dy + dz * dz));
        }
    } // namespace
    ViewFrustum view_frustum(DVector eye, DRotation view, double aspect, double near, double far)
    {
        return {eye,
                rotate_vector({1, 0, 0}, view),
                rotate_vector({0, 1, 0}, view),
                rotate_vector({0, 0, 1}, view),
                aspect,
                near,
                far};
    }
    bool sphere_visible(const ViewFrustum& f, DVector center, double radius)
    {
        const DVector rel = vector_subtract(center, f.eye);
        auto dot = [](DVector a, DVector b) { return double(a.x) * b.x + double(a.y) * b.y + double(a.z) * b.z; };
        const double x = dot(rel, f.right), y = dot(rel, f.up), z = dot(rel, f.forward);
        const double side = radius * std::sqrt(2.0), vertical = radius * std::sqrt(1 + f.aspect * f.aspect);
        return std::isfinite(radius) && z + radius >= f.near && z - radius <= f.far && z - x >= -side &&
               z + x >= -side && z - f.aspect * y >= -vertical && z + f.aspect * y >= -vertical;
    }
    void WorldPipelines::release()
    {
        for (auto*& pipeline : pipelines_)
        {
            if (pipeline)
                SDL_ReleaseGPUGraphicsPipeline(device_, pipeline);
            pipeline = nullptr;
        }
        format_ = SDL_GPU_TEXTUREFORMAT_INVALID;
    }
    bool WorldPipelines::prepare(SDL_GPUTextureFormat format, std::string& error)
    {
        if (format_ == format && pipelines_[0])
            return true;
        release();
        SDL_GPUShaderCreateInfo vertex_info{}, skinned_info{}, opaque_info{}, alpha_info{};
        const auto formats = SDL_GetGPUShaderFormats(device_);
        if (!world_shader_info(WorldShader::Vertex, formats, vertex_info) ||
            !world_shader_info(WorldShader::SkinnedVertex, formats, skinned_info) ||
            !world_shader_info(WorldShader::OpaqueFragment, formats, opaque_info) ||
            !world_shader_info(WorldShader::AlphaFragment, formats, alpha_info))
        {
            error = "No bundled world shader for this GPU backend (SPIR-V or MSL required)";
            return false;
        }
        auto* vertex = SDL_CreateGPUShader(device_, &vertex_info);
        auto* skinned = vertex ? SDL_CreateGPUShader(device_, &skinned_info) : nullptr;
        auto* opaque = skinned ? SDL_CreateGPUShader(device_, &opaque_info) : nullptr;
        auto* alpha = opaque ? SDL_CreateGPUShader(device_, &alpha_info) : nullptr;
        bool success = alpha != nullptr;
        if (!success)
            error = SDL_GetError();
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
        SDL_GPUVertexBufferDescription skinned_buffer{};
        skinned_buffer.pitch = sizeof(SkinnedVertex);
        skinned_buffer.input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX;
        SDL_GPUVertexAttribute skinned_attributes[4]{};
        skinned_attributes[0].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3;
        skinned_attributes[0].offset = offsetof(SkinnedVertex, position);
        skinned_attributes[1].location = 1;
        skinned_attributes[1].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2;
        skinned_attributes[1].offset = offsetof(SkinnedVertex, uv);
        skinned_attributes[2].location = 2;
        skinned_attributes[2].format = SDL_GPU_VERTEXELEMENTFORMAT_UBYTE4;
        skinned_attributes[2].offset = offsetof(SkinnedVertex, nodes);
        skinned_attributes[3].location = 3;
        skinned_attributes[3].format = SDL_GPU_VERTEXELEMENTFORMAT_UINT;
        skinned_attributes[3].offset = offsetof(SkinnedVertex, deform);
        for (std::size_t i = 0; success && i < pipelines_.size(); ++i)
        {
            const auto kind = static_cast<WorldPipeline>(i);
            const bool blend =
                kind == WorldPipeline::Alpha || kind == WorldPipeline::SkyAlpha || kind == WorldPipeline::SkinnedAlpha;
            const bool sky = kind == WorldPipeline::Sky || kind == WorldPipeline::SkyAlpha;
            const bool skin = kind == WorldPipeline::SkinnedOpaque || kind == WorldPipeline::SkinnedAlpha;
            SDL_GPUColorTargetDescription target{};
            target.format = format;
            if (blend)
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
            create.vertex_shader = skin ? skinned : vertex;
            create.fragment_shader = blend ? alpha : opaque;
            create.vertex_input_state = skin ? SDL_GPUVertexInputState{&skinned_buffer, 1, skinned_attributes, 4}
                                             : SDL_GPUVertexInputState{&buffer, 1, attributes, 3};
            create.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
            create.rasterizer_state.fill_mode = SDL_GPU_FILLMODE_FILL;
            create.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_NONE;
            create.rasterizer_state.enable_depth_clip = true;
            create.multisample_state.sample_count = SDL_GPU_SAMPLECOUNT_1;
            create.depth_stencil_state.enable_depth_test = !sky;
            create.depth_stencil_state.enable_depth_write = !blend && !sky;
            create.depth_stencil_state.compare_op = SDL_GPU_COMPAREOP_LESS;
            create.target_info.color_target_descriptions = &target;
            create.target_info.num_color_targets = 1;
            create.target_info.has_depth_stencil_target = true;
            create.target_info.depth_stencil_format = SDL_GPU_TEXTUREFORMAT_D32_FLOAT;
            pipelines_[i] = SDL_CreateGPUGraphicsPipeline(device_, &create);
            if (!pipelines_[i])
            {
                error = SDL_GetError();
                success = false;
            }
        }
        for (auto* shader : {vertex, skinned, opaque, alpha})
            if (shader)
                SDL_ReleaseGPUShader(device_, shader);
        if (!success)
        {
            release();
            return false;
        }
        format_ = format;
        ++creations;
        return true;
    }
    WorldRenderer::~WorldRenderer()
    {
        clear_geometry();
        if (sampler_)
            SDL_ReleaseGPUSampler(device_, sampler_);
        if (depth_)
            SDL_ReleaseGPUTexture(device_, depth_);
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
        release_world_textures(device_, textures_);
        revision_ = UINT64_MAX;
        stats.triangles = stats.draws = stats.alpha_batches = stats.pan_batches = stats.alpha_pan_batches = 0;
        stats.surface_time = 0;
        stats.sky_batches = 0;
        stats.culled = 0;
        alpha_order_.clear();
        visible_.clear();
    }
    SDL_GPUCopyPass* begin_upload(SDL_GPUDevice* device, SDL_GPUCommandBuffer* frame, SDL_GPUCommandBuffer*& owned)
    {
        owned = frame ? nullptr : SDL_AcquireGPUCommandBuffer(device);
        auto* pass = frame || owned ? SDL_BeginGPUCopyPass(frame ? frame : owned) : nullptr;
        if (!pass && owned)
        {
            SDL_CancelGPUCommandBuffer(owned);
            owned = nullptr;
        }
        return pass;
    }
    bool end_upload(SDL_GPUCopyPass* pass, SDL_GPUCommandBuffer* owned)
    {
        SDL_EndGPUCopyPass(pass);
        return !owned || SDL_SubmitGPUCommandBuffer(owned);
    }
    bool WorldRenderer::prepare(const WorldMesh& mesh, std::uint64_t revision, SDL_GPUTextureFormat format,
                                Uint32 width, Uint32 height, std::string& error, bool dynamic,
                                SDL_GPUCommandBuffer* frame)
    {
        error.clear();
        if (!width || !height || width > 8192 || height > 8192 || mesh.vertices.size() > 3000000)
        {
            error = "Diagnostic GPU dimensions/geometry exceed limits";
            return false;
        }
        if (!pipelines_->prepare(format, error))
            return false;
        if (!external_targets_ && (!depth_ || width_ != width || height_ != height))
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
        }
        width_ = width;
        height_ = height;
        if (revision_ == revision)
            return !dynamic || upload_vertices(mesh, error, frame);
        clear_geometry();
        if (!upload_vertices(mesh, error, frame))
            return false;
        if (!upload_materials(mesh, error, frame))
        {
            clear_geometry();
            return false;
        }
        revision_ = revision;
        ++stats.uploads;
        return true;
    }
    bool WorldRenderer::upload_vertices(const WorldMesh& mesh, std::string& error, SDL_GPUCommandBuffer* frame)
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
            SDL_GPUCommandBuffer* owned = nullptr;
            if (auto* copy = begin_upload(device_, frame, owned))
            {
                SDL_GPUTransferBufferLocation source{vertex_transfer_, 0};
                SDL_GPUBufferRegion target{vertices_, 0, bytes};
                SDL_UploadToGPUBuffer(copy, &source, &target, true);
                success = end_upload(copy, owned);
            }
        }
        if (!success)
            error = SDL_GetError();
        return success;
    }
    SDL_GPUSampler* create_world_sampler(SDL_GPUDevice* device, bool clamp)
    {
        SDL_GPUSamplerCreateInfo info{};
        info.min_filter = info.mag_filter = SDL_GPU_FILTER_NEAREST;
        info.mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_NEAREST;
        info.max_lod = 1000; // Authored DTX levels; point sampling within each level.
        info.address_mode_u = info.address_mode_v = info.address_mode_w =
            clamp ? SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE : SDL_GPU_SAMPLERADDRESSMODE_REPEAT;
        return SDL_CreateGPUSampler(device, &info);
    }
    void release_world_textures(SDL_GPUDevice* device, std::vector<SDL_GPUTexture*>& textures)
    {
        for (auto* texture : textures)
            if (texture)
                SDL_ReleaseGPUTexture(device, texture);
        textures.clear();
    }
    bool upload_world_textures(SDL_GPUDevice* device, const std::vector<WorldMaterial>& materials,
                               std::vector<SDL_GPUTexture*>& textures, std::string& error, SDL_GPUCommandBuffer* frame)
    {
        release_world_textures(device, textures);
        const Texture white{1, 1, 1, 0, 0, {255, 255, 255, 255}, {}};
        const auto count = std::max(std::size_t(1), materials.size());
        if (count > 4097)
        {
            error = "GPU texture count limit exceeded";
            return false;
        }
        std::vector<const Texture*> images(count, &white);
        std::size_t bytes = 0;
        for (std::size_t i = 0; i < count; ++i)
        {
            if (!materials.empty())
                images[i] = materials[i].error.empty() && materials[i].frames.empty() ? &materials[i].image : nullptr;
            if (const auto* image = images[i])
            {
                const auto size = image->rgba.size() + image->mip_rgba.size();
                if (!image->width || !image->height || image->width > 1024 || image->height > 1024 ||
                    image->mipmaps > 11 || image->rgba.size() != std::size_t(image->width) * image->height * 4 ||
                    image->mip_rgba.size() != texture_mip_bytes(*image) || size > 128 * 1024 * 1024 - bytes)
                {
                    error = "Invalid GPU texture dimensions/payload/budget";
                    return false;
                }
                bytes += size;
            }
        }
        if (!images[0])
        {
            error = "Missing diagnostic GPU texture";
            return false;
        }
        SDL_GPUTransferBufferCreateInfo transfer_info{SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD, static_cast<Uint32>(bytes),
                                                      0};
        auto* transfer = SDL_CreateGPUTransferBuffer(device, &transfer_info);
        if (!transfer)
        {
            error = SDL_GetError();
            return false;
        }
        textures.resize(count);
        bool success = false;
        if (auto* mapped = static_cast<Uint8*>(SDL_MapGPUTransferBuffer(device, transfer, false)))
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
                    info.num_levels = std::max(1u, image->mipmaps);
                    textures[i] = SDL_CreateGPUTexture(device, &info);
                    if (!textures[i])
                    {
                        allocated = false;
                        break;
                    }
                    std::memcpy(mapped + offset, image->rgba.data(), image->rgba.size());
                    offset += image->rgba.size();
                    if (!image->mip_rgba.empty())
                        std::memcpy(mapped + offset, image->mip_rgba.data(), image->mip_rgba.size());
                    offset += image->mip_rgba.size();
                }
            }
            SDL_UnmapGPUTransferBuffer(device, transfer);
            SDL_GPUCommandBuffer* owned = nullptr;
            if (allocated)
                if (auto* copy = begin_upload(device, frame, owned))
                {
                    {
                        Uint32 offset = 0;
                        for (std::size_t i = 0; i < count; ++i)
                        {
                            if (const auto* image = images[i])
                                for (Uint32 level = 0, w = image->width, h = image->height;
                                     level < std::max(1u, image->mipmaps);
                                     ++level, w = std::max(1u, w / 2), h = std::max(1u, h / 2))
                                {
                                    SDL_GPUTextureTransferInfo source{};
                                    source.transfer_buffer = transfer;
                                    source.offset = offset;
                                    source.pixels_per_row = w;
                                    source.rows_per_layer = h;
                                    SDL_GPUTextureRegion target{};
                                    target.texture = textures[i];
                                    target.mip_level = level;
                                    target.w = w;
                                    target.h = h;
                                    target.d = 1;
                                    SDL_UploadToGPUTexture(copy, &source, &target, false);
                                    offset += w * h * 4;
                                }
                        }
                    }
                    success = end_upload(copy, owned);
                }
        }
        SDL_ReleaseGPUTransferBuffer(device, transfer);
        if (!success)
            error = SDL_GetError();
        return success;
    }
    bool WorldRenderer::upload_materials(const WorldMesh& mesh, std::string& error, SDL_GPUCommandBuffer* frame)
    {
        if (!sampler_)
        {
            sampler_ = create_world_sampler(device_, style_ == DrawStyle::SpriteAlpha);
            if (!sampler_)
            {
                error = SDL_GetError();
                return false;
            }
        }
        return upload_world_textures(device_, mesh.materials, textures_, error, frame);
    }
    bool WorldRenderer::validate(const WorldMesh& mesh, std::span<const WorldDraw> draws, DVector eye, DRotation view,
                                 WorldPass world_pass, double surface_time, const PointLight& light, bool letterbox,
                                 bool close_view, std::string& error)
    {
        visible_.clear();
        alpha_order_.clear();
        if (!physics_vector(light.position) || !physics_vector(light.color) || !std::isfinite(light.radius) ||
            light.radius < 0 || light.radius > 1e7 || light.color.x < 0 || light.color.y < 0 || light.color.z < 0 ||
            light.color.x > 1 || light.color.y > 1 || light.color.z > 1)
        {
            error = "Invalid point light";
            return false;
        }
        if (!pipelines_->get(WorldPipeline::Opaque) || (!external_targets_ && !depth_) ||
            (!vertices_ && !mesh.vertices.empty()))
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
        const bool translucent = world_pass == WorldPass::Translucent;
        const bool sky = world_pass == WorldPass::Sky;
        if (sky && style_ != DrawStyle::Opaque)
        {
            error = "Sky requires an opaque renderer and clear pass";
            return false;
        }
        if (translucent && style_ != DrawStyle::Opaque)
        {
            error = "Transparent world pass requires loaded opaque depth";
            return false;
        }
        const Uint32 top = letterbox ? height_ / 6 : 0;
        const float aspect = float(width_) / float(height_ - 2 * top);
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
                std::uint32_t image = 0;
                if (!world_material_frame(mesh, b.material, surface_time, image) || image >= textures_.size() ||
                    !textures_[image] || b.first < r.first || b.first > std::size_t(r.first) + r.count ||
                    b.count > std::size_t(r.first) + r.count - b.first ||
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
        // Visibility: sky and view-model draws keep their dedicated projections and are never culled.
        if (sky || close_view)
            visible_.assign(draws.begin(), draws.end());
        else
        {
            const auto frustum = view_frustum(eye, view, aspect);
            for (const auto& d : draws)
            {
                if (in_frustum(mesh.ranges[d.range], d, frustum))
                    visible_.push_back(d);
                else
                    ++stats.culled;
            }
        }
        if (translucent && !world_alpha_order(mesh, visible_, eye, view, alpha_order_, error))
        {
            visible_.clear();
            return false;
        }
        return true;
    }
    bool WorldRenderer::record(const WorldRecordTarget& target, const WorldMesh& mesh, std::span<const WorldDraw> draws,
                               DVector eye, DRotation view, std::string& error, WorldPass world_pass,
                               double surface_time, const PointLight& light, bool close_view)
    {
        error.clear();
        if (!target.commands || !target.pass || !std::isfinite(target.min_depth) || !std::isfinite(target.max_depth) ||
            target.min_depth < 0 || target.min_depth >= target.max_depth || target.max_depth > 1)
        {
            error = "Invalid world record target";
            return false;
        }
        if (!validate(mesh, draws, eye, view, world_pass, surface_time, light, target.letterbox, close_view, error))
            return false;
        encode(target, mesh, eye, view, world_pass, surface_time, light, close_view);
        return true;
    }
    bool WorldRenderer::draw(SDL_GPUCommandBuffer* commands, SDL_GPUTexture* target, const WorldMesh& mesh,
                             std::span<const WorldDraw> draws, DVector eye, DRotation view, bool letterbox,
                             std::string& error, DrawLoad load, SDL_GPUTexture* shared_depth, WorldPass world_pass,
                             double surface_time, const PointLight& light)
    {
        error.clear();
        if (world_pass == WorldPass::Sky && load != DrawLoad::Clear)
        {
            error = "Sky requires an opaque renderer and clear pass";
            return false;
        }
        if (world_pass == WorldPass::Translucent && style_ == DrawStyle::Opaque && load != DrawLoad::Load)
        {
            error = "Transparent world pass requires loaded opaque depth";
            return false;
        }
        if (!shared_depth && !depth_)
        {
            error = "Diagnostic renderer not prepared";
            return false;
        }
        if (!validate(mesh, draws, eye, view, world_pass, surface_time, light, letterbox, load == DrawLoad::ClearDepth,
                      error))
            return false;
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
        encode({commands, pass, letterbox}, mesh, eye, view, world_pass, surface_time, light,
               load == DrawLoad::ClearDepth);
        SDL_EndGPURenderPass(pass);
        return true;
    }
    void WorldRenderer::encode(const WorldRecordTarget& target, const WorldMesh& mesh, DVector eye, DRotation view,
                               WorldPass world_pass, double surface_time, const PointLight& light, bool close_view)
    {
        const bool translucent = world_pass == WorldPass::Translucent;
        const bool sky = world_pass == WorldPass::Sky;
        if (sky || world_pass == WorldPass::Opaque)
            stats.sky_batches = 0;
        const Uint32 top = target.letterbox ? height_ / 6 : 0, height = height_ - 2 * top;
        const float aspect = float(width_) / float(height);
        std::size_t pan_batches = 0;
        SDL_GPUViewport viewport{0, float(top), float(width_), float(height), target.min_depth, target.max_depth};
        SDL_SetGPUViewport(target.pass, &viewport);
        auto* bound = pipelines_->get(
            sky ? WorldPipeline::Sky
                : (translucent || style_ == DrawStyle::SpriteAlpha ? WorldPipeline::Alpha : WorldPipeline::Opaque));
        SDL_BindGPUGraphicsPipeline(target.pass, bound);
        if (vertices_)
        {
            SDL_GPUBufferBinding binding{vertices_, 0};
            SDL_BindGPUVertexBuffers(target.pass, 0, &binding, 1);
            // Redundant state is skipped within this record; another renderer may have
            // pushed uniforms or bound samplers in between, so nothing is assumed on entry.
            SDL_GPUTexture* bound_texture = nullptr;
            WorldViewUniform uniform, pushed;
            bool has_pushed = false;
            auto bind = [&](SDL_GPUGraphicsPipeline* pipeline)
            {
                if (pipeline == bound)
                    return;
                SDL_BindGPUGraphicsPipeline(target.pass, pipeline);
                bound = pipeline;
                bound_texture = nullptr;
                has_pushed = false;
            };
            auto emit = [&](const WorldDraw& d, const WorldBatch* face)
            {
                const auto& r = mesh.ranges[d.range];
                if (!r.count)
                    return;
                world_view_uniform(eye, view, aspect, d, uniform);
                if (sky || close_view)
                {
                    constexpr float near = .01f, far = 65536;
                    uniform.projection[2] = far / (far - near);
                    uniform.projection[3] = -near * far / (far - near);
                }
                const bool illuminate = !sky && style_ == DrawStyle::Opaque && !close_view;
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
                SDL_PushGPUFragmentUniformData(target.commands, 0, fragment.data(), sizeof(fragment));
                auto batch =
                    [&](std::uint32_t first, std::uint32_t count, std::uint32_t material, std::array<double, 2> rate)
                {
                    world_texture_offset(rate, surface_time, uniform.texture_offset);
                    if (!has_pushed || std::memcmp(&pushed, &uniform, sizeof(uniform)) != 0)
                    {
                        SDL_PushGPUVertexUniformData(target.commands, 0, &uniform, sizeof(uniform));
                        pushed = uniform;
                        has_pushed = true;
                    }
                    pan_batches += rate[0] != 0 || rate[1] != 0;
                    std::uint32_t image = material;
                    world_material_frame(mesh, material, surface_time, image); // Validated before recording.
                    if (textures_[image] != bound_texture)
                    {
                        SDL_GPUTextureSamplerBinding texture{textures_[image], sampler_};
                        SDL_BindGPUFragmentSamplers(target.pass, 0, &texture, 1);
                        bound_texture = textures_[image];
                    }
                    SDL_DrawGPUPrimitives(target.pass, count, 1, first, 0);
                    ++stats.draw_calls;
                };
                if (face)
                    batch(face->first, face->count, face->material, face->uv_rate);
                else if (r.batches.empty())
                {
                    if (sky)
                    {
                        bind(pipelines_->get(WorldPipeline::Sky));
                        ++stats.sky_batches;
                    }
                    batch(r.first, r.count, 0, {});
                }
                else
                    for (const auto& b : r.batches)
                    {
                        if (sky)
                        {
                            bind(pipelines_->get(b.translucent ? WorldPipeline::SkyAlpha : WorldPipeline::Sky));
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
                    const auto& d = visible_[ordered.draw];
                    emit(d, &mesh.ranges[d.range].batches[ordered.batch]);
                }
            else
                for (const auto& d : visible_)
                    emit(d, nullptr);
        }
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
    }
} // namespace OpenBlood2
