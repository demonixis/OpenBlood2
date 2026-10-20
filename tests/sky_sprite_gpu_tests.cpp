#include "app/scene_presenter.h"
#include "app/session_playback.h"
#include "engine/capture.h"
#include "engine/world_shaders.h"
#include <SDL3/SDL.h>
#include <cmath>
#include <iostream>
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
    bool frame(SDL_GPUDevice* gpu, ScenePresenter& presenter, GameSession& game, Capture& capture,
               std::vector<Uint8>& pixels, std::string& error)
    {
        if (!presenter.update(game, error) || !presenter.render_world)
            return false;
        // Isolate authored sky layers for a pixel assertion independent of the player's room/ceiling.
        presenter.world_draws.clear();
        presenter.model_scene.world_draws.clear();
        presenter.model_scene.close_draws.clear();
        presenter.sprite_scene.batched_draws.clear();
        presenter.polygrid_scene.draws.clear();
        auto* commands = SDL_AcquireGPUCommandBuffer(gpu);
        if (!commands || !presenter.record(game, commands, capture.texture, SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM,
                                           capture.width, capture.height, error))
            return false;
        auto* copy = SDL_BeginGPUCopyPass(commands);
        SDL_GPUTextureRegion source{};
        source.texture = capture.texture;
        source.w = capture.width;
        source.h = capture.height;
        source.d = 1;
        SDL_GPUTextureTransferInfo destination{};
        destination.transfer_buffer = capture.transfer;
        destination.pixels_per_row = capture.width;
        destination.rows_per_layer = capture.height;
        SDL_DownloadFromGPUTexture(copy, &source, &destination);
        SDL_EndGPUCopyPass(copy);
        if (!SDL_SubmitGPUCommandBuffer(commands) || !SDL_WaitForGPUIdle(gpu))
            return false;
        auto* bytes = static_cast<const Uint8*>(SDL_MapGPUTransferBuffer(gpu, capture.transfer, false));
        if (!bytes)
            return false;
        pixels.assign(bytes, bytes + std::size_t(capture.width) * capture.height * 4);
        SDL_UnmapGPUTransferBuffer(gpu, capture.transfer);
        return true;
    }
} // namespace
int main(int argc, char** argv)
{
    std::string error;
    if (argc != 3)
        return 2;
    CHECK(SDL_Init(SDL_INIT_VIDEO));
    auto* gpu = SDL_CreateGPUDevice(WorldShaderFormats, true, nullptr);
    CHECK(gpu);
    {
        GameSession game;
        Config config;
        config.data_dir = argv[1];
        CHECK(game.initialize() && game.resources.mount_game(config, error));
        CHECK(game.request_world("multi/bb_crypt", error));
        game.server_shell.inspection_start = true;
        CHECK(game.run_world(error) && game.step(.02));
        const auto moon_id = game.server.sky.objects[2];
        auto* moon = game.objects.get(moon_id);
        CHECK(moon && moon->sprite && moon->state.m_ObjectType == OT_SPRITE);
        ScenePresenter presenter(gpu);
        presenter.model_shadows = false;
        CHECK(presenter.update(game, error));
        CHECK(presenter.sky_sprite_scene.draws.size() == 1 && presenter.sky_order.size() == 5);
        for (std::size_t slot = 0; slot < game.server.sky.objects.size(); ++slot)
            if (auto* object = game.objects.get(game.server.sky.objects[slot]))
                std::cout << "Sky slot=" << slot << " name=" << object->state.m_Name << "\n";
        CHECK(!presenter.sky_order[0].sprite && presenter.sky_order[1].sprite && !presenter.sky_order[2].sprite &&
              !presenter.sky_order[3].sprite && !presenter.sky_order[4].sprite);
        for (const auto& entry : presenter.sprite_scene.entries)
            CHECK(entry.object != moon_id);
        auto direction = vector_subtract(moon->state.m_Pos, presenter.sky_eye);
        game.client_shell.view.rotation = rotation_from_euler(
            {-std::atan2(direction.y, std::hypot(direction.x, direction.z)), std::atan2(direction.x, direction.z), 0});
        Capture capture;
        CHECK(capture.initialize(gpu, SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM, 640, 480));
        const std::filesystem::path root = argv[2];
        std::filesystem::create_directories(root);
        std::vector<Uint8> visible, hidden;
        CHECK(frame(gpu, presenter, game, capture, visible, error));
        CHECK(capture.save((root / "crypt-sky.png").c_str()));
        CHECK(presenter.sky_sprite_renderer.stats.draw_calls == 1);
        // Leave only the moon to verify its real texture, billboarding and alpha independently of cloud cover.
        for (auto id : game.server.sky.objects)
            if (auto* object = game.objects.get(id); object && id != moon_id)
                object->state.m_Flags &= ~FLAG_VISIBLE;
        CHECK(frame(gpu, presenter, game, capture, visible, error));
        CHECK(capture.save((root / "moon.png").c_str()));
        moon->state.m_Flags &= ~FLAG_VISIBLE;
        CHECK(frame(gpu, presenter, game, capture, hidden, error));
        std::size_t changed = 0;
        for (std::size_t i = 0; i < visible.size(); i += 4)
            changed += visible[i] != hidden[i] || visible[i + 1] != hidden[i + 1] || visible[i + 2] != hidden[i + 2];
        CHECK(changed > 20 && presenter.sky_sprite_scene.draws.empty());
        CHECK(game.unload_world() && presenter.update(game, error));
        CHECK(presenter.sky_order.empty() && presenter.sky_sprite_scene.entries.empty());
        std::cout << "Crypt sky: interleaved slot order, separate sprite pass, real moon pixels=" << changed
                  << ", hidden/unload cleanup passed\n";
    }
    SDL_DestroyGPUDevice(gpu);
    SDL_Quit();
    return 0;
}
