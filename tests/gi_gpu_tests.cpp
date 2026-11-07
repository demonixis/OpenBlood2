// GI lot G8 on retail worlds (gi_contracts.md): with GLOBAL ILLUMINATION on, a missing, stale or
// zero (E = 0) Ready file leaves the presented frame byte-identical to GI off, in the original and
// the enhanced renderer and at DETAIL LOW; a nonzero file changes it, the Original view restores it,
// and switching GI off again gives the reference frame back. Lot G9: zero light probes leave models and
// the Light probes world source identical; nonzero probes change the models only, and the switch,
// intensity 0 and the Original view restore the frame. Lot GD2: a DIR section equal to O - A_eff with
// E = 0 is the reference frame; the re-baked D' changes the lightmapped frame only, and the Original
// view, DETAIL LOW and GI off keep the reference.
#include "app/scene_presenter.h"
#include "app/session_playback.h"
#include "engine/capture.h"
#include "engine/gi_bake_scene.h"
#include "engine/gi_direct.h"
#include "engine/gi_lightmap.h"
#include "engine/sha256.h"
#include "engine/world_shaders.h"
#include "game/local_scene.h"
#include <SDL3/SDL.h>
#include <filesystem>
#include <cmath>
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
    // LMAP/VTX of uniform E, and (lot G9) PLAC/PROB: a probe at every node of a 256-unit lattice over the
    // world with uniform E = probe_energy.
    // `direct` (lot GD2): 0 no DIR section, 1 D = O - A_eff, 2 the re-baked D'.
    bool gi_file(const World& world, const Resources& resources, std::string_view path, float energy,
                 float probe_energy, GiFile& file, std::string& error, int direct = 0)
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
        GiLightLattice lattice;
        lattice.spacing = 256;
        lattice.origin = world.min;
        lattice.dims = {std::uint32_t(std::ceil((world.max.x - world.min.x) / 256)) + 1,
                        std::uint32_t(std::ceil((world.max.y - world.min.y) / 256)) + 1,
                        std::uint32_t(std::ceil((world.max.z - world.min.z) / 256)) + 1};
        GiProbes probes;
        for (std::uint32_t node = 0; node < lattice.dims[0] * lattice.dims[1] * lattice.dims[2]; ++node)
        {
            lattice.probes.push_back({node, lattice.node_position(node)});
            for (int k = 0; k < 27; ++k)
                probes.sh.push_back(k < 3 ? float(probe_energy / 0.28209479177387814) : 0.f);
            probes.valid.push_back(1);
        }
        file = {};
        file.sections.resize(4);
        file.sections[0].tag = gi_format::placement;
        file.sections[1].tag = gi_format::lightmaps;
        file.sections[2].tag = gi_format::vertices;
        file.sections[3].tag = gi_format::probes;
        if (!encode_gi_placement_section(lattice, {}, file.sections[0].payload, error) ||
            !write_gi_lightmaps(maps, file.sections[1].payload, error) ||
            !write_gi_vertex_light(vertex, file.sections[2].payload, error) ||
            !write_gi_probes(probes, file.sections[3].payload, error))
            return false;
        if (direct)
        {
            GiDirectSection section;
            if (direct == 1)
            {
                const float ambient[3]{receivers.ambient.x, receivers.ambient.y, receivers.ambient.z};
                for (std::size_t t = 0; t < receivers.texels; ++t)
                {
                    float d[3];
                    for (int c = 0; c < 3; ++c)
                    {
                        const double o = receivers.original[t][c];
                        const double a = ambient[c] > 0 ? std::min(o, double(ambient[c])) : 0.0;
                        d[c] = float((o - a) / 255);
                    }
                    section.texels.push_back(gi_rgb9e5({d[0], d[1], d[2]}));
                }
            }
            else
            {
                GiDirectBake bake;
                if (!gi_rebake_direct(world, receivers, 0, nullptr, {}, bake, error))
                    return false;
                section = std::move(bake.section);
            }
            GiSection dir{gi_format::direct, {}};
            if (!write_gi_direct(section, dir.payload, error))
                return false;
            file.sections.insert(file.sections.begin() + 2, std::move(dir));
        }
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
        GiFile zero, lit, probed, exact_direct, rebaked;
        CHECK(gi_file(scene.geometry, game->resources, path, 0, 0, zero, error));
        CHECK(gi_file(scene.geometry, game->resources, path, 0, 0, exact_direct, error, 1));
        CHECK(gi_file(scene.geometry, game->resources, path, 0, 0, rebaked, error, 2));
        CHECK(gi_file(scene.geometry, game->resources, path, .05f, 0, lit, error));
        CHECK(gi_file(scene.geometry, game->resources, path, 0, .4f, probed, error)); // Above AmbientLight.
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
                presenter.gi_probes = {};
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
                CHECK(presenter.gi_status().probes > 0 && presenter.model_lighting.stats.probe_lit > 0);
                // The zero probes also leave the Light probes world source identical.
                presenter.gi_composition.source = GiWorldSource::Probes;
                CHECK(frame(gpu, presenter, *game, capture, pixels, error) && presenter.rebuilt && pixels == reference);
                presenter.gi_composition.source = GiWorldSource::Texels;
                // Lot G9: nonzero probes light the models only (E = 0 keeps the world); the switch,
                // intensity 0 and the Original view restore the frame; as the world source they change it.
                CHECK(save_gi_file(target, probed, error));
                ++presenter.gi_reload;
                CHECK(frame(gpu, presenter, *game, capture, pixels, error));
                CHECK(presenter.gi_status().state == GiRuntimeState::Applied && pixels != reference);
                const auto lit_models = presenter.model_lighting.stats.probe_lit;
                CHECK(lit_models > 0 && presenter.model_lighting.stats.probe_missing == 0);
                presenter.gi_probes.models = false;
                CHECK(frame(gpu, presenter, *game, capture, pixels, error) && !presenter.rebuilt && pixels == reference);
                CHECK(presenter.model_lighting.stats.probe_lit == 0);
                presenter.gi_probes = {true, 0};
                CHECK(frame(gpu, presenter, *game, capture, pixels, error) && pixels == reference);
                presenter.gi_probes = {};
                presenter.gi_composition.view = GiView::Original;
                CHECK(frame(gpu, presenter, *game, capture, pixels, error) && pixels == reference);
                presenter.gi_composition.view = GiView::Final;
                presenter.gi_composition.source = GiWorldSource::Probes;
                presenter.gi_probes.models = false;
                CHECK(frame(gpu, presenter, *game, capture, pixels, error) && presenter.rebuilt && pixels != reference);
                presenter.gi_composition.source = GiWorldSource::Texels;
                presenter.gi_probes.models = true;
                std::cout << label << ": probes lit " << lit_models << " models\n";
                // Lot GD2: a DIR section of O - A_eff with E = 0 is the identity; the re-baked D'
                // changes the lightmapped frame, not DETAIL LOW; the Original view restores it.
                CHECK(save_gi_file(target, exact_direct, error));
                ++presenter.gi_reload;
                CHECK(frame(gpu, presenter, *game, capture, pixels, error));
                CHECK(presenter.gi_status().state == GiRuntimeState::Applied && presenter.gi_status().rebaked &&
                      pixels == reference);
                CHECK(save_gi_file(target, rebaked, error));
                ++presenter.gi_reload;
                CHECK(frame(gpu, presenter, *game, capture, pixels, error));
                CHECK(presenter.gi_status().state == GiRuntimeState::Applied && presenter.gi_status().rebaked &&
                      (lightmaps ? pixels != reference : pixels == reference));
                presenter.gi_composition.view = GiView::Original;
                CHECK(frame(gpu, presenter, *game, capture, pixels, error) && pixels == reference);
                presenter.gi_composition.view = GiView::Final;
                std::cout << label << ": re-baked direct light " << (lightmaps ? "changed" : "kept") << " the frame\n";
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
