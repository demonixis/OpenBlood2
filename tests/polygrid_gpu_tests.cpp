#include "app/polygrid_scene.h"
#include "blood_splat_fixture.h"
#include "engine/capture.h"
#include "engine/runtime.h"
#include "engine/world_renderer.h"
#include <cmath>
using namespace OpenBlood2;
#define CHECK(x)                                                                                                       \
    do                                                                                                                 \
    {                                                                                                                  \
        if (!(x))                                                                                                      \
        {                                                                                                              \
            std::cerr << __LINE__ << ": " #x " " << error << " " << SDL_GetError() << "\n";                            \
            return 1;                                                                                                  \
        }                                                                                                              \
    } while (false)
namespace
{
    bool render(Runtime& runtime, WorldRenderer& renderer, PolyGridView& scene, DVector eye, DRotation camera,
                Capture& capture, std::vector<std::uint8_t>& pixels, std::string& error,
                WorldRenderer* world_renderer = nullptr, const WorldMesh* world = nullptr,
                std::span<const WorldDraw> world_draws = {})
    {
        if ((!capture.texture && !capture.initialize(runtime.gpu, SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM, 400, 300)) ||
            !renderer.prepare(scene.mesh, scene.revision, SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM, 400, 300, error, true))
            return false;
        auto* commands = SDL_AcquireGPUCommandBuffer(runtime.gpu);
        if (!commands)
            return false;
        bool drawn = true;
        if (world_renderer && world)
        {
            drawn = world_renderer->draw(commands, capture.texture, *world, world_draws, eye, camera, false, error) &&
                    world_renderer->draw(commands, capture.texture, *world, world_draws, eye, camera, false, error,
                                         DrawLoad::Load, nullptr, WorldPass::Translucent);
        }
        else
            drawn = renderer.draw(commands, capture.texture, scene.mesh, scene.draws, eye, camera, false, error);
        drawn = drawn && renderer.draw(commands, capture.texture, scene.mesh, scene.draws, eye, camera, false, error,
                                       DrawLoad::Load, world_renderer ? world_renderer->depth_target() : nullptr,
                                       WorldPass::Translucent);
        if (!drawn)
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
    if (argc != 2 && argc != 3)
        return 2;
    Runtime runtime;
    std::string error;
    CHECK(runtime.initialize(true, error));
    std::cout << "GPU device: "
              << SDL_GetStringProperty(SDL_GetGPUDeviceProperties(runtime.gpu), SDL_PROP_GPU_DEVICE_NAME_STRING,
                                       "unknown")
              << " | backend " << SDL_GetGPUDeviceDriver(runtime.gpu) << "\n";
    Resources resources;
    CHECK(test_blood_splat::mount(resources, argv[1], error));
    WorldRenderer renderer(runtime.gpu);
    Capture capture;
    PolyGridView scene;
    std::array<DVector, 256> white;
    white.fill({1, 1, 1});
    std::array<std::int8_t, 4> heights{};
    auto rotation = rotation_from_euler({-1.57079632679f, 0, 0});
    CHECK(scene.add(1, true, {{0, 0, 4}, rotation}, {3, 0, 2}, 2, heights, white, {2, 1}, {0, 0}, .5f, "alpha.dtx",
                    resources, error));
    std::vector<std::uint8_t> pixels;
    CHECK(render(runtime, renderer, scene, {}, {}, capture, pixels, error));
    unsigned red = 0, green = 0, black = 0, transparent = 0, runs = 0;
    bool previous = false;
    for (unsigned x = 51; x < 349; ++x)
    {
        const auto* p = pixels.data() + (150 * 400 + x) * 4;
        const bool is_red =
            std::abs(int(p[0]) - 131) <= 2 && std::abs(int(p[1]) - 4) <= 2 && std::abs(int(p[2]) - 6) <= 2;
        red += is_red;
        if (is_red && !previous)
            ++runs;
        previous = is_red;
        green += p[1] > 60 && p[1] < 80 && p[0] < 10;
        black += p[0] >= 2 && p[0] <= 4 && p[1] >= 3 && p[1] <= 5 && p[2] >= 5 && p[2] <= 8;
        transparent += p[0] == 6 && p[1] == 9 && p[2] == 13;
    }
    std::cout << "Repeat/alpha scan: red=" << red << " green=" << green << " black=" << black
              << " clear=" << transparent << " red runs=" << runs << "\n";
    CHECK(red > 50 && green > 50 && black > 50 && transparent > 50 && runs == 2);
    CHECK(capture.save((std::filesystem::path(argv[1]) / "polygrid-repeat-alpha.png").c_str()));
    if (argc == 3)
    {
        GameSession g;
        Config config;
        config.data_dir = argv[2];
        CHECK(g.initialize() && g.resources.mount_game(config, error));
        for (unsigned cycle = 0; cycle < 2; ++cycle)
        {
            CHECK(g.request_world("02_CityHub", error) && g.enter_player(error));
            CHECK(g.step(1.0 / 60) && !g.server.polygrids.active.empty() && g.server.polygrids.failed == 0);
            scene.clear();
            renderer.clear_geometry();
            CHECK(update_polygrid_scene(g, scene, error));
            const auto& fx = g.server.polygrids.active.front().effect;
            DVector eye = vector_add(fx.pose.position, {0, 300, -100});
            DRotation camera;
            CHECK(align_rotation(vector_subtract(fx.pose.position, eye), camera));
            CHECK(render(runtime, renderer, scene, eye, camera, capture, pixels, error));
            unsigned ink = 0;
            for (unsigned k = 0; k < pixels.size(); k += 4)
                ink += pixels[k] != 6 || pixels[k + 1] != 9 || pixels[k + 2] != 13;
            CHECK(ink > 1000 && renderer.stats.alpha_batches > 0);
            CHECK(capture.save(
                (std::filesystem::path(argv[1]) / ("cityhub-water-" + std::to_string(cycle) + ".png")).c_str()));
            std::cout << "CityHub cycle " << cycle << " surface batches " << renderer.stats.alpha_batches << " changed pixels "
                      << ink << "\n";
            auto prior = pixels;
            CHECK(g.step(.1) && update_polygrid_scene(g, scene, error) &&
                  render(runtime, renderer, scene, eye, camera, capture, pixels, error));
            CHECK(pixels != prior);
            WorldRenderer world_renderer(runtime.gpu);
            WorldMesh world;
            std::vector<WorldDraw> world_draws;
            CHECK(world.build(g.world, error, &g.resources) &&
                  OpenBlood2::world_draws(world, g.objects, world_draws, world_renderer.stats, error));
            CHECK(
                world_renderer.prepare(world, g.world_revision, SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM, 400, 300, error));
            // Authored canal from an above-bank diagnostic pose; unchanged original session.
            eye = vector_add(fx.pose.position, {250, 110, -200});
            CHECK(align_rotation(vector_subtract(vector_add(fx.pose.position, {0, 0, 500}), eye), camera));
            CHECK(render(runtime, renderer, scene, eye, camera, capture, pixels, error, &world_renderer, &world,
                         world_draws));
            CHECK(capture.save(
                (std::filesystem::path(argv[1]) / ("cityhub-world-water-" + std::to_string(cycle) + ".png")).c_str()));
            auto with_water = pixels;
            scene.draws.clear();
            CHECK(render(runtime, renderer, scene, eye, camera, capture, pixels, error, &world_renderer, &world,
                         world_draws));
            unsigned changed = 0;
            for (unsigned k = 0; k < pixels.size(); k += 4)
                changed += pixels[k] != with_water[k] || pixels[k + 1] != with_water[k + 1] ||
                           pixels[k + 2] != with_water[k + 2];
            CHECK(changed > 500);
            std::cout << "World water contribution " << changed << " pixels\n";
            CHECK(g.unload_world());
            scene.clear();
            renderer.clear_geometry();
            CHECK(update_polygrid_scene(g, scene, error) && scene.entries.empty());
        }
    }
    return 0;
}
