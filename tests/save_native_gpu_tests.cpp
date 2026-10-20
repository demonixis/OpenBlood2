// Sky and PolyGrid presentation after a saved-game load (save_load_contracts lot 13), with the
// original and the enhanced renderer: the restored world presents the same sky slots and the
// same PolyGrid surfaces as the saved world, each exactly once.
#include "app/scene_presenter.h"
#include "app/session_playback.h"
#include "engine/capture.h"
#include "engine/save_objects.h"
#include "engine/world_shaders.h"
#include "game/world_validation.h"
#include <SDL3/SDL.h>
#include <iostream>
#include <set>
using namespace OpenBlood2;
#define CHECK(x)                                                                                                       \
    do                                                                                                                 \
    {                                                                                                                  \
        if (!(x))                                                                                                      \
        {                                                                                                              \
            std::cerr << __LINE__ << ": " #x " " << error << " " << SDL_GetError() << '\n';                            \
            return 1;                                                                                                  \
        }                                                                                                              \
    } while (false)
namespace
{
    struct Presented
    {
        std::size_t sky_world = 0, sky_sprites = 0, sky_order = 0, grids = 0, grid_draws = 0;
        bool operator==(const Presented&) const = default;
    };
    bool frame(SDL_GPUDevice* gpu, ScenePresenter& presenter, GameSession& game, Capture& capture, Presented& out,
               std::string& error)
    {
        if (!presenter.update(game, error) || !presenter.render_world)
            return false;
        auto* commands = SDL_AcquireGPUCommandBuffer(gpu);
        if (!commands || !presenter.record(game, commands, capture.texture, SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM,
                                           capture.width, capture.height, error))
            return false;
        if (!SDL_SubmitGPUCommandBuffer(commands) || !SDL_WaitForGPUIdle(gpu))
            return false;
        std::set<std::uint64_t> ids;
        for (const auto& entry : presenter.polygrid_scene.entries)
            if (!ids.insert(entry.id).second)
            {
                error = "PolyGrid presented twice";
                return false;
            }
        out = {presenter.sky_draws.size(), presenter.sky_sprite_scene.draws.size(), presenter.sky_order.size(),
               presenter.polygrid_scene.entries.size(), presenter.polygrid_scene.draws.size()};
        return true;
    }
} // namespace
int main(int argc, char** argv)
{
    std::string error;
    if (argc != 2)
        return 2;
    CHECK(SDL_Init(SDL_INIT_VIDEO));
    auto* gpu = SDL_CreateGPUDevice(WorldShaderFormats, true, nullptr);
    CHECK(gpu);
    std::cout << "GPU=" << SDL_GetStringProperty(SDL_GetGPUDeviceProperties(gpu), SDL_PROP_GPU_DEVICE_NAME_STRING, "?")
              << " backend=" << SDL_GetGPUDeviceDriver(gpu) << '\n';
    for (const char* world : {"02_CityHub", "13_Downtown"})
    {
        Config config;
        config.data_dir = argv[1];
        auto game = std::make_unique<GameSession>();
        CHECK(game->initialize() && game->resources.mount_game(config, error) && game->request_world(world, error) &&
              game->run_world(error));
        for (int i = 0; i < 30; ++i)
            CHECK(step_app_session(*game, 1.0 / 60));
        Capture capture;
        CHECK(capture.initialize(gpu, SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM, 640, 360));
        ScenePresenter presenter(gpu);
        Presented saved[2], restored[2];
        const SurfaceShading modes[2]{SurfaceShading::Original, SurfaceShading::Standard};
        for (int m = 0; m < 2; ++m)
        {
            presenter.shading = modes[m];
            CHECK(frame(gpu, presenter, *game, capture, saved[m], error));
        }
        CHECK(saved[0] == saved[1] && saved[0].sky_order > 0 && saved[0].grids > 0);
        // An image of every Original object (the player, movers, brushes and volumes), loaded through
        // the restore-mode world: the volumes recreate their surfaces, the sky objects return.
        std::vector<HOBJECT> covered;
        for (auto id : game->objects.snapshot())
            if (const auto* r = game->objects.get(id); r && r->life == Life::Alive &&
                                                       save_support(r->type) == SaveSupport::Original &&
                                                       (r->user_flags & USRFLG_SAVEABLE))
                covered.push_back(r->handle);
        SaveObjectsReport report;
        CHECK(game->server.save_objects("Save\\Current\\Current.sav", covered, 0,
                                        SAVEOBJECTS_SAVEGAMECONSOLE | SAVEOBJECTS_SAVEPORTALS, report));
        GameSave save{game->world_path, {}, *game->server.save_store.find(SaveStoreSlot::Current)};
        SaveError fingerprint;
        CHECK(build_fingerprint(game->resources, save.world_path, save.image.classes, save.fingerprint, fingerprint));
        CHECK(GameSession::load_game(game, save, error));
        for (int i = 0; i < 10; ++i)
            CHECK(step_app_session(*game, 1.0 / 60));
        CHECK(check_restored_publications(*game, error));
        for (int m = 0; m < 2; ++m)
        {
            presenter.shading = modes[m];
            CHECK(frame(gpu, presenter, *game, capture, restored[m], error));
            CHECK(restored[m] == saved[m]);
        }
        std::cout << world << ": " << saved[0].sky_order << " sky draws (" << saved[0].sky_world << " world, "
                  << saved[0].sky_sprites << " sprite), " << saved[0].grids << " PolyGrids (" << saved[0].grid_draws
                  << " draws), identical after the load with both renderers\n";
        game->unload_world();
    }
    SDL_DestroyGPUDevice(gpu);
    SDL_Quit();
    std::cout << "GPU sky and PolyGrid republication after a load presents each once\n";
    return 0;
}
