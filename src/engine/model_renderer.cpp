#include "engine/model_renderer.h"
#include "engine/collision.h"
#include <algorithm>
#include <cmath>
#include <cstring>

namespace OpenBlood2
{
    namespace
    {
        // Matches the skinned.vert std140 View block: the world uniform, then scale, color,
        // bases, ambient (w = source count) and three float4 per light source.
        struct SkinnedUniform
        {
            WorldViewUniform view;
            std::array<float, 4> scale{}, color{};
            std::array<std::uint32_t, 4> bases{};
            std::array<float, 4> ambient{};
            std::array<std::array<float, 4>, 3 * ModelLight::source_limit> sources{};
        };
        static_assert(sizeof(SkinnedUniform) == 384);
    } // namespace
    ModelRenderer::~ModelRenderer()
    {
        clear_geometry();
        registry_->release(palette_);
        registry_->release(deformation_);
    }
    void ModelRenderer::clear_geometry()
    {
        // Shared assets stay in the registry until their owners expire.
        instance_assets_.clear();
        textures_.clear();
        texture_paths_.clear();
        surface_materials_.clear();
        revision_ = UINT64_MAX;
        visible_.clear();
        stats.triangles = stats.draws = stats.alpha_batches = 0;
    }
    bool ModelRenderer::prepare(const ModelView& view, SDL_GPUTextureFormat format, Uint32 width, Uint32 height,
                                std::string& error)
    {
        error.clear();
        staged_bytes = 0;
        instance_assets_.clear();
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
        if (revision_ != view.revision)
        {
            if (!(sampler_ = registry_->sampler(style_ == DrawStyle::SpriteAlpha, error)) ||
                !(lightmap_sampler_ = registry_->lightmap_sampler(error)) ||
                !(shadow_sampler_ = registry_->sampler(true, error)) ||
                !registry_->refresh_textures(view.mesh.materials, textures_, texture_paths_, error))
            {
                revision_ = UINT64_MAX;
                return false;
            }
            revision_ = view.revision;
            ++stats.uploads;
        }
        if (enhanced() && !surface_materials_.refresh(*surface, *registry_, view.mesh.materials, error))
        {
            revision_ = UINT64_MAX;
            return false;
        }
        // Each asset's static corner stream is the same validated stream ModelView checked.
        asset_vertices = 0;
        instance_assets_.reserve(view.instances.size());
        for (const auto& instance : view.instances)
        {
            const std::weak_ptr<const void> owner = instance.owner;
            const auto* asset = registry_->shared(instance.model, owner);
            if (!asset)
            {
                const auto alive = instance.owner.lock();
                if (!alive || alive.get() != instance.model)
                {
                    error = "Skinned model asset expired before upload";
                    return false;
                }
                std::vector<SkinnedVertex> corners;
                if (!build_skinned_vertices(*instance.model, corners, error))
                    return false;
                if (registry_->stats.shared >= asset_limit || corners.size() > asset_vertex_limit)
                {
                    error = "GPU skinned asset budget exceeded";
                    return false;
                }
                GpuRegistry::Shared entry{{}, static_cast<std::uint32_t>(corners.size())};
                if (!corners.empty())
                {
                    const auto bytes = static_cast<Uint32>(corners.size() * sizeof(SkinnedVertex));
                    entry.buffer = registry_->create_buffer(SDL_GPU_BUFFERUSAGE_VERTEX, bytes, error);
                    if (!entry.buffer || !registry_->stage(entry.buffer, corners.data(), bytes, false, error))
                    {
                        registry_->release(entry.buffer);
                        return false;
                    }
                    staged_bytes += bytes;
                }
                asset = registry_->share(instance.model, owner, entry);
            }
            instance_assets_.push_back(*asset);
            asset_vertices += asset->count;
        }
        // Storage buffers always exist so the vertex stage binding is never empty.
        static const std::array<float, 4> zero{};
        const auto palette_bytes = static_cast<Uint32>(std::max<std::size_t>(1, view.palette.size()) * 16);
        const auto deformation_bytes = static_cast<Uint32>(std::max<std::size_t>(1, view.deformation.size()) * 16);
        if (!registry_->reserve(palette_, SDL_GPU_BUFFERUSAGE_GRAPHICS_STORAGE_READ, palette_bytes, error) ||
            !registry_->reserve(deformation_, SDL_GPU_BUFFERUSAGE_GRAPHICS_STORAGE_READ, deformation_bytes, error) ||
            !registry_->stage(palette_, view.palette.empty() ? zero.data() : view.palette.data()->data(), palette_bytes,
                              true, error) ||
            !registry_->stage(deformation_, view.deformation.empty() ? zero.data() : view.deformation.data()->data(),
                              deformation_bytes, true, error))
            return false;
        staged_bytes += palette_bytes + deformation_bytes;
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
        if (!pipelines_->get(WorldPipeline::SkinnedOpaque) || !registry_->get(palette_) ||
            !registry_->get(deformation_) || revision_ != view.revision || textures_.empty() ||
            !registry_->get(textures_[0]) || instance_assets_.size() != view.instances.size())
        {
            error = "Diagnostic renderer not prepared";
            return false;
        }
        const bool shaded = enhanced();
        if (shaded && (surface_materials_.uniforms.size() < textures_.size() ||
                       surface_materials_.normals.size() < textures_.size() || !surface_materials_.sampler ||
                       !registry_->get(surface->lights) || !registry_->get(surface->cells)))
        {
            error = "Enhanced surface materials not prepared";
            return false;
        }
        const Uint32 top = target.letterbox ? height_ / 6 : 0, height = height_ - 2 * top;
        const float aspect = float(width_) / float(height);
        const auto frustum =
            target.eye ? eye_frustum(eye, view_rotation, *target.eye) : view_frustum(eye, view_rotation, aspect);
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
            if ((instance_assets_[d.range].count && !registry_->get(instance_assets_[d.range].buffer)) ||
                instance.material >= textures_.size() || !registry_->get(textures_[instance.material]) ||
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
            const DVector delta = vector_subtract(instance.center, eye);
            visible_.push_back({i, double(delta.x) * frustum.forward.x + double(delta.y) * frustum.forward.y +
                                       double(delta.z) * frustum.forward.z});
        }
        if (translucent)
            std::stable_sort(visible_.begin(), visible_.end(),
                             [](const Visible& a, const Visible& b) { return a.depth > b.depth; });
        SDL_GPUViewport viewport{0, float(top), float(width_), float(height), target.min_depth, target.max_depth};
        SDL_SetGPUViewport(target.pass, &viewport);
        const bool blended = translucent || style_ == DrawStyle::SpriteAlpha;
        SDL_BindGPUGraphicsPipeline(target.pass,
                                    pipelines_->get(shaded    ? (blended ? WorldPipeline::SkinnedSurfaceAlpha
                                                                         : WorldPipeline::SkinnedSurfaceOpaque)
                                                    : blended ? WorldPipeline::SkinnedAlpha
                                                              : WorldPipeline::SkinnedOpaque));
        SDL_GPUBuffer* storage[] = {registry_->get(palette_), registry_->get(deformation_)};
        SDL_BindGPUVertexStorageBuffers(target.pass, 0, storage, 2);
        if (shaded)
            SurfaceMaterials::bind_scene(*surface, *registry_, target.commands, target.pass, eye);
        SurfaceMaterialUniform pushed_material;
        bool has_material = false;
        static const ShadowUniform no_shadows{}; // Models receive no model shadows.
        SDL_PushGPUFragmentUniformData(target.commands, 1, &no_shadows, sizeof(no_shadows));
        const bool illuminate = style_ == DrawStyle::Opaque && !close_view;
        const SDL_GPUBuffer* bound_buffer = nullptr;
        const SDL_GPUTexture* bound_texture = nullptr;
        std::size_t triangles = 0;
        for (const auto& entry : visible_)
        {
            const auto& d = draws[entry.draw];
            const auto& instance = view.instances[d.range];
            const auto& asset = instance_assets_[d.range];
            if (!asset.count)
                continue;
            SkinnedUniform skinned;
            world_view_uniform(eye, view_rotation, aspect, d, skinned.view);
            if (target.eye)
            {
                skinned.view.projection[0] = target.eye->scale_x;
                skinned.view.projection[1] = target.eye->scale_y;
                skinned.view.right[3] = target.eye->offset_x;
                skinned.view.up[3] = target.eye->offset_y;
            }
            else if (!close_view)
            {
                skinned.view.projection[0] *= target.view_scale;
                skinned.view.projection[1] *= target.view_scale;
            }
            if (close_view)
            {
                constexpr float near = .01f, far = 65536;
                skinned.view.projection[2] = far / (far - near);
                skinned.view.projection[3] = -near * far / (far - near);
            }
            skinned.scale = {instance.scale.x, instance.scale.y, instance.scale.z, instance.light.scale};
            skinned.color = {instance.color.x, instance.color.y, instance.color.z, 1};
            skinned.bases = {instance.palette, instance.deformation, 0, 0};
            const auto& model_light = instance.light;
            skinned.ambient = {model_light.ambient.x, model_light.ambient.y, model_light.ambient.z,
                               float(std::min<std::size_t>(model_light.count, ModelLight::source_limit))};
            for (std::size_t i = 0; i < ModelLight::source_limit; ++i)
            {
                const auto& source = model_light.sources[i];
                skinned.sources[i * 3] = {source.position.x, source.position.y, source.position.z, source.radius};
                skinned.sources[i * 3 + 1] = {source.color.x, source.color.y, source.color.z, source.cos_half};
                skinned.sources[i * 3 + 2] = {source.direction.x, source.direction.y, source.direction.z, 0};
            }
            SDL_PushGPUVertexUniformData(target.commands, 0, &skinned, sizeof(skinned));
            FragmentLightUniform fragment;
            fragment.scene_color = {target.scene_color.x, target.scene_color.y, target.scene_color.z, 1};
            if (target.effects && !close_view)
            {
                fragment.effects = *target.effects;
                for (auto& color : fragment.effects.colors)
                    if (static_cast<unsigned>(color[3]) & (1u << 5))
                        color = {}; // FLAG_ONLYLIGHTWORLD.
                if (!illuminate)
                    fragment.effects.light_info = {};
            }

            const bool pixel_light = per_pixel_lighting || shaded;
            fragment.modulation = {d.alpha, 0, 0, pixel_light ? 1.f : 0.f};
            fragment.position_radius = {light.position.x, light.position.y, light.position.z,
                                        illuminate ? light.radius : 0};
            fragment.light_color = {light.color.x, light.color.y, light.color.z, 0};
            if (pixel_light)
            {
                fragment.model_ambient = skinned.ambient;
                fragment.model_color = {skinned.color[0], skinned.color[1], skinned.color[2], skinned.scale[3]};
                std::copy(skinned.sources.begin(), skinned.sources.end(), fragment.model_sources.begin());
            }
            SDL_PushGPUFragmentUniformData(target.commands, 0, &fragment, sizeof(fragment));
            if (auto* buffer = registry_->get(asset.buffer); buffer != bound_buffer)
            {
                SDL_GPUBufferBinding binding{buffer, 0};
                SDL_BindGPUVertexBuffers(target.pass, 0, &binding, 1);
                bound_buffer = buffer;
            }
            if (shaded)
            {
                // Lit (Saturate N.L) instances take the physically based path; unlit, flat and
                // FLAG_NOLIGHT instances keep the original shading.
                auto parameters = surface_materials_.uniforms[instance.material];
                const bool lit = instance.light.scale > 1;
                parameters.material[3] = lit ? parameters.material[3] + 1 : 4;
                if (!has_material || !(parameters == pushed_material))
                {
                    SDL_PushGPUFragmentUniformData(target.commands, 3, &parameters, sizeof(parameters));
                    pushed_material = parameters;
                    has_material = true;
                }
            }
            if (auto* skin = registry_->get(textures_[instance.material]); skin != bound_texture)
            {
                // Models have no lightmap; slot 1 holds white to keep the binding valid.
                auto* white = registry_->get(textures_[0]);
                auto* normal = shaded ? registry_->get(surface_materials_.normals[instance.material]) : nullptr;
                const SDL_GPUTextureSamplerBinding textures[]{{skin, sampler_},
                                                              {white, lightmap_sampler_},
                                                              {white, shadow_sampler_},
                                                              {normal ? normal : white, surface_materials_.sampler}};
                SDL_BindGPUFragmentSamplers(target.pass, 0, textures, shaded ? 4 : 3);
                bound_texture = skin;
            }
            SDL_DrawGPUPrimitives(target.pass, asset.count, 1, 0, 0);
            ++stats.draw_calls;
            triangles += asset.count / 3;
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
    bool ModelRenderer::record_shadows(SDL_GPUCommandBuffer* commands, SDL_GPURenderPass* pass, const ModelView& view,
                                       std::span<const ShadowCaster> casters, std::string& error)
    {
        error.clear();
        auto* pipeline = pipelines_->get(WorldPipeline::ShadowCaster);
        if (!pipeline || instance_assets_.size() != view.instances.size())
        {
            error = "Model shadow renderer not prepared";
            return false;
        }
        for (const auto& caster : casters)
            if (caster.instance >= view.instances.size() || !physics_vector(caster.min) ||
                !physics_vector(caster.max) || !(caster.max.y > caster.min.y) ||
                !(caster.footprint[2] > caster.footprint[0]) || !(caster.footprint[3] > caster.footprint[1]) ||
                !std::isfinite(caster.slope[0]) || !std::isfinite(caster.slope[1]))
            {
                error = "Invalid model shadow caster";
                return false;
            }
        SDL_BindGPUGraphicsPipeline(pass, pipeline);
        SDL_GPUBuffer* storage[] = {registry_->get(palette_), registry_->get(deformation_)};
        SDL_BindGPUVertexStorageBuffers(pass, 0, storage, 2);
        const SDL_GPUBuffer* bound_buffer = nullptr;
        for (const auto& caster : casters)
        {
            const auto& instance = view.instances[caster.instance];
            const auto& asset = instance_assets_[caster.instance];
            if (!asset.count)
                continue;
            const SDL_GPUViewport viewport{
                caster.viewport[0], caster.viewport[1], caster.viewport[2], caster.viewport[3], 0, 1};
            SDL_SetGPUViewport(pass, &viewport);
            // Orthographic along the light: x/z sheared onto the box top, z up the tile, depth from
            // the box top down.
            SkinnedUniform skinned;
            const auto& f = caster.footprint;
            const float hx = (f[2] - f[0]) / 2, hz = (f[3] - f[1]) / 2;
            skinned.view.eye = {f[0] + hx, caster.max.y, f[1] + hz, 0};
            skinned.view.right = {1, caster.slope[0], 0, 0};
            skinned.view.up = {0, caster.slope[1], 1, 0};
            skinned.view.forward = {0, -1, 0, 1};
            skinned.view.projection = {1 / hx, 1 / hz, 1 / (caster.max.y - caster.min.y), 0};
            skinned.view.position = {caster.position.x, caster.position.y, caster.position.z, 0};
            skinned.view.quaternion = {caster.rotation.m_Vec.x, caster.rotation.m_Vec.y, caster.rotation.m_Vec.z,
                                       caster.rotation.m_Spin};
            skinned.scale = {instance.scale.x, instance.scale.y, instance.scale.z, 1};
            skinned.color = {1, 1, 1, 1};
            skinned.bases = {instance.palette, instance.deformation, 0, 0};
            skinned.ambient = {1, 1, 1, 0};
            SDL_PushGPUVertexUniformData(commands, 0, &skinned, sizeof(skinned));
            if (auto* buffer = registry_->get(asset.buffer); buffer != bound_buffer)
            {
                SDL_GPUBufferBinding binding{buffer, 0};
                SDL_BindGPUVertexBuffers(pass, 0, &binding, 1);
                bound_buffer = buffer;
            }
            SDL_DrawGPUPrimitives(pass, asset.count, 1, 0, 0);
            ++stats.draw_calls;
        }
        return true;
    }
} // namespace OpenBlood2
