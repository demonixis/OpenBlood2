#include "engine/model_renderer.h"
#include "engine/collision.h"
#include <algorithm>
#include <cmath>
#include <cstring>

namespace OpenBlood2
{
    namespace
    {
        // Matches the skinned.vert std140 View block: the world uniform, then scale, color and bases.
        struct SkinnedUniform
        {
            WorldViewUniform view;
            std::array<float, 4> scale{}, color{};
            std::array<std::uint32_t, 4> bases{};
        };
        static_assert(sizeof(SkinnedUniform) == 176);
    } // namespace
    ModelRenderer::~ModelRenderer()
    {
        clear_geometry();
        for (auto* buffer : {palette_, deformation_})
            if (buffer)
                SDL_ReleaseGPUBuffer(device_, buffer);
        if (transfer_)
            SDL_ReleaseGPUTransferBuffer(device_, transfer_);
        if (sampler_)
            SDL_ReleaseGPUSampler(device_, sampler_);
    }
    void ModelRenderer::release_asset(Asset& asset)
    {
        if (asset.buffer)
            SDL_ReleaseGPUBuffer(device_, asset.buffer);
        asset_vertices_ -= asset.vertices;
        asset = {};
    }
    void ModelRenderer::clear_geometry()
    {
        for (auto& [model, asset] : assets_)
            release_asset(asset);
        assets_.clear();
        asset_vertices_ = 0;
        release_world_textures(device_, textures_);
        revision_ = UINT64_MAX;
        visible_.clear();
        stats.triangles = stats.draws = stats.alpha_batches = 0;
    }
    bool ModelRenderer::reserve(SDL_GPUBuffer*& buffer, Uint32& capacity, Uint32 bytes, std::string& error)
    {
        if (bytes <= capacity && buffer)
            return true;
        const Uint32 size = std::max<Uint32>(bytes, capacity + capacity / 2);
        SDL_GPUBufferCreateInfo info{SDL_GPU_BUFFERUSAGE_GRAPHICS_STORAGE_READ, size, 0};
        auto* next = SDL_CreateGPUBuffer(device_, &info);
        if (!next)
        {
            error = SDL_GetError();
            return false;
        }
        if (buffer)
            SDL_ReleaseGPUBuffer(device_, buffer);
        buffer = next;
        capacity = size;
        return true;
    }
    bool ModelRenderer::prepare(const ModelView& view, SDL_GPUTextureFormat format, Uint32 width, Uint32 height,
                                std::string& error, SDL_GPUCommandBuffer* frame)
    {
        error.clear();
        uploaded_bytes = 0;
        if (!width || !height || width > 8192 || height > 8192)
        {
            error = "Diagnostic GPU dimensions/geometry exceed limits";
            return false;
        }
        if (!view.gpu_skinning)
        {
            error = "Model renderer requires a GPU-skinning model view";
            return false;
        }
        if (!pipelines_->prepare(format, error))
            return false;
        width_ = width;
        height_ = height;
        for (auto it = assets_.begin(); it != assets_.end();)
        {
            if (it->second.owner.expired())
            {
                release_asset(it->second);
                it = assets_.erase(it);
            }
            else
                ++it;
        }
        if (revision_ != view.revision)
        {
            if (!sampler_ && !(sampler_ = create_world_sampler(device_, style_ == DrawStyle::SpriteAlpha)))
            {
                error = SDL_GetError();
                return false;
            }
            if (!upload_world_textures(device_, view.mesh.materials, textures_, error, frame))
            {
                release_world_textures(device_, textures_);
                revision_ = UINT64_MAX;
                return false;
            }
            revision_ = view.revision;
            ++stats.uploads;
        }
        // New immutable assets: the same validated corner stream ModelView checked.
        struct Copy
        {
            SDL_GPUBuffer* buffer;
            const void* data;
            Uint32 bytes;
            bool cycle;
        };
        std::vector<Copy> copies;
        std::vector<const Model*> created;
        pending_.clear();
        // A failed preparation must not leave a created but never uploaded asset behind.
        auto fail = [&]()
        {
            for (const auto* model : created)
                if (auto found = assets_.find(model); found != assets_.end())
                {
                    release_asset(found->second);
                    assets_.erase(found);
                }
            pending_.clear();
            return false;
        };
        for (const auto& instance : view.instances)
        {
            auto found = assets_.find(instance.model);
            if (found != assets_.end() && !found->second.owner.owner_before(instance.owner) &&
                !instance.owner.owner_before(found->second.owner))
                continue;
            if (found != assets_.end())
            {
                release_asset(found->second);
                assets_.erase(found);
            }
            const auto owner = instance.owner.lock();
            if (!owner || owner.get() != instance.model)
            {
                error = "Skinned model asset expired before upload";
                return fail();
            }
            std::vector<SkinnedVertex> corners;
            if (!build_skinned_vertices(*instance.model, corners, error))
                return fail();
            if (assets_.size() >= asset_limit || corners.size() > asset_vertex_limit - asset_vertices_)
            {
                error = "GPU skinned asset budget exceeded";
                return fail();
            }
            Asset asset;
            asset.owner = instance.owner;
            asset.vertices = static_cast<std::uint32_t>(corners.size());
            if (!corners.empty())
            {
                const auto bytes = static_cast<Uint32>(corners.size() * sizeof(SkinnedVertex));
                SDL_GPUBufferCreateInfo info{SDL_GPU_BUFFERUSAGE_VERTEX, bytes, 0};
                asset.buffer = SDL_CreateGPUBuffer(device_, &info);
                if (!asset.buffer)
                {
                    error = SDL_GetError();
                    return fail();
                }
                pending_.push_back(std::move(corners));
                copies.push_back({asset.buffer, pending_.back().data(), bytes, false});
            }
            asset_vertices_ += asset.vertices;
            assets_.emplace(instance.model, std::move(asset));
            created.push_back(instance.model);
        }
        // Storage buffers always exist so the vertex stage binding is never empty.
        const auto palette_bytes = static_cast<Uint32>(std::max<std::size_t>(1, view.palette.size()) * 16);
        const auto deformation_bytes = static_cast<Uint32>(std::max<std::size_t>(1, view.deformation.size()) * 16);
        static const std::array<float, 4> zero{};
        if (!reserve(palette_, palette_capacity_, palette_bytes, error) ||
            !reserve(deformation_, deformation_capacity_, deformation_bytes, error))
            return fail();
        copies.push_back(
            {palette_, view.palette.empty() ? zero.data() : view.palette.data()->data(), palette_bytes, true});
        copies.push_back({deformation_, view.deformation.empty() ? zero.data() : view.deformation.data()->data(),
                          deformation_bytes, true});
        Uint32 total = 0;
        for (const auto& copy : copies)
            total += copy.bytes;
        if (total > transfer_capacity_)
        {
            SDL_GPUTransferBufferCreateInfo info{SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
                                                 std::max<Uint32>(total, transfer_capacity_ + transfer_capacity_ / 2),
                                                 0};
            auto* next = SDL_CreateGPUTransferBuffer(device_, &info);
            if (!next)
            {
                error = SDL_GetError();
                return fail();
            }
            if (transfer_)
                SDL_ReleaseGPUTransferBuffer(device_, transfer_);
            transfer_ = next;
            transfer_capacity_ = info.size;
        }
        auto* mapped = static_cast<Uint8*>(SDL_MapGPUTransferBuffer(device_, transfer_, true));
        if (!mapped)
        {
            error = SDL_GetError();
            return fail();
        }
        Uint32 offset = 0;
        for (const auto& copy : copies)
        {
            std::memcpy(mapped + offset, copy.data, copy.bytes);
            offset += copy.bytes;
        }
        SDL_UnmapGPUTransferBuffer(device_, transfer_);
        SDL_GPUCommandBuffer* owned = nullptr;
        auto* pass = begin_upload(device_, frame, owned);
        if (!pass)
        {
            error = SDL_GetError();
            return fail();
        }
        offset = 0;
        for (const auto& copy : copies)
        {
            SDL_GPUTransferBufferLocation source{transfer_, offset};
            SDL_GPUBufferRegion target{copy.buffer, 0, copy.bytes};
            SDL_UploadToGPUBuffer(pass, &source, &target, copy.cycle);
            offset += copy.bytes;
        }
        if (!end_upload(pass, owned))
        {
            error = SDL_GetError();
            return fail();
        }
        uploaded_bytes = total;
        pending_.clear();
        return true;
    }
    bool ModelRenderer::record(const WorldRecordTarget& target, const ModelView& view, std::span<const WorldDraw> draws,
                               DVector eye, DRotation view_rotation, std::string& error, WorldPass world_pass,
                               const PointLight& light, bool close_view)
    {
        error.clear();
        visible_.clear();
        if (!target.commands || !target.pass || !std::isfinite(target.min_depth) || !std::isfinite(target.max_depth) ||
            target.min_depth < 0 || target.min_depth >= target.max_depth || target.max_depth > 1)
        {
            error = "Invalid world record target";
            return false;
        }
        if (!physics_vector(light.position) || !physics_vector(light.color) || !std::isfinite(light.radius) ||
            light.radius < 0 || light.radius > 1e7 || light.color.x < 0 || light.color.y < 0 || light.color.z < 0 ||
            light.color.x > 1 || light.color.y > 1 || light.color.z > 1)
        {
            error = "Invalid point light";
            return false;
        }
        const bool translucent = world_pass == WorldPass::Translucent;
        if (world_pass == WorldPass::Sky || (translucent && style_ != DrawStyle::Opaque))
        {
            error = "Model renderer supports opaque and translucent model passes only";
            return false;
        }
        if (!pipelines_->get(WorldPipeline::SkinnedOpaque) || !palette_ || !deformation_ ||
            revision_ != view.revision || textures_.empty() || !textures_[0])
        {
            error = "Diagnostic renderer not prepared";
            return false;
        }
        const Uint32 top = target.letterbox ? height_ / 6 : 0, height = height_ - 2 * top;
        const float aspect = float(width_) / float(height);
        const auto frustum = view_frustum(eye, view_rotation, aspect);
        WorldViewUniform uniform;
        for (std::size_t i = 0; i < draws.size(); ++i)
        {
            const auto& d = draws[i];
            if (!std::isfinite(d.alpha) || d.alpha < 0 || d.alpha > 1)
            {
                error = "Invalid sprite draw alpha";
                return false;
            }
            if (d.range >= view.instances.size() || !world_view_uniform(eye, view_rotation, aspect, d, uniform))
            {
                error = "Invalid diagnostic draw/camera";
                return false;
            }
            const auto& instance = view.instances[d.range];
            const auto asset = assets_.find(instance.model);
            if (asset == assets_.end() || instance.material >= textures_.size() || !textures_[instance.material] ||
                instance.palette + instance.model->nodes.size() * 2 > view.palette.size() ||
                instance.deformation > view.deformation.size())
            {
                error = "Invalid diagnostic GPU material batch";
                return false;
            }
            if (style_ == DrawStyle::Opaque && instance.translucent != translucent)
                continue;
            if (!close_view && instance.bounded)
            {
                const DVector local{(instance.min.x + instance.max.x) * .5f, (instance.min.y + instance.max.y) * .5f,
                                    (instance.min.z + instance.max.z) * .5f};
                const double dx = instance.max.x - instance.min.x, dy = instance.max.y - instance.min.y,
                             dz = instance.max.z - instance.min.z;
                if (!sphere_visible(frustum, vector_add(d.position, rotate_vector(local, d.rotation)),
                                    .5 * std::sqrt(dx * dx + dy * dy + dz * dz)))
                {
                    ++stats.culled;
                    continue;
                }
            }
            const DVector delta = vector_subtract(d.position, eye);
            visible_.push_back({i, double(delta.x) * frustum.forward.x + double(delta.y) * frustum.forward.y +
                                       double(delta.z) * frustum.forward.z});
        }
        if (translucent)
            std::stable_sort(visible_.begin(), visible_.end(),
                             [](const Visible& a, const Visible& b) { return a.depth > b.depth; });
        SDL_GPUViewport viewport{0, float(top), float(width_), float(height), target.min_depth, target.max_depth};
        SDL_SetGPUViewport(target.pass, &viewport);
        SDL_BindGPUGraphicsPipeline(target.pass, pipelines_->get(translucent || style_ == DrawStyle::SpriteAlpha
                                                                     ? WorldPipeline::SkinnedAlpha
                                                                     : WorldPipeline::SkinnedOpaque));
        SDL_GPUBuffer* storage[] = {palette_, deformation_};
        SDL_BindGPUVertexStorageBuffers(target.pass, 0, storage, 2);
        const bool illuminate = style_ == DrawStyle::Opaque && !close_view;
        const SDL_GPUBuffer* bound_buffer = nullptr;
        const SDL_GPUTexture* bound_texture = nullptr;
        std::size_t triangles = 0;
        for (const auto& entry : visible_)
        {
            const auto& d = draws[entry.draw];
            const auto& instance = view.instances[d.range];
            const auto& asset = assets_.at(instance.model);
            if (!asset.vertices)
                continue;
            SkinnedUniform skinned;
            world_view_uniform(eye, view_rotation, aspect, d, skinned.view);
            if (close_view)
            {
                constexpr float near = .01f, far = 65536;
                skinned.view.projection[2] = far / (far - near);
                skinned.view.projection[3] = -near * far / (far - near);
            }
            skinned.scale = {instance.scale.x, instance.scale.y, instance.scale.z, 0};
            skinned.color = {instance.color.x, instance.color.y, instance.color.z, 1};
            skinned.bases = {instance.palette, instance.deformation, 0, 0};
            SDL_PushGPUVertexUniformData(target.commands, 0, &skinned, sizeof(skinned));
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
            if (asset.buffer != bound_buffer)
            {
                SDL_GPUBufferBinding binding{asset.buffer, 0};
                SDL_BindGPUVertexBuffers(target.pass, 0, &binding, 1);
                bound_buffer = asset.buffer;
            }
            if (textures_[instance.material] != bound_texture)
            {
                SDL_GPUTextureSamplerBinding texture{textures_[instance.material], sampler_};
                SDL_BindGPUFragmentSamplers(target.pass, 0, &texture, 1);
                bound_texture = textures_[instance.material];
            }
            SDL_DrawGPUPrimitives(target.pass, asset.vertices, 1, 0, 0);
            ++stats.draw_calls;
            triangles += asset.vertices / 3;
        }
        if (translucent)
        {
            ++stats.alpha_frames;
            stats.alpha_batches = visible_.size();
        }
        else
        {
            ++stats.frames;
            stats.draws = visible_.size();
            stats.triangles = triangles;
        }
        return true;
    }
} // namespace OpenBlood2
