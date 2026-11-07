// GI lot G8 on retail worlds (gi_contracts.md): with GLOBAL ILLUMINATION on, a missing, stale or
// zero (E = 0) Ready file leaves the presented frame byte-identical to GI off, in the original and
// the enhanced renderer and at DETAIL LOW; a nonzero file changes it, the Original view restores it,
// and switching GI off again gives the reference frame back.
#include "app/scene_presenter.h"
#include "app/session_playback.h"
#include "engine/capture.h"
#include "engine/gi_bake_scene.h"
#include "engine/gi_lightmap.h"
#include "engine/sha256.h"
#include "engine/world_shaders.h"
#include "game/local_scene.h"
#include <SDL3/SDL.h>
#include <filesystem>
#include <iostream>
using namespace OpenBlood2;
namespace fs = std::filesystem;
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
    // A file of uniform E for every receiver of the world, keyed for the mounted content.
    bool gi_file(const World& world, const Resources& resources, std::string_view path, float energy, GiFile& file,
                 std::string& error)
    {
        WorldMesh mesh;
        std::vector<WorldTriangleSource> sources;
        std::vector<GiRangeUse> use;
        GiSceneReport report;
        GiBakeScene scene;
        GiReceivers receivers;
        GiExpectation expected;
        if (!mesh.build(world, error, &resources, true, &sources) || !gi_range_uses(world, {}, use, report, error) ||
            !scene.build(std::move(mesh), use, report, error) ||
            !gi_receivers(world, scene, sources, use, 0, receivers, error) ||
            !gi_expectation(resources, path, expected, error))
            return false;
        GiIndirect indirect;
        indirect.energy.assign(receivers.elements() * 3, energy);
        indirect.valid.assign(receivers.elements(), 1);
        GiLightmaps maps;
        GiVertexLight vertex;
        gi_lightmap_sections(receivers, indirect, maps, vertex);
        file = {};
        file.sections.resize(2);
        file.sections[0].tag = gi_format::lightmaps;
        file.sections[1].tag = gi_format::vertices;
        if (!write_gi_lightmaps(maps, file.sections[0].payload, error) ||
            !write_gi_vertex_light(vertex, file.sections[1].payload, error))
            return false;
        file.key.world = expected.world;
        file.key.albedo = expected.albedo;
        file.key.edition = expected.edition;
        file.key.world_path = std::string(path);
        file.key.settings = sha256({});
        return true;
    }
} // namespace
int main(int argc, char** argv)
{
    std::string error;
    if (argc != 3)
        return 2;
    const fs::path scratch = argv[2];
    std::error_code ec;
    fs::remove_all(scratch, ec);
    CHECK(fs::create_directories(scratch / "worlds", ec));
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
        const auto scene = local_scene(*game);
        const std::string path(scene.world_path);
        std::string name;
        GiExpectation expected;
        CHECK(gi_expectation(game->resources, path, expected, error) && gi_file_name(path, expected.world, name));
        const auto target = scratch / path_from_utf8(name);
        fs::remove(target, ec);
        GiFile zero, lit;
        CHECK(gi_file(scene.geometry, game->resources, path, 0, zero, error));
        CHECK(gi_file(scene.geometry, game->resources, path, .05f, lit, error));
        Capture capture;
        CHECK(capture.initialize(gpu, SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM, 960, 540));
        ScenePresenter presenter(gpu);
        presenter.post = {Antialiasing::Off, false, Tonemap::Off, AmbientOcclusion::Off, Reflections::Off};
        presenter.gi_roots = {{"--gi-dir", path_text(scratch)}};
        for (const auto shading : {SurfaceShading::Original, SurfaceShading::Standard})
            for (const bool lightmaps : {true, false})
            {
                presenter.shading = shading;
                presenter.lightmaps = lightmaps;
                presenter.global_illumination = false;
                presenter.gi_composition = {};
                fs::remove(target, ec);
                std::vector<Uint8> reference, pixels;
                CHECK(frame(gpu, presenter, *game, capture, reference, error));
                const auto label = std::string(world) +
                                   (shading == SurfaceShading::Original ? " original" : " enhanced") +
                                   (lightmaps ? " high" : " low");
                // On, no file: the original light.
                presenter.global_illumination = true;
                ++presenter.gi_reload;
                CHECK(frame(gpu, presenter, *game, capture, pixels, error));
                CHECK(presenter.gi_status().state == GiRuntimeState::Missing && pixels == reference);
                // A stale file is never applied.
                auto stale = lit;
                stale.key.albedo[0] ^= 1;
                CHECK(save_gi_file(target, stale, error));
                ++presenter.gi_reload;
                CHECK(frame(gpu, presenter, *game, capture, pixels, error));
                CHECK(presenter.gi_status().state == GiRuntimeState::Missing && pixels == reference);
                // E = 0 at Final (k 2, f 1) is the exact identity.
                CHECK(save_gi_file(target, zero, error));
                ++presenter.gi_reload;
                CHECK(frame(gpu, presenter, *game, capture, pixels, error));
                CHECK(presenter.gi_status().state == GiRuntimeState::Applied && pixels == reference);
                CHECK(presenter.world_mesh.gi_blocks + presenter.world_mesh.gi_sets > 0);
                // Nonzero E changes the frame; the Original view and GI off restore it.
                CHECK(save_gi_file(target, lit, error));
                ++presenter.gi_reload;
                CHECK(frame(gpu, presenter, *game, capture, pixels, error));
                CHECK(presenter.gi_status().state == GiRuntimeState::Applied && pixels != reference);
                std::size_t brighter = 0, darker = 0;
                for (std::size_t i = 0; i < pixels.size(); i += 4)
                    for (int c = 0; c < 3; ++c)
                        if (pixels[i + c] > reference[i + c])
                            ++brighter;
                        else if (pixels[i + c] < reference[i + c])
                            ++darker;
                presenter.gi_composition.view = GiView::Original;
                CHECK(frame(gpu, presenter, *game, capture, pixels, error) && presenter.rebuilt && pixels == reference);
                presenter.gi_composition.view = GiView::Lighting;
                CHECK(frame(gpu, presenter, *game, capture, pixels, error) && pixels != reference);
                presenter.global_illumination = false;
                CHECK(frame(gpu, presenter, *game, capture, pixels, error) && presenter.rebuilt && pixels == reference);
                CHECK(presenter.gi_status().state == GiRuntimeState::Off);
                std::cout << label << ": missing, stale and zero files identical; E = 0.05 changed " << brighter
                          << " channels up, " << darker << " down\n";
            }
        game->unload_world();
    }
    SDL_DestroyGPUDevice(gpu);
    SDL_Quit();
    fs::remove_all(scratch, ec);
    std::cout << "GPU GI override leaves the frame intact unless a Ready nonzero file is applied\n";
    return 0;
}
