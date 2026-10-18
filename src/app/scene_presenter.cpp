#include "app/scene_presenter.h"
#include <SDL3/SDL_timer.h>
#include <algorithm>

namespace OpenBlood2
{
    ScenePresenter::ScenePresenter(SDL_GPUDevice* device)
        : registry(device), pipelines(device), world_renderer(DrawStyle::Opaque, pipelines, registry),
          model_renderer(DrawStyle::Opaque, pipelines, registry),
          explosion_model_renderer(DrawStyle::SpriteAlpha, pipelines, registry),
          polygrid_renderer(DrawStyle::Opaque, pipelines, registry),
          sprite_renderer(DrawStyle::SpriteAlpha, pipelines, registry)
    {
        model_scene.gpu_skinning = explosion_model_scene.gpu_skinning = true;
        world_renderer.receive_shadows = true;
    }
    ScenePresenter::~ScenePresenter()
    {
        registry.release(depth_);
        registry.release(shadow_atlas_);
        registry.release(shadow_depth_);
    }
    void ScenePresenter::select_shadow_casters(const GameSession& game)
    {
        shadow_casters.clear();
        shadow_uniform = {};
        const bool available = shadow_stats.available;
        shadow_stats = {};
        shadow_stats.available = available;
        if (!model_shadows)
            return;
        const auto& camera = game.client_shell.view;
        const auto eye = camera.position;
        const auto frustum = view_frustum(eye, camera.rotation, shadow_aspect);
        struct Candidate
        {
            double distance = 0;
            ModelRenderer::ShadowCaster caster;
            std::array<float, 4> region{}; // World x/z of every receiver this caster can shade.
        };
        std::vector<Candidate> candidates;
        for (const auto& draw : model_scene.world_draws)
        {
            if (draw.range >= model_scene.instances.size())
                continue;
            const auto& instance = model_scene.instances[draw.range];
            // Entries map instances back to their object for the original FLAG_SHADOW.
            const ObjectRecord* record = nullptr;
            for (const auto& entry : model_scene.entries)
                if (entry.range == draw.range && !entry.close && entry.object)
                {
                    record = game.objects.get(entry.object);
                    break;
                }
            // FLAG_REALLYCLOSE (bit 6 for models in basetypes_de.h) draws at the eye; no shadow.
            constexpr DDWORD really_close = 1u << 6;
            if (!record || !(record->state.m_Flags & FLAG_SHADOW) || (record->state.m_Flags & really_close))
                continue;
            ++shadow_stats.candidates;
            if (!instance.bounded)
            {
                ++shadow_stats.unbounded;
                continue;
            }
            const auto delta = vector_subtract(instance.center, eye);
            const double distance = std::sqrt(double(vector_dot(delta, delta)));
            if (distance > shadow_distance)
            {
                ++shadow_stats.distant;
                continue;
            }
            Candidate candidate{distance, {draw.range, draw.position, draw.rotation, {}, {}, {}, {}, {}}};
            auto& caster = candidate.caster;
            for (int corner = 0; corner < 8; ++corner)
            {
                const DVector local{corner & 1 ? instance.max.x : instance.min.x,
                                    corner & 2 ? instance.max.y : instance.min.y,
                                    corner & 4 ? instance.max.z : instance.min.z};
                const auto p = vector_add(draw.position, rotate_vector(local, draw.rotation));
                caster.min = corner ? DVector{std::min(caster.min.x, p.x), std::min(caster.min.y, p.y),
                                              std::min(caster.min.z, p.z)}
                                    : p;
                caster.max = corner ? DVector{std::max(caster.max.x, p.x), std::max(caster.max.y, p.y),
                                              std::max(caster.max.z, p.z)}
                                    : p;
            }
            caster.min = vector_subtract(caster.min, {shadow_margin, 1, shadow_margin});
            caster.max = vector_add(caster.max, {shadow_margin, 1, shadow_margin});
            // Light travel direction: from the strongest static source through the model center, at
            // least shadow_elevation above the horizon; straight down without a source.
            DVector direction{0, -1, 0};
            if (instance.light.count)
            {
                const auto away = vector_subtract(instance.center, instance.light.sources[0].position);
                const float length = std::sqrt(vector_dot(away, away));
                if (length > 1)
                    direction = vector_scale(away, 1 / length);
            }
            const float horizontal = std::sqrt(direction.x * direction.x + direction.z * direction.z);
            const float minimum = std::sin(shadow_elevation * 3.14159265358979f / 180);
            if (-direction.y < minimum)
            {
                const float scale = horizontal > 1e-6f ? std::sqrt(1 - minimum * minimum) / horizontal : 0;
                direction = {direction.x * scale, -minimum, direction.z * scale};
            }
            caster.slope = {-direction.x / direction.y, -direction.z / direction.y};
            // A point at height y projects along the light onto the box top at xz + slope * (y - top).
            // Caster points (feet to top) span the footprint; receivers (top down to shadow_fade below
            // the feet) lie at xz - slope * (y - top) from a footprint point, spanning the region.
            std::array<float, 4> region{};
            for (int corner = 0; corner < 8; ++corner)
            {
                const float x = corner & 1 ? caster.max.x : caster.min.x, z = corner & 2 ? caster.max.z : caster.min.z;
                const float rise = (corner & 4 ? caster.max.y : caster.min.y) - caster.max.y;
                const float drop = (corner & 4 ? caster.max.y : caster.min.y - shadow_fade) - caster.max.y;
                const float px = x + caster.slope[0] * rise, pz = z + caster.slope[1] * rise;
                const float rx = x - caster.slope[0] * drop, rz = z - caster.slope[1] * drop;
                auto& f = caster.footprint;
                f = corner ? std::array<float, 4>{std::min({f[0], x, px}), std::min({f[1], z, pz}),
                                                  std::max({f[2], x, px}), std::max({f[3], z, pz})}
                           : std::array<float, 4>{std::min(x, px), std::min(z, pz), std::max(x, px), std::max(z, pz)};
                region = corner
                             ? std::array<float, 4>{std::min({region[0], x, rx}), std::min({region[1], z, rz}),
                                                    std::max({region[2], x, rx}), std::max({region[3], z, rz})}
                             : std::array<float, 4>{std::min(x, rx), std::min(z, rz), std::max(x, rx), std::max(z, rz)};
            }
            // Skip casters whose receiving region is outside the view.
            const DVector lo{region[0], caster.min.y - shadow_fade, region[1]};
            const DVector hi{region[2], caster.max.y, region[3]};
            const auto half = vector_scale(vector_subtract(hi, lo), .5f);
            if (!sphere_visible(frustum, vector_add(lo, half), std::sqrt(double(vector_dot(half, half)))))
            {
                ++shadow_stats.outside;
                continue;
            }
            candidate.region = region;
            candidates.push_back(candidate);
        }
        std::stable_sort(candidates.begin(), candidates.end(),
                         [](const Candidate& a, const Candidate& b) { return a.distance < b.distance; });
        const std::size_t limit = std::min<std::size_t>(shadow_caster_limit, (shadow_atlas_size / shadow_tile) *
                                                                                 (shadow_atlas_size / shadow_tile));
        if (candidates.size() > limit)
        {
            shadow_stats.beyond_limit = candidates.size() - limit;
            candidates.resize(limit);
        }
        const auto per_row = shadow_atlas_size / shadow_tile;
        for (std::size_t i = 0; i < candidates.size(); ++i)
        {
            auto caster = candidates[i].caster;
            const float x = float(i % per_row * shadow_tile), y = float(i / per_row * shadow_tile);
            caster.viewport = {x, y, float(shadow_tile), float(shadow_tile)};
            const float size = float(shadow_tile) / shadow_atlas_size;
            // The caster's feet are the bottom of its box before the margin.
            const auto& f = caster.footprint;
            shadow_uniform.casters[i * 3] = {x / shadow_atlas_size, y / shadow_atlas_size, size, caster.max.y};
            shadow_uniform.casters[i * 3 + 1] = {f[0], f[1], 1 / (f[2] - f[0]), 1 / (f[3] - f[1])};
            shadow_uniform.casters[i * 3 + 2] = {caster.min.y + 1, shadow_bias, caster.slope[0], caster.slope[1]};
            shadow_casters.push_back(caster);
        }
        shadow_stats.casters = shadow_casters.size();
        for (std::size_t i = 0; i < candidates.size(); ++i)
        {
            const auto& r = candidates[i].region;
            auto& b = shadow_uniform.bounds;
            b = i ? std::array<float, 4>{std::min(b[0], r[0]), std::min(b[1], r[1]), std::max(b[2], r[2]),
                                         std::max(b[3], r[3])}
                  : r;
            const float top = candidates[i].caster.max.y;
            shadow_uniform.height[0] = i ? std::max(shadow_uniform.height[0], top) : top;
        }
        shadow_uniform.info = {float(shadow_casters.size()), shadow_strength, shadow_fade, 1.f / shadow_atlas_size};
        shadow_uniform.eye = {eye.x, eye.y, eye.z, 0};
    }
    bool ScenePresenter::record_shadow_tiles(SDL_GPUCommandBuffer* commands, std::string& error)
    {
        if (shadow_casters.empty())
            return true;
        if (!registry.get(shadow_atlas_))
        {
            auto* device = registry.device();
            shadow_stats.available =
                SDL_GPUTextureSupportsFormat(device, shadow_tile_format, SDL_GPU_TEXTURETYPE_2D,
                                             SDL_GPU_TEXTUREUSAGE_COLOR_TARGET | SDL_GPU_TEXTUREUSAGE_SAMPLER) &&
                SDL_GPUTextureSupportsFormat(device, SDL_GPU_TEXTUREFORMAT_D32_FLOAT, SDL_GPU_TEXTURETYPE_2D,
                                             SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET);
            if (!shadow_stats.available)
            {
                model_shadows = false; // Explicitly unavailable on this device.
                shadow_casters.clear();
                return true;
            }
            SDL_GPUTextureCreateInfo info{};
            info.type = SDL_GPU_TEXTURETYPE_2D;
            info.format = shadow_tile_format;
            info.usage = SDL_GPU_TEXTUREUSAGE_COLOR_TARGET | SDL_GPU_TEXTUREUSAGE_SAMPLER;
            info.width = info.height = shadow_atlas_size;
            info.layer_count_or_depth = info.num_levels = 1;
            shadow_atlas_ = registry.create_texture(info, error);
            info.format = SDL_GPU_TEXTUREFORMAT_D32_FLOAT;
            info.usage = SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET;
            shadow_depth_ = shadow_atlas_ ? registry.create_texture(info, error) : TextureHandle{};
            if (!shadow_depth_)
                return false;
        }
        SDL_GPUColorTargetInfo color{};
        color.texture = registry.get(shadow_atlas_);
        color.clear_color = {shadow_tile_clear, 0, 0, 0};
        color.load_op = SDL_GPU_LOADOP_CLEAR;
        color.store_op = SDL_GPU_STOREOP_STORE;
        color.cycle = true;
        SDL_GPUDepthStencilTargetInfo depth{};
        depth.texture = registry.get(shadow_depth_);
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
        const bool recorded = model_renderer.record_shadows(commands, pass, model_scene, shadow_casters, error);
        SDL_EndGPURenderPass(pass);
        return recorded;
    }
    bool ScenePresenter::update(GameSession& game, std::string& error)
    {
        error.clear();
        auto fail = [&](const char* stage)
        {
            error = std::string(stage) + ": " + error;
            return false;
        };
        rebuilt = mesh_revision != game.world_revision || built_lightmaps_ != lightmaps;
        const auto model_revision = model_scene.revision, effect_revision = explosion_model_scene.revision,
                   sprite_revision = sprite_scene.revision;
        if (rebuilt)
        {
            world_renderer.clear_geometry();
            model_renderer.clear_geometry();
            model_scene.clear();
            explosion_model_scene.clear();
            explosion_model_renderer.clear_geometry();
            polygrid_scene.clear();
            polygrid_renderer.clear_geometry();
            sprite_scene.clear();
            sprite_renderer.clear_geometry();
            registry.clear_textures(); // Material textures belong to one world revision.
            world_mesh = {};
            world_draws.clear();
            model_lighting.clear();
            if (!game.world_path.empty() && !world_mesh.build(game.world, error, &game.resources, lightmaps))
                return fail("World mesh");
            built_lightmaps_ = lightmaps;
            if (!model_lighting.build(world_mesh, game.world, error))
                return fail("Model lighting");
            mesh_revision = game.world_revision;
        }
        render_world = game.client_shell.m_bInWorld && !world_mesh.ranges.empty();
        const auto started = SDL_GetTicksNS();
        if (!OpenBlood2::world_draws(world_mesh, game.objects, world_draws, world_renderer.stats, error))
            return fail("World draw bindings");
        if (!game.server.sky.draws(game.world, world_mesh, game.objects, game.client_shell.view.position, sky_draws,
                                   sky_eye, error))
            return fail("Sky presentation");
        const auto world_done = SDL_GetTicksNS();
        if (!update_model_scene(game, model_scene, error))
            return fail("Model presentation");
        if (!model_lighting.apply(game, world_mesh, world_draws, model_scene, error))
            return fail("Model lighting");
        select_shadow_casters(game);
        model_renderer.per_pixel_lighting = per_pixel_lighting;
        if (!update_explosion_model_scene(game, explosion_model_scene, error))
            return fail("Explosion model presentation");
        const auto models_done = SDL_GetTicksNS();
        if (!update_polygrid_scene(game, polygrid_scene, error))
            return fail("PolyGrid presentation");
        if (!update_sprite_scene(game, sprite_scene, error))
            return fail("Sprite presentation");
        sprite_scene.batch();
        stats.world_ns = world_done - started;
        stats.models_ns = models_done - world_done;
        stats.effects_ns = SDL_GetTicksNS() - models_done;
        resources_changed = rebuilt || model_revision != model_scene.revision ||
                            effect_revision != explosion_model_scene.revision ||
                            sprite_revision != sprite_scene.revision;
        return true;
    }
    bool ScenePresenter::record(GameSession& game, SDL_GPUCommandBuffer* commands, SDL_GPUTexture* target,
                                SDL_GPUTextureFormat format, Uint32 width, Uint32 height, std::string& error)
    {
        error.clear();
        auto fail = [&](const char* stage)
        {
            error = std::string(stage) + ": " + error;
            return false;
        };
        const auto& view = game.client_shell.view;
        const bool letterbox = view.object && view.settings.type == 1;
        if (width && height > (letterbox ? 2 * (height / 6) : 0))
            shadow_aspect = float(width) / float(height - (letterbox ? 2 * (height / 6) : 0)); // Next update().
        const auto flashlight = game.client_shell.flashlight.Current(game.server);
        const auto time = game.server.world_time();
        const auto started = SDL_GetTicksNS();
        stats.passes = 0;
        for (auto* renderer : {&world_renderer.stats, &model_renderer.stats, &explosion_model_renderer.stats,
                               &polygrid_renderer.stats, &sprite_renderer.stats})
            renderer->draw_calls = renderer->culled = 0;
        // Renderers stage uploads in the shared registry; one copy pass in the frame's
        // command buffer records them all before the scene pass.
        if (!world_renderer.prepare(world_mesh, mesh_revision, format, width, height, error))
            return fail("World viewport");
        if (!model_renderer.prepare(model_scene, format, width, height, error))
            return fail("Model viewport");
        if (!explosion_model_scene.world_draws.empty() &&
            !explosion_model_renderer.prepare(explosion_model_scene, format, width, height, error))
            return fail("Explosion model viewport");
        if (!polygrid_renderer.prepare(polygrid_scene.mesh, polygrid_scene.revision, format, width, height, error,
                                       true))
            return fail("PolyGrid viewport");
        if (!sprite_renderer.prepare(sprite_scene.mesh, sprite_scene.revision, format, width, height, error, true))
            return fail("Sprite viewport");
        if (!registry.flush(commands, error))
            return fail("GPU uploads");
        if (!record_shadow_tiles(commands, error))
            return fail("Model shadow tiles");
        stats.passes = shadow_casters.empty() ? 0 : 1;
        if (!registry.get(depth_) || depth_width_ != width || depth_height_ != height)
        {
            SDL_GPUTextureCreateInfo info{};
            info.type = SDL_GPU_TEXTURETYPE_2D;
            info.format = SDL_GPU_TEXTUREFORMAT_D32_FLOAT;
            info.usage = SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET;
            info.width = width;
            info.height = height;
            info.layer_count_or_depth = 1;
            info.num_levels = 1;
            const auto next = registry.create_texture(info, error);
            if (!next)
                return fail("World viewport");
            registry.release(depth_);
            depth_ = next;
            depth_width_ = width;
            depth_height_ = height;
        }
        SDL_GPUColorTargetInfo color{};
        color.texture = target;
        color.clear_color = {.025f, .035f, .05f, 1};
        color.load_op = SDL_GPU_LOADOP_CLEAR;
        color.store_op = SDL_GPU_STOREOP_STORE;
        SDL_GPUDepthStencilTargetInfo depth{};
        depth.texture = registry.get(depth_);
        depth.clear_depth = 1;
        depth.load_op = SDL_GPU_LOADOP_CLEAR;
        depth.store_op = SDL_GPU_STOREOP_DONT_CARE; // Nothing reads scene depth after this pass.
        depth.stencil_load_op = SDL_GPU_LOADOP_DONT_CARE;
        depth.stencil_store_op = SDL_GPU_STOREOP_DONT_CARE;
        depth.cycle = true;
        auto* pass = SDL_BeginGPURenderPass(commands, &color, 1, &depth);
        if (!pass)
        {
            error = SDL_GetError();
            return fail("World viewport");
        }
        ++stats.passes;
        WorldRecordTarget scene{commands, pass, letterbox, view_model_depth, 1};
        if (!shadow_casters.empty())
        {
            scene.shadow_atlas = registry.get(shadow_atlas_);
            scene.shadows = &shadow_uniform;
        }
        // Same order as the former separate passes: sky, opaque world/models, sorted
        // world/model alpha, explosion models, PolyGrids, sprites, then view models.
        const char* stage = nullptr;
        if (!sky_draws.empty() &&
            !world_renderer.record(scene, world_mesh, sky_draws, sky_eye, view.rotation, error, WorldPass::Sky, time))
            stage = "World viewport";
        else if (!world_renderer.record(scene, world_mesh, world_draws, view.position, view.rotation, error,
                                        sky_draws.empty() ? WorldPass::Opaque : WorldPass::OpaqueWithSky, time,
                                        flashlight))
            stage = "World viewport";
        else if (!model_renderer.record(scene, model_scene, model_scene.world_draws, view.position, view.rotation,
                                        error, WorldPass::Opaque, flashlight))
            stage = "Model viewport";
        else if (world_mesh.blended &&
                 !world_renderer.record(scene, world_mesh, world_draws, view.position, view.rotation, error,
                                        WorldPass::Translucent, time, flashlight))
            stage = "Transparent world viewport";
        else if (model_scene.mesh.blended &&
                 !model_renderer.record(scene, model_scene, model_scene.world_draws, view.position, view.rotation,
                                        error, WorldPass::Translucent, flashlight))
            stage = "Transparent model viewport";
        else if (!explosion_model_scene.world_draws.empty() &&
                 !explosion_model_renderer.record(scene, explosion_model_scene, explosion_model_scene.world_draws,
                                                  view.position, view.rotation, error))
            stage = "Explosion model viewport";
        else if (!polygrid_renderer.record(scene, polygrid_scene.mesh, polygrid_scene.draws, view.position,
                                           view.rotation, error, WorldPass::Translucent))
            stage = "PolyGrid viewport";
        else if (!sprite_renderer.record(scene, sprite_scene.mesh, sprite_scene.batched_draws, view.position,
                                         view.rotation, error))
            stage = "Sprite viewport";
        else if (!model_scene.close_draws.empty() &&
                 !model_renderer.record({commands, pass, false, 0, view_model_depth}, model_scene,
                                        model_scene.close_draws, view.position, view.rotation, error, WorldPass::Opaque,
                                        {}, true))
            stage = "Model viewport";
        SDL_EndGPURenderPass(pass);
        if (stage)
            return fail(stage);
        stats.draw_calls = world_renderer.stats.draw_calls + model_renderer.stats.draw_calls +
                           explosion_model_renderer.stats.draw_calls + polygrid_renderer.stats.draw_calls +
                           sprite_renderer.stats.draw_calls;
        stats.culled = world_renderer.stats.culled + model_renderer.stats.culled +
                       explosion_model_renderer.stats.culled + polygrid_renderer.stats.culled +
                       sprite_renderer.stats.culled;
        stats.record_ns = SDL_GetTicksNS() - started;
        ++stats.frames;
        return true;
    }
} // namespace OpenBlood2
