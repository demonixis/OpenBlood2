// Enhanced (physically based) renderer on retail worlds: the same presented frame with the
// original shaders, the standard and high profiles and generated normal maps. The enhanced
// frames must stay close to the original look, normal maps must be generated, and switching
// back must reproduce the original frame byte for byte.
#include "app/scene_presenter.h"
#include "app/session_playback.h"
#include "engine/capture.h"
#include "engine/world_shaders.h"
#include <SDL3/SDL.h>
#include <cmath>
#include <iostream>
#include <regex>
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
    // Mean absolute channel difference and the share of channels that moved by more than 64.
    void compare(const std::vector<Uint8>& a, const std::vector<Uint8>& b, double& mean, double& large)
    {
        std::uint64_t sum = 0, big = 0, channels = 0;
        for (std::size_t i = 0; i < a.size(); i += 4)
            for (int c = 0; c < 3; ++c, ++channels)
            {
                const int d = std::abs(int(a[i + c]) - int(b[i + c]));
                sum += unsigned(d);
                big += d > 64;
            }
        mean = double(sum) / double(channels);
        large = double(big) / double(channels);
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
    for (const char* world : {"02_CityHub", "01_SubwayA"})
    {
        Config config;
        config.data_dir = argv[1];
        auto game = std::make_unique<GameSession>();
        CHECK(game->initialize() && game->resources.mount_game(config, error) && game->request_world(world, error) &&
              game->run_world(error));
        for (int i = 0; i < 30; ++i)
            CHECK(step_app_session(*game, 1.0 / 60));
        Capture capture;
        CHECK(capture.initialize(gpu, SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM, 960, 540));
        ScenePresenter presenter(gpu);
        // The shading alone (gpu.post covers the rest).
        presenter.post = {Antialiasing::Off, false, Tonemap::Off, AmbientOcclusion::Off};
        std::vector<Uint8> original, standard, high, again;
        CHECK(frame(gpu, presenter, *game, capture, original, error));
        CHECK(presenter.pipelines.shading() == SurfaceShading::Original && !presenter.world_renderer.surface);
        presenter.shading = SurfaceShading::Standard;
        CHECK(frame(gpu, presenter, *game, capture, standard, error));
        CHECK(presenter.pipelines.shading() == SurfaceShading::Standard && presenter.world_renderer.surface &&
              presenter.pipelines.get(WorldPipeline::SurfaceOpaque));
        CHECK(presenter.surface_stats.grid_lights > 0 && presenter.surface_stats.grid_cells > 0);
        CHECK(presenter.normal_map_cache.stats.generated == 0);
        presenter.shading = SurfaceShading::High;
        presenter.normal_maps = true;
        CHECK(frame(gpu, presenter, *game, capture, high, error));
        CHECK(presenter.normal_map_cache.stats.generated > 50);
        // Material inspection follows the world and the settings (roadmap 5.5).
        const auto report = presenter.material_report;
        std::cout << world << ": report " << report.materials.size() << " materials, " << report.generated << " maps, "
                  << report.fullbright_faces << "/" << presenter.world_mesh.fullbright << " bright faces, "
                  << report.unbound_overrides.size() << " unbound\n";
        CHECK(report.materials.size() > 40 && report.generated > 30 && report.unbound_overrides.empty() &&
              report.fullbright_faces == presenter.world_mesh.fullbright);
        // Emission only adds light (roadmap 5.2): the same frame with every emissive value removed
        // from the manifest is nowhere brighter.
        std::vector<Uint8> dark;
        const std::string unlit =
            std::regex_replace(std::string(builtin_surface_manifest()),
                               std::regex(R"(, "emissive": [0-9.]+, "emissive_threshold": [0-9.]+)"), "");
        CHECK(unlit.find("emissive\"") == std::string::npos && presenter.surface_manifest.parse(unlit, error));
        presenter.shading = SurfaceShading::Standard;
        presenter.normal_maps = false;
        CHECK(frame(gpu, presenter, *game, capture, dark, error) && presenter.material_report.emissive == 0);
        std::size_t glowing = 0;
        for (std::size_t i = 0; i < dark.size(); i += 4)
        {
            bool brighter = false;
            for (int c = 0; c < 3; ++c)
            {
                CHECK(int(standard[i + c]) + 1 >= int(dark[i + c]));
                brighter |= standard[i + c] > dark[i + c] + 1;
            }
            glowing += brighter;
        }
        std::cout << world << ": " << report.emissive << " emissive materials, " << report.fullbright_faces
                  << " fully bright faces, " << glowing << " pixels lit by emission\n";
        CHECK(presenter.surface_manifest.parse_builtin(error));
        presenter.shading = SurfaceShading::Original;
        presenter.normal_maps = false;
        CHECK(frame(gpu, presenter, *game, capture, again, error));
        CHECK(!presenter.pipelines.get(WorldPipeline::SurfaceOpaque) && !presenter.world_renderer.surface);
        double standard_mean = 0, standard_large = 0, high_mean = 0, high_large = 0;
        compare(original, standard, standard_mean, standard_large);
        compare(original, high, high_mean, high_large);
        std::cout << world << ": " << presenter.surface_stats.grid_lights << " grid lights in "
                  << presenter.surface_stats.grid_cells << " cells (" << presenter.surface_stats.grid_build_ns / 1000
                  << " us), " << presenter.normal_map_cache.stats.generated << " normal maps ("
                  << presenter.normal_map_cache.stats.generation_ns / 1000000 << " ms); mean difference standard "
                  << standard_mean << " high " << high_mean << ", channels beyond 64: standard " << standard_large
                  << " high " << high_large << '\n';
        // The enhanced frames change the image without departing from the original look.
        CHECK(original != standard && original != high && standard != high);
        CHECK(standard_mean < 12 && high_mean < 12 && standard_large < .02 && high_large < .02);
        // The original renderer remains the untouched reference.
        CHECK(again == original);
        game->unload_world();
    }
    SDL_DestroyGPUDevice(gpu);
    SDL_Quit();
    std::cout << "GPU enhanced renderer stays close to the original and leaves it intact\n";
    return 0;
}
