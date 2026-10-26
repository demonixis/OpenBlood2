#include "app/sprite_scene.h"
#include "engine/capture.h"
#include "engine/runtime.h"
#include "engine/world_renderer.h"
#include "explosion_flame_fixture.h"
#include <cmath>
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
    bool render(Runtime& runtime, WorldRenderer& renderer, SpriteView& scene, DVector eye, DRotation camera,
                Capture& capture, std::vector<std::uint8_t>& pixels, std::string& error)
    {
        if ((!capture.texture && !capture.initialize(runtime.gpu, SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM, 400, 300)) ||
            !renderer.prepare(scene.mesh, scene.revision, SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM, 400, 300, error, true))
            return false;
        auto* commands = SDL_AcquireGPUCommandBuffer(runtime.gpu);
        if (!commands)
            return false;
        if (!renderer.draw(commands, capture.texture, scene.mesh, scene.draws, eye, camera, false, error))
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
    WorldRenderer renderer(runtime.gpu, DrawStyle::SpriteAlpha);
    renderer.point_sprites = true; // Exact texel colours of the 4x2 fixture.
    Capture capture;
    SpriteView scene;
    Resources resources;
    CHECK(test_explosion_flame::mount(resources, argv[1], error));
    ParticleStreamSettings settings;
    settings.on = true;
    settings.min_velocity = settings.max_velocity = 0;
    settings.position_radius = 0;
    settings.min_color = settings.max_color = {128, 255, 255};
    settings.alpha = .5f;
    CParticleStreamFX fx;
    std::minstd_rand random(1);
    CHECK(fx.Init(settings, error) && fx.CreateObject(resources, {{0, 0, 20}, {}}, 0, random, error));
    std::vector<std::uint8_t> white, tinted;
    for (bool tint : {false, true})
    {
        CHECK(scene.begin_frame({}, {}, error) &&
              scene.add_particle(fx.texture, fx.position, fx.settings.radius / 640,
                                 tint ? fx.particles[0].color : DVector{1, 1, 1}, tint ? fx.settings.alpha : 1, 1, 0,
                                 resources, error));
        CHECK(scene.entries[0].error.empty() &&
              render(runtime, renderer, scene, {}, {}, capture, tint ? tinted : white, error));
    }
    unsigned verified = 0;
    for (unsigned i = 0; i < 400 * 300; ++i)
        if (white[i * 4] == 255 && white[i * 4 + 1] == 0 && white[i * 4 + 2] == 0)
        {
            CHECK(std::abs(int(tinted[i * 4]) - 67) <= 2 && std::abs(int(tinted[i * 4 + 1]) - 4.5f) <= 2 &&
                  std::abs(int(tinted[i * 4 + 2]) - 6.5f) <= 2);
            ++verified;
        }
    CHECK(verified > 100);
    std::cout << "Stream tint/alpha: " << verified << " checked pixels\n";
    CHECK(capture.save((std::filesystem::path(argv[1]) / "stream-alpha.png").c_str()));
    if (argc == 3)
    {
        GameSession g;
        Config c;
        c.data_dir = argv[2];
        CHECK(g.initialize() && g.resources.mount_game(c, error));
        std::uint64_t previous = 0;
        for (unsigned cycle = 0; cycle < 2; ++cycle)
        {
            CHECK(g.request_world("02_cityhub", error));
            for (unsigned i = 0; i < 90; ++i)
                CHECK(g.step(1. / 60));
            auto& streams = g.server.particle_streams;
            CHECK(streams.active.size() == 8 && !streams.failed);
            const auto& steam = streams.active[1].effect;
            CHECK(steam.id > previous);
            previous = steam.id;
            g.client_shell.view.position = vector_add(steam.position, {0, 0, -100});
            g.client_shell.view.rotation = {};
            const auto updates = steam.updates;
            scene.clear();
            renderer.clear_geometry();
            CHECK(update_sprite_scene(g, scene, error));
            unsigned quads = 0;
            for (auto& e : scene.entries)
                if (e.stream_system)
                {
                    CHECK(e.error.empty() && e.range != UINT32_MAX);
                    ++quads;
                }
            CHECK(quads == streams.particle_count() && quads > 30);
            CHECK(render(runtime, renderer, scene, g.client_shell.view.position, {}, capture, tinted, error));
            unsigned ink = 0;
            for (unsigned i = 0; i < 400 * 300; ++i)
                if (tinted[i * 4] != 6 || tinted[i * 4 + 1] != 9 || tinted[i * 4 + 2] != 13)
                    ++ink;
            CHECK(ink > 100 && steam.updates == updates);
            CHECK(capture.save((std::filesystem::path(argv[1]) / "cityhub-streams.png").c_str()));
            std::cout << "CityHub stream GPU cycle " << cycle << ": " << quads << " actual particle quads, " << ink
                      << " covered pixels\n";
            for (const auto& i : streams.active)
                CHECK(g.objects.remove(i.owner));
            CHECK(g.step(.02) && streams.active.empty());
            CHECK(update_sprite_scene(g, scene, error));
            for (auto& e : scene.entries)
                CHECK(!e.stream_system);
            CHECK(g.unload_world() && streams.reports.empty());
            scene.clear();
            renderer.clear_geometry();
        }
    }
    return 0;
}
