#include "app/model_scene.h"
#include "engine/capture.h"
#include "engine/runtime.h"
#include "engine/world_renderer.h"
#include <SDL3/SDL.h>
#include <iostream>
using namespace OpenBlood2;
#define CHECK(x)                                                                                                       \
    do                                                                                                                 \
    {                                                                                                                  \
        if (!(x))                                                                                                      \
        {                                                                                                              \
            std::cerr << __LINE__ << ": " #x " | " << error << ' ' << SDL_GetError() << '\n';                          \
            return 1;                                                                                                  \
        }                                                                                                              \
    } while (false)
int main(int argc, char** argv)
{
    if (argc != 3)
        return 2;
    std::string error;
    Runtime runtime;
    CHECK(runtime.initialize(true, error));
    std::cout << "GPU device: "
              << SDL_GetStringProperty(SDL_GetGPUDeviceProperties(runtime.gpu), SDL_PROP_GPU_DEVICE_NAME_STRING,
                                       "unreported")
              << '\n';
    auto game = std::make_unique<GameSession>();
    Config config;
    config.data_dir = argv[1];
    CHECK(game->initialize() && game->resources.mount_game(config, error));
    CHECK(game->request_world("01_SubwayA", error) && game->enter_player(error));
    CHECK(game->server.audio.open_output(error));
    WorldMesh mesh;
    ModelView scene;
    WorldRenderer world(runtime.gpu), models(runtime.gpu);
    std::uint64_t revision = UINT64_MAX;
    for (unsigned cycle = 0; cycle < 3; ++cycle)
    {
        CHECK(game->step(1. / 60));
        CHECK(game->server.audio.has_output() && game->server.audio.submitted_frames > 0);
        CHECK(game->world_revision != revision);
        revision = game->world_revision;
        // Same cache invalidation boundary used by the application; no retained
        // original pointers, model entries or GPU material IDs cross worlds.
        scene.clear();
        CHECK(mesh.build(game->world, error, &game->resources));
        CHECK(update_model_scene(*game, scene, error));
        std::vector<WorldDraw> draws;
        WorldViewStats stats;
        CHECK(world_draws(mesh, game->objects, draws, stats, error));
        Capture capture;
        CHECK(capture.initialize(runtime.gpu, SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM, 640, 420));
        CHECK(world.prepare(mesh, revision, SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM, 640, 420, error));
        CHECK(models.prepare(scene.mesh, scene.revision, SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM, 640, 420, error, true));
        const auto view = game->client_shell.view;
        CHECK(view.object);
        auto* cmd = SDL_AcquireGPUCommandBuffer(runtime.gpu);
        CHECK(cmd);
        CHECK(world.draw(cmd, capture.texture, mesh, draws, view.position, view.rotation, true, error, DrawLoad::Clear,
                         nullptr, WorldPass::Opaque, game->server.world_time()));
        CHECK(models.draw(cmd, capture.texture, scene.mesh, scene.world_draws, view.position, view.rotation, true,
                          error, DrawLoad::Load, world.depth_target()));
        CHECK(world.draw(cmd, capture.texture, mesh, draws, view.position, view.rotation, true, error, DrawLoad::Load,
                         nullptr, WorldPass::Translucent, game->server.world_time()));
        auto* copy = SDL_BeginGPUCopyPass(cmd);
        CHECK(copy);
        SDL_GPUTextureRegion region{};
        region.texture = capture.texture;
        region.w = 640;
        region.h = 420;
        region.d = 1;
        SDL_GPUTextureTransferInfo target{};
        target.transfer_buffer = capture.transfer;
        target.pixels_per_row = 640;
        target.rows_per_layer = 420;
        SDL_DownloadFromGPUTexture(copy, &region, &target);
        SDL_EndGPUCopyPass(copy);
        CHECK(SDL_SubmitGPUCommandBuffer(cmd) && SDL_WaitForGPUIdle(runtime.gpu));
        const auto* bytes = static_cast<const Uint8*>(SDL_MapGPUTransferBuffer(runtime.gpu, capture.transfer, false));
        CHECK(bytes);
        std::vector<Uint8> pixels(bytes, bytes + 640 * 420 * 4);
        SDL_UnmapGPUTransferBuffer(runtime.gpu, capture.transfer);
        std::size_t filled = 0, missing = 0;
        for (std::size_t i = 0; i < pixels.size(); i += 4)
        {
            filled += pixels[i] != 6 || pixels[i + 1] != 9 || pixels[i + 2] != 13;
            missing += pixels[i] > 240 && pixels[i + 1] < 10 && pixels[i + 2] > 240;
        }
        CHECK(filled > 10000 && missing == 0 && world.stats.uploads == cycle + 1);
        CHECK(capture.save((std::string(argv[2]) + "-" + std::to_string(cycle) + ".png").c_str()));
        auto* player = game->server_shell.Player(game->local_client);
        CHECK(player);
        player->m_damage.SetHitPoints(37);
        player->m_damage.SetArmorPoints(23);
        player->m_InventoryMgr.SetAmmoCount(1, 1234);
        const auto before = game.get();
        const auto old_player = player->m_hObject;
        CHECK(game->server.SendWorldExit({"02_CityHub"}, "01_egressa") == DE_OK && game->pump_player_messages(error));
        CHECK(GameSession::process_level_change(game, error) == LevelChangeResult::Rejected);
        CHECK(game.get() == before && game->client_shell.player == old_player && game->world_revision == revision);
        CHECK(game->level_change.unknown == 10 && game->level_change.failed == 1 && game->server.audio.has_output());
        CHECK(game->server.SendWorldExit({"01_SubwayA"}, "start") == DE_OK && game->pump_player_messages(error));
        CHECK(GameSession::process_level_change(game, error) == LevelChangeResult::Committed);
        CHECK(game.get() != before && game->client_shell.player != old_player && game->server.audio.has_output());
        CHECK(game->level_change.cleanup_complete && game->level_change.health == 37 &&
              game->level_change.armor == 23 && game->level_change.bullets == 1234);
        CHECK(game->client_shell.world_exit.completed == cycle + 1 &&
              game->client_shell.world_exit.blocked == cycle + 1);
        std::cout
            << "Cycle " << cycle << ": " << filled
            << " textured pixels, no fallback; CityHub retained source, SubwayA committed, audio output transferred\n";
    }
    CHECK(game->unload_world() && game->server.string_count() == 0 && game->server.client_count() == 0);
    return 0;
}
