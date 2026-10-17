#include "app/model_scene.h"
#include "app/sprite_scene.h"
#include "engine/capture.h"
#include "engine/runtime.h"
#include "engine/world_renderer.h"
#include "game/original/AI_Mgr.h"
#include "game/original/Corpse.h"
#include "game/original/Gib.h"
#include <algorithm>
#include <cmath>
#include <iostream>
using namespace OpenBlood2;
#define CHECK(x)                                                                                                       \
    do                                                                                                                 \
    {                                                                                                                  \
        if (!(x))                                                                                                      \
        {                                                                                                              \
            std::cerr << __LINE__ << ": " #x << " " << error << " " << SDL_GetError() << "\n";                         \
            return 1;                                                                                                  \
        }                                                                                                              \
    } while (false)
namespace
{
    bool render(Runtime& runtime, WorldRenderer& renderer, const WorldMesh& mesh, std::uint64_t revision,
                const std::vector<WorldDraw>& draws, DVector eye, DRotation camera, Capture& capture,
                std::vector<std::uint8_t>& pixels, std::string& error)
    {
        if ((!capture.texture && !capture.initialize(runtime.gpu, SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM, 400, 300)) ||
            !renderer.prepare(mesh, revision, SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM, 400, 300, error, true))
            return false;
        auto* commands = SDL_AcquireGPUCommandBuffer(runtime.gpu);
        if (!commands)
            return false;
        if (!renderer.draw(commands, capture.texture, mesh, draws, eye, camera, false, error))
        {
            SDL_SubmitGPUCommandBuffer(commands);
            return false;
        }
        auto* copy = SDL_BeginGPUCopyPass(commands);
        if (!copy)
        {
            SDL_SubmitGPUCommandBuffer(commands);
            return false;
        }
        SDL_GPUTextureRegion region{};
        region.texture = capture.texture;
        region.w = 400;
        region.h = 300;
        region.d = 1;
        SDL_GPUTextureTransferInfo target{};
        target.transfer_buffer = capture.transfer;
        target.pixels_per_row = 400;
        target.rows_per_layer = 300;
        SDL_DownloadFromGPUTexture(copy, &region, &target);
        SDL_EndGPUCopyPass(copy);
        if (!SDL_SubmitGPUCommandBuffer(commands) || !SDL_WaitForGPUIdle(runtime.gpu))
            return false;
        auto* data = static_cast<const std::uint8_t*>(SDL_MapGPUTransferBuffer(runtime.gpu, capture.transfer, false));
        if (!data)
            return false;
        pixels.assign(data, data + 400 * 300 * 4);
        SDL_UnmapGPUTransferBuffer(runtime.gpu, capture.transfer);
        return true;
    }
} // namespace
int main(int argc, char** argv)
{
    if (argc != 3)
        return 2;
    std::string error;
    Runtime runtime;
    CHECK(runtime.initialize(true, error));
    std::cout << "GPU device: "
              << SDL_GetStringProperty(SDL_GetGPUDeviceProperties(runtime.gpu), SDL_PROP_GPU_DEVICE_NAME_STRING,
                                       "unknown")
              << "\n";
    WorldRenderer renderer(runtime.gpu, DrawStyle::SpriteAlpha);
    GameSession game;
    CHECK(game.initialize());
    Config config;
    config.data_dir = argv[2];
    CHECK(game.resources.mount_game(config, error));
    WorldModel floor;
    floor.points = {{-4000, 0, -4000}, {4000, 0, -4000}, {4000, 0, 4000}, {-4000, 0, 4000}};
    floor.surfaces.resize(1);
    floor.surfaces[0].flags = 1;
    floor.polygons = {{0, {0, 1, 2, 3}}};
    CHECK(game.collision.build(floor, error));
    game.server.collision_ready = true;
    std::filesystem::create_directories(argv[1]);
    Capture capture;
    std::vector<std::uint8_t> pixels, again;
    auto ink = [&]()
    {
        unsigned n = 0;
        for (unsigned i = 0; i < 400 * 300; ++i)
            if (pixels[i * 4] != 6 || pixels[i * 4 + 1] != 9 || pixels[i * 4 + 2] != 13)
                ++n;
        return n;
    };
    for (int cycle = 0; cycle < 2; ++cycle)
    {
        auto h = game.objects.create(game.objects.find_class("CultistAI"), {}, Creation::World,
                                     {{"Pos", DVector{0, 100, 100}}, {"AIState", std::string("IDLE")}});
        auto* ai = dynamic_cast<AI_Mgr*>(game.server.HandleToObject(h));
        CHECK(ai && ai->RuntimeReady() && ai->m_MoveObj.Update());
        CHECK(ai->StartBurnDeath());
        ai->m_damage.reaction_damage_type = DamageType::Fire;
        CHECK(ai->SetNewState(AI_Mgr::StateDying) && ai->DispatchState() && ai->DispatchState());
        auto* corpse = dynamic_cast<CCorpse*>(game.server.HandleToObject(ai->last_corpse));
        CHECK(corpse && corpse->fire_effect);
        CHECK(game.objects.remove(h) && game.objects.finish_frame());
        SpriteView sprites;
        const auto now = game.objects.time();
        CHECK(game.server.object_particles.advance(now, game.server) &&
              game.server.object_particles.advance(now + .11, game.server));
        CHECK(update_sprite_scene(game, sprites, error));
        CHECK(sprites.entries.size() == 15);
        const auto* body = game.objects.get(corpse->m_hObject);
        CHECK(body);
        DVector torso;
        DRotation rot;
        CHECK(game.server.GetModelNodeTransform(corpse->m_hObject, "torso", &torso, &rot));
        auto eye = vector_subtract(torso, {0, 0, 75});
        CHECK(
            render(runtime, renderer, sprites.mesh, sprites.revision, sprites.draws, eye, {}, capture, pixels, error));
        CHECK(ink() > 25);
        const auto fire_pixels = ink();
        CHECK(
            render(runtime, renderer, sprites.mesh, sprites.revision, sprites.draws, eye, {}, capture, again, error) &&
            again == pixels);
        CHECK(capture.save((std::filesystem::path(argv[1]) / ("flame-" + std::to_string(cycle) + ".png")).c_str()));
        corpse->ClearBurn();
        CHECK(game.server.object_particles.advance(now + .12, game.server));
        auto limb = CreateLimb(corpse->m_hObject, 1, {0, 0, 1}, error);
        CHECK(limb);
        ModelView models;
        const auto* detached = game.objects.get(limb);
        CHECK(detached);
        models.begin_frame();
        CHECK(models.add(detached->model.get(), detached->animation, detached->state.m_SkinName,
                         {detached->state.m_Pos, detached->state.m_Rotation}, detached->state.m_Scale, true, false,
                         detached->id, detached->state.m_Filename, game.resources, error, detached->hidden_nodes));
        CHECK(models.entries.size() == 1 && models.entries.front().error.empty() &&
              models.entries.front().triangles > 0);
        CHECK(render(runtime, renderer, models.mesh, models.revision, models.world_draws,
                     vector_subtract(detached->state.m_Pos, {0, 0, 50}), {}, capture, pixels, error));
        CHECK(ink() > 25);
        const auto limb_pixels = ink();
        ObjectCreateStruct smoke_state;
        smoke_state.m_Pos = {0, 100, 100};
        auto smoke = game.objects.create(game.objects.find_class("BaseClass"), smoke_state);
        CHECK(smoke);
        CHECK(game.server.PublishObjectParticles(smoke, 21, error));
        CHECK(game.server.object_particles.advance(now + .13, game.server));
        smoke_state.m_Pos.x += 26;
        CHECK(game.server.TeleportObject(smoke, &smoke_state.m_Pos) == DE_OK);
        CHECK(game.server.object_particles.advance(now + .24, game.server));
        CHECK(update_sprite_scene(game, sprites, error) && sprites.entries.size() == 9);
        CHECK(render(runtime, renderer, sprites.mesh, sprites.revision, sprites.draws, {26, 100, 25}, {}, capture,
                     pixels, error));
        CHECK(ink() > 25);
        const auto smoke_pixels = ink();
        CHECK(game.objects.remove(smoke) && game.objects.finish_frame());
        CHECK(game.server.PublishFleshGibs(corpse->m_hObject, {0, 100, 100}, {0, 100, 0}, {10, 10, 10}, 1, 8, error));
        CHECK(update_explosion_model_scene(game, models, error) &&
              models.entries.size() == game.server.flesh_gibs.active.size() && models.entries.size() >= 8);
        for (const auto& e : models.entries)
            CHECK(e.error.empty() && e.triangles > 0);
        CHECK(render(runtime, renderer, models.mesh, models.revision, models.world_draws, {0, 100, 40}, {}, capture,
                     pixels, error));
        CHECK(ink() > 25);
        CHECK(capture.save((std::filesystem::path(argv[1]) / ("flesh-" + std::to_string(cycle) + ".png")).c_str()));
        std::cout << "death FX cycle=" << cycle << " flame pixels=" << fire_pixels << " smoke pixels=" << smoke_pixels
                  << " limb pixels=" << limb_pixels << " flesh pixels=" << ink()
                  << "; hidden UI, repeated readback unchanged\n";
        CHECK(game.unload_world());
        sprites.clear();
        models.clear();
        renderer.clear_geometry();
        CHECK(game.resources.mount_game(config, error) && game.collision.build(floor, error));
        game.server.collision_ready = true;
    }
    // Original player death notification -> original orbit -> actual corpse rendering.
    // Uses the same diagnostic floor and retail Caleb assets, with no inspector clocks.
    game.world_path = "diagnostic-player-death-gpu.dat";
    game.state = SessionState::Inspection;
    game.server_shell.PreStartWorld();
    CHECK(game.server_shell.PostStartWorld());
    CHECK(game.objects.create(game.objects.find_class("GameStartPoint"), {}, Creation::World,
                              {{"Name", std::string("START")}, {"Pos", DVector{0, 100, 0}}}));
    CHECK(game.enter_player(error));
    auto* player = game.server_shell.Player(game.local_client);
    CHECK(player);
    auto damage = game.server.StartMessageToObject(player, player->m_hObject, damage_message);
    CHECK(damage &&
          game.server.WriteDamage(damage, {{0, 0, 1}, {}, 1000, DamageType::Normal, player->m_hObject}) == DE_OK);
    CHECK(game.server.EndMessage(damage) == DE_OK);
    for (int i = 0; i < 60; ++i)
        CHECK(game.step(1. / 60));
    CHECK(player->m_bDead && player->death_corpses == 1 && player->death_error.empty() &&
          player->death_effect_error.empty());
    CHECK(game.client_shell.death_camera.active && !game.client_shell.weapons_visible);
    ModelView player_models;
    CHECK(update_model_scene(game, player_models, error));
    const auto corpse_handle = player->last_death_corpse;
    const auto* body = game.objects.get(corpse_handle);
    CHECK(body && body->model);
    // Isolate the created corpse's real mesh for a stable raster assertion.
    player_models.begin_frame();
    CHECK(player_models.add(body->model.get(), body->animation, body->state.m_SkinName,
                            {body->state.m_Pos, body->state.m_Rotation}, body->state.m_Scale, true, false, body->id,
                            body->state.m_Filename, game.resources, error, body->hidden_nodes));
    CHECK(player_models.entries.front().error.empty() && player_models.entries.front().triangles > 0);
    const auto death_view = game.client_shell.view;
    CHECK(render(runtime, renderer, player_models.mesh, player_models.revision, player_models.world_draws,
                 death_view.position, death_view.rotation, capture, pixels, error));
    CHECK(ink() > 25);
    const auto corpse_pixels = ink();
    CHECK(render(runtime, renderer, player_models.mesh, player_models.revision, player_models.world_draws,
                 death_view.position, death_view.rotation, capture, again, error) &&
          again == pixels);
    CHECK(capture.save((std::filesystem::path(argv[1]) / "player-death.png").c_str()));
    CHECK(game.leave_player() && game.server.IsObjectAlive(corpse_handle));
    CHECK(game.unload_world() && !game.client_shell.death_camera.active);
    player_models.clear();
    renderer.clear_geometry();
    std::cout << "Original player death camera: corpse pixels=" << corpse_pixels
              << "; repeated readback unchanged; independent corpse lifetime; client reset\n";
    return 0;
}
