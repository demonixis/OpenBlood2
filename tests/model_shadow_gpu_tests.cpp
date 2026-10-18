// FLAG_SHADOW model shadows on retail CityHub: the same presented frame with and without
// shadows. Shadows must only darken (never brighten) and must darken a visible area.
#include "app/scene_presenter.h"
#include "app/session_playback.h"
#include "engine/capture.h"
#include "engine/world_shaders.h"
#include <SDL3/SDL.h>
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
    if (argc != 2)
        return 2;
    CHECK(SDL_Init(SDL_INIT_VIDEO));
    auto* gpu = SDL_CreateGPUDevice(WorldShaderFormats, true, nullptr);
    CHECK(gpu);
    std::cout << "GPU=" << SDL_GetStringProperty(SDL_GetGPUDeviceProperties(gpu), SDL_PROP_GPU_DEVICE_NAME_STRING, "?")
              << " backend=" << SDL_GetGPUDeviceDriver(gpu) << '\n';
    {
        Config config;
        config.data_dir = argv[1];
        auto game = std::make_unique<GameSession>();
        CHECK(game->initialize() && game->resources.mount_game(config, error) &&
              game->request_world("02_CityHub", error) && game->run_world(error));
        for (int i = 0; i < 30; ++i)
            CHECK(step_app_session(*game, 1.0 / 60));
        Capture capture;
        CHECK(capture.initialize(gpu, SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM, 960, 540));
        ScenePresenter presenter(gpu);
        std::vector<Uint8> shadowed, plain;
        CHECK(frame(gpu, presenter, *game, capture, shadowed, error));
        const auto stats = presenter.shadow_stats;
        CHECK(stats.available && stats.candidates > stats.casters && stats.casters > 0 && presenter.stats.passes == 2);
        presenter.model_shadows = false;
        CHECK(frame(gpu, presenter, *game, capture, plain, error));
        CHECK(presenter.shadow_stats.casters == 0 && presenter.stats.passes == 1);
        std::size_t darker = 0, brighter = 0;
        for (std::size_t i = 0; i < plain.size(); i += 4)
        {
            bool any_darker = false;
            for (int c = 0; c < 3; ++c)
            {
                brighter += shadowed[i + c] > plain[i + c];
                any_darker |= shadowed[i + c] < plain[i + c];
            }
            darker += any_darker;
        }
        std::cout << "CityHub: " << stats.candidates << " FLAG_SHADOW candidates, " << stats.casters
                  << " casters drawn; " << darker << " pixels darkened, " << brighter << " channels brighter\n";
        CHECK(darker > 200 && brighter == 0);
        game->unload_world();
    }
    SDL_DestroyGPUDevice(gpu);
    SDL_Quit();
    std::cout << "GPU model shadows darken only and require casters\n";
    return 0;
}
