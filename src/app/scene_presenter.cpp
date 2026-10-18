#include "app/scene_presenter.h"
#include <SDL3/SDL_timer.h>

namespace OpenBlood2
{
    ScenePresenter::ScenePresenter(SDL_GPUDevice* device)
        : pipelines(device), world_renderer(device, DrawStyle::Opaque, pipelines),
          model_renderer(device, DrawStyle::Opaque, pipelines),
          explosion_model_renderer(device, DrawStyle::SpriteAlpha, pipelines),
          polygrid_renderer(device, DrawStyle::Opaque, pipelines),
          sprite_renderer(device, DrawStyle::SpriteAlpha, pipelines), device_(device)
    {
    }
    ScenePresenter::~ScenePresenter()
    {
        if (depth_)
            SDL_ReleaseGPUTexture(device_, depth_);
    }
    bool ScenePresenter::update(GameSession& game, std::string& error)
    {
        error.clear();
        auto fail = [&](const char* stage)
        {
            error = std::string(stage) + ": " + error;
            return false;
        };
        rebuilt = mesh_revision != game.world_revision;
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
            world_mesh = {};
            world_draws.clear();
            if (!game.world_path.empty() && !world_mesh.build(game.world, error, &game.resources))
                return fail("World mesh");
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
        const auto flashlight = game.client_shell.flashlight.Current(game.server);
        const auto time = game.server.world_time();
        const auto started = SDL_GetTicksNS();
        stats.passes = 0;
        for (auto* renderer :
             {&world_renderer, &model_renderer, &explosion_model_renderer, &polygrid_renderer, &sprite_renderer})
            renderer->stats.draw_calls = renderer->stats.culled = 0;
        // Uploads precede the scene pass; copy passes cannot run inside a render pass.
        if (!world_renderer.prepare(world_mesh, mesh_revision, format, width, height, error))
            return fail("World viewport");
        if (!model_renderer.prepare(model_scene.mesh, model_scene.revision, format, width, height, error, true))
            return fail("Model viewport");
        if (!explosion_model_scene.world_draws.empty() &&
            !explosion_model_renderer.prepare(explosion_model_scene.mesh, explosion_model_scene.revision, format, width,
                                              height, error, true))
            return fail("Explosion model viewport");
        if (!polygrid_renderer.prepare(polygrid_scene.mesh, polygrid_scene.revision, format, width, height, error,
                                       true))
            return fail("PolyGrid viewport");
        if (!sprite_renderer.prepare(sprite_scene.mesh, sprite_scene.revision, format, width, height, error, true))
            return fail("Sprite viewport");
        if (!depth_ || depth_width_ != width || depth_height_ != height)
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
                return fail("World viewport");
            }
            if (depth_)
                SDL_ReleaseGPUTexture(device_, depth_);
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
        depth.texture = depth_;
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
        stats.passes = 1;
        const WorldRecordTarget scene{commands, pass, letterbox, view_model_depth, 1};
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
        else if (!model_renderer.record(scene, model_scene.mesh, model_scene.world_draws, view.position, view.rotation,
                                        error, WorldPass::Opaque, 0, flashlight))
            stage = "Model viewport";
        else if (world_mesh.blended &&
                 !world_renderer.record(scene, world_mesh, world_draws, view.position, view.rotation, error,
                                        WorldPass::Translucent, time, flashlight))
            stage = "Transparent world viewport";
        else if (model_scene.mesh.blended &&
                 !model_renderer.record(scene, model_scene.mesh, model_scene.world_draws, view.position, view.rotation,
                                        error, WorldPass::Translucent, 0, flashlight))
            stage = "Transparent model viewport";
        else if (!explosion_model_scene.world_draws.empty() &&
                 !explosion_model_renderer.record(scene, explosion_model_scene.mesh, explosion_model_scene.world_draws,
                                                  view.position, view.rotation, error))
            stage = "Explosion model viewport";
        else if (!polygrid_renderer.record(scene, polygrid_scene.mesh, polygrid_scene.draws, view.position,
                                           view.rotation, error, WorldPass::Translucent))
            stage = "PolyGrid viewport";
        else if (!sprite_renderer.record(scene, sprite_scene.mesh, sprite_scene.batched_draws, view.position,
                                         view.rotation, error))
            stage = "Sprite viewport";
        else if (!model_scene.close_draws.empty() &&
                 !model_renderer.record({commands, pass, false, 0, view_model_depth}, model_scene.mesh,
                                        model_scene.close_draws, view.position, view.rotation, error, WorldPass::Opaque,
                                        0, {}, true))
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
