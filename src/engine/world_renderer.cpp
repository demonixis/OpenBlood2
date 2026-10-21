#include "engine/world_renderer.h"
#include "engine/collision.h"
#include "engine/surface_report.h"
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
    ViewFrustum eye_frustum(DVector eye, DRotation view, const EyeProjection& p, double near, double far)
    {
        auto f = view_frustum(eye, view, 1, near, far);
        f.asymmetric = true;
        f.tan_left = p.tan_left;
        f.tan_right = p.tan_right;
        f.tan_down = p.tan_down;
        f.tan_up = p.tan_up;
        return f;
    }
    bool sphere_visible(const ViewFrustum& f, DVector center, double radius)
    {
        const DVector rel = vector_subtract(center, f.eye);
        auto dot = [](DVector a, DVector b) { return double(a.x) * b.x + double(a.y) * b.y + double(a.z) * b.z; };
        const double x = dot(rel, f.right), y = dot(rel, f.up), z = dot(rel, f.forward);
        if (f.asymmetric)
        {
            // Each side plane passes through the eye at its tangent: x = t z (likewise y).
            const auto margin = [radius](double t) { return -radius * std::sqrt(1 + t * t); };
            return std::isfinite(radius) && z + radius >= f.near && z - radius <= f.far &&
                   x - f.tan_left * z >= margin(f.tan_left) && f.tan_right * z - x >= margin(f.tan_right) &&
                   y - f.tan_down * z >= margin(f.tan_down) && f.tan_up * z - y >= margin(f.tan_up);
        }
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
        return prepare(format, pipelines_[0] ? shading_ : SurfaceShading::Original, error);
    }
    bool WorldPipelines::prepare(SDL_GPUTextureFormat format, SurfaceShading shading, std::string& error)
    {
        if (format_ == format && shading_ == shading && pipelines_[0])
            return true;
        release();
        const bool enhanced = shading != SurfaceShading::Original;
        const bool high = shading == SurfaceShading::High;
        SDL_GPUShaderCreateInfo vertex_info{}, skinned_info{}, opaque_info{}, alpha_info{}, shadow_info{},
            surface_info{}, surface_alpha_info{};
        const auto formats = SDL_GetGPUShaderFormats(device_);
        if (!world_shader_info(WorldShader::Vertex, formats, vertex_info) ||
            !world_shader_info(WorldShader::SkinnedVertex, formats, skinned_info) ||
            !world_shader_info(WorldShader::OpaqueFragment, formats, opaque_info) ||
            !world_shader_info(WorldShader::AlphaFragment, formats, alpha_info) ||
            !world_shader_info(WorldShader::ShadowFragment, formats, shadow_info) ||
            (enhanced &&
             (!world_shader_info(high ? WorldShader::PbrHighFragment : WorldShader::PbrFragment, formats,
                                 surface_info) ||
              !world_shader_info(high ? WorldShader::PbrHighAlphaFragment : WorldShader::PbrAlphaFragment, formats,
                                 surface_alpha_info))))
        {
            error = "No bundled world shader for this GPU backend (SPIR-V or MSL required)";
            return false;
        }
        auto* vertex = SDL_CreateGPUShader(device_, &vertex_info);
        auto* skinned = vertex ? SDL_CreateGPUShader(device_, &skinned_info) : nullptr;
        auto* opaque = skinned ? SDL_CreateGPUShader(device_, &opaque_info) : nullptr;
        auto* alpha = opaque ? SDL_CreateGPUShader(device_, &alpha_info) : nullptr;
        auto* shadow = alpha ? SDL_CreateGPUShader(device_, &shadow_info) : nullptr;
        auto* surface = shadow && enhanced ? SDL_CreateGPUShader(device_, &surface_info) : nullptr;
        auto* surface_alpha = surface ? SDL_CreateGPUShader(device_, &surface_alpha_info) : nullptr;
        bool success = shadow != nullptr && (!enhanced || surface_alpha);
        if (!success)
            error = SDL_GetError();
        SDL_GPUVertexBufferDescription buffer{};
        buffer.pitch = sizeof(WorldVertex);
        buffer.input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX;
        SDL_GPUVertexAttribute attributes[5]{};
        attributes[0].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3;
        attributes[1].location = 1;
        attributes[1].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3;
        attributes[1].offset = offsetof(WorldVertex, color);
        attributes[2].location = 2;
        attributes[2].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2;
        attributes[2].offset = offsetof(WorldVertex, uv);
        attributes[3].location = 3;
        attributes[3].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2;
        attributes[3].offset = offsetof(WorldVertex, lightmap);
        attributes[4].location = 4;
        attributes[4].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3;
        attributes[4].offset = offsetof(WorldVertex, normal);
        SDL_GPUVertexBufferDescription skinned_buffer{};
        skinned_buffer.pitch = sizeof(SkinnedVertex);
        skinned_buffer.input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX;
        SDL_GPUVertexAttribute skinned_attributes[5]{};
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
        skinned_attributes[4].location = 4;
        skinned_attributes[4].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3;
        skinned_attributes[4].offset = offsetof(SkinnedVertex, normal);
        for (std::size_t i = 0; success && i < pipelines_.size(); ++i)
        {
            const auto kind = static_cast<WorldPipeline>(i);
            const bool surfaced = kind == WorldPipeline::SurfaceOpaque || kind == WorldPipeline::SurfaceAlpha ||
                                  kind == WorldPipeline::SkinnedSurfaceOpaque ||
                                  kind == WorldPipeline::SkinnedSurfaceAlpha;
            if (surfaced && !enhanced)
                continue; // Only the enhanced renderer creates and uses them.
            const bool blend = kind == WorldPipeline::Alpha || kind == WorldPipeline::SkyAlpha ||
                               kind == WorldPipeline::SkinnedAlpha || kind == WorldPipeline::SurfaceAlpha ||
                               kind == WorldPipeline::SkinnedSurfaceAlpha;
            const bool sky = kind == WorldPipeline::Sky || kind == WorldPipeline::SkyAlpha;
            const bool caster = kind == WorldPipeline::ShadowCaster;
            const bool skin = kind == WorldPipeline::SkinnedOpaque || kind == WorldPipeline::SkinnedAlpha ||
                              kind == WorldPipeline::SkinnedSurfaceOpaque ||
                              kind == WorldPipeline::SkinnedSurfaceAlpha || caster;
            SDL_GPUColorTargetDescription target{};
            target.format = caster ? shadow_tile_format : format;
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
            create.fragment_shader = caster     ? shadow
                                     : surfaced ? (blend ? surface_alpha : surface)
                                     : blend    ? alpha
                                                : opaque;
            create.vertex_input_state = skin ? SDL_GPUVertexInputState{&skinned_buffer, 1, skinned_attributes, 5}
                                             : SDL_GPUVertexInputState{&buffer, 1, attributes, 5};
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
        for (auto* shader : {vertex, skinned, opaque, alpha, shadow, surface, surface_alpha})
            if (shader)
                SDL_ReleaseGPUShader(device_, shader);
        if (!success)
        {
            release();
            return false;
        }
        format_ = format;
        shading_ = shading;
        ++creations;
        return true;
    }
    bool SurfaceMaterials::refresh(const SurfaceScene& scene, GpuRegistry& registry,
                                   std::span<const WorldMaterial> materials, std::string& error)
    {
        if (revision != scene.revision || uniforms.size() > materials.size())
        {
            uniforms.clear();
            normals.clear();
            revision = scene.revision;
        }
        if (!sampler && !(sampler = registry.normal_sampler(error)))
            return false;
        static const SurfaceMaterial fallback;
        for (std::size_t i = uniforms.size(); i < materials.size(); ++i)
        {
            const auto& entry = materials[i];
            const auto& path = entry.path;
            const bool texture = path.size() > 4 && (path.ends_with(".dtx") || path.ends_with(".DTX"));
            const auto& material = scene.manifest && texture ? scene.manifest->resolve(path) : fallback;
            SurfaceMaterialUniform uniform;
            uniform.material = {material.roughness, material.metallic, material.specular, 0};
            uniform.emission = {material.emissive, material.emissive_threshold, 0, 0};
            TextureHandle normal;
            // Lightmap pages, the white slot, failed and animated (SPR) materials get no map.
            if (normal_map_state(entry, material, scene.normal_maps != nullptr) == NormalMapState::Generated)
            {
                const auto map = scene.normal_maps->get(entry.image, material.normal, error);
                if (!map || !(normal = registry.derived_texture("normal", *map, error)))
                {
                    error = "Normal map for " + path + ": " + error;
                    clear();
                    return false;
                }
                uniform.material[3] = 2;
            }
            uniforms.push_back(uniform);
            normals.push_back(normal);
        }
        return true;
    }
    bool SurfaceMaterials::bind_scene(const SurfaceScene& scene, const GpuRegistry& registry,
                                      SDL_GPUCommandBuffer* commands, SDL_GPURenderPass* pass, DVector eye)
    {
        SDL_GPUBuffer* buffers[] = {registry.get(scene.lights), registry.get(scene.cells)};
        if (!buffers[0] || !buffers[1])
            return false;
        SDL_BindGPUFragmentStorageBuffers(pass, 0, buffers, 2);
        auto frame = scene.frame;
        frame.eye = {eye.x, eye.y, eye.z, 0};
        SDL_PushGPUFragmentUniformData(commands, 2, &frame, sizeof(frame));
        return true;
    }
    WorldRenderer::~WorldRenderer()
    {
        clear_geometry();
        registry_->release(depth_);
    }
    void WorldRenderer::clear_geometry()
    {
        registry_->release(vertices_);
        textures_.clear();
        texture_paths_.clear();
        surface_materials_.clear();
        if (registry_ == &own_registry_)
            own_registry_.clear_textures();
        revision_ = UINT64_MAX;
        stats.triangles = stats.draws = stats.alpha_batches = stats.pan_batches = stats.alpha_pan_batches = 0;
        stats.surface_time = 0;
        stats.sky_batches = 0;
        stats.culled = 0;
        alpha_order_.clear();
        visible_.clear();
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
        if (!external_targets_ && (!registry_->get(depth_) || width_ != width || height_ != height))
        {
            SDL_GPUTextureCreateInfo info{};
            info.type = SDL_GPU_TEXTURETYPE_2D;
            info.format = SDL_GPU_TEXTUREFORMAT_D32_FLOAT;
            info.usage = SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET;
            info.width = width;
            info.height = height;
            info.layer_count_or_depth = 1;
            info.num_levels = 1;
            const auto next = registry_->create_texture(info, error);
            if (!next)
                return false;
            registry_->release(depth_);
            depth_ = next;
        }
        width_ = width;
        height_ = height;
        if (revision_ == revision)
        {
            if (dynamic && !upload_vertices(mesh, error))
                return false;
        }
        else
        {
            // Keep the material handles so an appended material list only acquires its tail.
            auto handles = std::move(textures_);
            auto paths = std::move(texture_paths_);
            clear_geometry();
            textures_ = std::move(handles);
            texture_paths_ = std::move(paths);
            if (!(sampler_ = registry_->sampler(style_ == DrawStyle::SpriteAlpha, error)) ||
                !(lightmap_sampler_ = registry_->lightmap_sampler(error)) ||
                !(shadow_sampler_ = registry_->sampler(true, error)) || !upload_vertices(mesh, error) ||
                !registry_->refresh_textures(mesh.materials, textures_, texture_paths_, error) || !textures_[0] ||
                !registry_->get(textures_[0]))
            {
                if (error.empty())
                    error = "Missing diagnostic GPU texture";
                clear_geometry();
                return false;
            }
            revision_ = revision;
            ++stats.uploads;
        }
        if (enhanced() && !surface_materials_.refresh(*surface, *registry_, mesh.materials, error))
            return false;
        return registry_ != &own_registry_ || own_registry_.flush(frame, error);
    }
    bool WorldRenderer::upload_vertices(const WorldMesh& mesh, std::string& error)
    {
        if (mesh.vertices.size() > 3000000)
        {
            error = "GPU vertex budget exceeded";
            return false;
        }
        const auto bytes = static_cast<Uint32>(mesh.vertices.size() * sizeof(WorldVertex));
        return !bytes || (registry_->reserve(vertices_, SDL_GPU_BUFFERUSAGE_VERTEX, bytes, error) &&
                          registry_->stage(vertices_, mesh.vertices.data(), bytes, true, error));
    }
    bool WorldRenderer::validate(const WorldMesh& mesh, std::span<const WorldDraw> draws, DVector eye, DRotation view,
                                 WorldPass world_pass, double surface_time, const PointLight& light, bool letterbox,
                                 bool close_view, std::string& error, const EyeProjection* eye_projection)
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
        if (!pipelines_->get(WorldPipeline::Opaque) || (!external_targets_ && !registry_->get(depth_)) ||
            (!registry_->get(vertices_) && !mesh.vertices.empty()))
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
                    !registry_->get(textures_[image]) || b.lightmap >= textures_.size() ||
                    !registry_->get(textures_[b.lightmap]) || b.first < r.first ||
                    b.first > std::size_t(r.first) + r.count || b.count > std::size_t(r.first) + r.count - b.first ||
                    !world_texture_offset(b.uv_rate, surface_time, texture_offset))
                {
                    error = "Invalid diagnostic GPU material batch";
                    return false;
                }
            }
            if (r.count && (textures_.empty() || !registry_->get(textures_[0])))
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
            const auto frustum = eye_projection ? eye_frustum(eye, view, *eye_projection) : view_frustum(eye, view, aspect);
            for (const auto& d : draws)
            {
                if (in_frustum(mesh.ranges[d.range], d, frustum))
                    visible_.push_back(d);
                else
                    ++stats.culled;
            }
        }
        if (enhanced() && !sky && !close_view &&
            (surface_materials_.uniforms.size() < textures_.size() ||
             surface_materials_.normals.size() < textures_.size() || !surface_materials_.sampler ||
             !registry_->get(surface->lights) || !registry_->get(surface->cells)))
        {
            visible_.clear();
            error = "Enhanced surface materials not prepared";
            return false;
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
        if (!validate(mesh, draws, eye, view, world_pass, surface_time, light, target.letterbox, close_view, error,
                      target.eye))
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
        if (!shared_depth && !registry_->get(depth_))
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
        depth.texture = shared_depth ? shared_depth : registry_->get(depth_);
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
        // Enhanced shading of world faces; validate() checked its resources.
        const bool shaded = enhanced() && !sky && !close_view;
        const bool blended = translucent || style_ == DrawStyle::SpriteAlpha;
        auto* bound = pipelines_->get(sky       ? WorldPipeline::Sky
                                      : shaded  ? (blended ? WorldPipeline::SurfaceAlpha : WorldPipeline::SurfaceOpaque)
                                      : blended ? WorldPipeline::Alpha
                                                : WorldPipeline::Opaque);
        SDL_BindGPUGraphicsPipeline(target.pass, bound);
        if (shaded)
            SurfaceMaterials::bind_scene(*surface, *registry_, target.commands, target.pass, eye);
        if (auto* vertices = registry_->get(vertices_))
        {
            SDL_GPUBufferBinding binding{vertices, 0};
            SDL_BindGPUVertexBuffers(target.pass, 0, &binding, 1);
            static const ShadowUniform no_shadows{};
            SDL_PushGPUFragmentUniformData(target.commands, 1, target.shadows ? target.shadows : &no_shadows,
                                           sizeof(ShadowUniform));
            const bool receive = receive_shadows && target.shadows && !sky && !close_view;
            // Redundant state is skipped within this record; another renderer may have
            // pushed uniforms or bound samplers in between, so nothing is assumed on entry.
            SDL_GPUTexture *bound_texture = nullptr, *bound_lightmap = nullptr, *bound_normal = nullptr;
            WorldViewUniform uniform, pushed;
            bool has_pushed = false, has_material = false;
            SurfaceMaterialUniform pushed_material;
            auto bind = [&](SDL_GPUGraphicsPipeline* pipeline)
            {
                if (pipeline == bound)
                    return;
                SDL_BindGPUGraphicsPipeline(target.pass, pipeline);
                bound = pipeline;
                bound_texture = bound_lightmap = nullptr;
                has_pushed = false;
            };
            auto emit = [&](const WorldDraw& d, const WorldBatch* face)
            {
                const auto& r = mesh.ranges[d.range];
                if (!r.count)
                    return;
                world_view_uniform(eye, view, aspect, d, uniform);
                if (target.eye)
                {
                    uniform.projection[0] = target.eye->scale_x;
                    uniform.projection[1] = target.eye->scale_y;
                    uniform.right[3] = target.eye->offset_x;
                    uniform.up[3] = target.eye->offset_y;
                }
                else if (!close_view)
                {
                    uniform.projection[0] *= target.view_scale;
                    uniform.projection[1] *= target.view_scale;
                }
                if (sky || close_view)
                {
                    constexpr float near = .01f, far = 65536;
                    uniform.projection[2] = far / (far - near);
                    uniform.projection[3] = -near * far / (far - near);
                }
                const bool illuminate = !sky && style_ == DrawStyle::Opaque && !close_view;
                FragmentLightUniform fragment;
                fragment.scene_color = {target.scene_color.x, target.scene_color.y, target.scene_color.z, 1};
                fragment.modulation = {d.alpha, world_saturate_scale, receive ? 1.f : 0.f, 0};
                fragment.position_radius = {light.position.x, light.position.y, light.position.z,
                                            illuminate ? light.radius : 0};
                fragment.light_color = {light.color.x, light.color.y, light.color.z, 0};
                SDL_PushGPUFragmentUniformData(target.commands, 0, &fragment, sizeof(fragment));
                auto batch = [&](std::uint32_t first, std::uint32_t count, std::uint32_t material,
                                 std::array<double, 2> rate, std::uint32_t lightmap)
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
                    // Faces without a lightmap ignore slot 1; white keeps the binding valid.
                    auto* image_texture = registry_->get(textures_[image]);
                    auto* lightmap_texture = registry_->get(textures_[lightmap]);
                    SDL_GPUTexture* normal_texture = nullptr;
                    if (shaded)
                    {
                        const auto& parameters = surface_materials_.uniforms[material];
                        if (!has_material || !(parameters == pushed_material))
                        {
                            SDL_PushGPUFragmentUniformData(target.commands, 3, &parameters, sizeof(parameters));
                            pushed_material = parameters;
                            has_material = true;
                        }
                        // Slot 3: the generated normal map, or white when the material has none (unread).
                        normal_texture = registry_->get(surface_materials_.normals[material]);
                        if (!normal_texture)
                            normal_texture = registry_->get(textures_[0]);
                    }
                    if (image_texture != bound_texture || lightmap_texture != bound_lightmap ||
                        normal_texture != bound_normal)
                    {
                        // Slot 2: the shadow tiles, or white when there are none (never read then).
                        auto* shadow_texture = target.shadow_atlas ? target.shadow_atlas : registry_->get(textures_[0]);
                        const SDL_GPUTextureSamplerBinding textures[]{{image_texture, sampler_},
                                                                      {lightmap_texture, lightmap_sampler_},
                                                                      {shadow_texture, shadow_sampler_},
                                                                      {normal_texture, surface_materials_.sampler}};
                        SDL_BindGPUFragmentSamplers(target.pass, 0, textures, shaded ? 4 : 3);
                        bound_texture = image_texture;
                        bound_lightmap = lightmap_texture;
                        bound_normal = normal_texture;
                    }
                    SDL_DrawGPUPrimitives(target.pass, count, 1, first, 0);
                    ++stats.draw_calls;
                };
                if (face)
                    batch(face->first, face->count, face->material, face->uv_rate, face->lightmap);
                else if (r.batches.empty())
                {
                    if (sky)
                    {
                        bind(pipelines_->get(WorldPipeline::Sky));
                        ++stats.sky_batches;
                    }
                    batch(r.first, r.count, 0, {}, 0);
                }
                else
                    for (const auto& b : r.batches)
                    {
                        if (sky)
                        {
                            bind(pipelines_->get(b.translucent ? WorldPipeline::SkyAlpha : WorldPipeline::Sky));
                            batch(b.first, b.count, b.material, b.uv_rate, b.lightmap);
                            ++stats.sky_batches;
                        }
                        else if ((style_ == DrawStyle::SpriteAlpha || !b.translucent) &&
                                 !(world_pass == WorldPass::OpaqueWithSky && b.sky_portal))
                            batch(b.first, b.count, b.material, b.uv_rate, b.lightmap);
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
