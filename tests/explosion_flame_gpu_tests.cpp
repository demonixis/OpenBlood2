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
    Capture capture;
    SpriteView scene;
    Resources resources;
    CHECK(test_explosion_flame::mount(resources, argv[1], error));
    std::vector<ExplosionFlameSettings> recipe;
    CHECK(original_explosion_flames(8, {0, -30, 20}, {0, 1, 0}, recipe, error));
    CExplosionFlameFX fx;
    std::minstd_rand random(1);
    CHECK(fx.Init(recipe[0], error) && fx.CreateObject(resources, 0, random, error) && fx.Update(.1, random));
    const auto& p = fx.particles.back();
    CHECK(p.color.x == 150.f / 255 && fx.alpha < 1);
    std::vector<std::uint8_t> white, tinted;
    for (bool tint : {false, true})
    {
        CHECK(scene.begin_frame({}, {}, error) &&
              scene.add_particle(fx.texture, {0, 0, 20}, fx.settings.radius / 640, tint ? p.color : DVector{1, 1, 1},
                                 tint ? fx.alpha : 1, fx.id, 0, resources, error));
        CHECK(scene.entries.back().error.empty());
        CHECK(render(runtime, renderer, scene, {}, {}, capture, tint ? tinted : white, error));
    }
    unsigned verified = 0;
    for (unsigned i = 0; i < 400 * 300; ++i)
    {
        const auto k = i * 4;
        if (white[k] == 255 && white[k + 1] == 0 && white[k + 2] == 0)
        {
            CHECK(std::abs(int(tinted[k]) - (255 * p.color.x * fx.alpha + 6 * (1 - fx.alpha))) <= 2);
            CHECK(std::abs(int(tinted[k + 1]) - 9 * (1 - fx.alpha)) <= 2 &&
                  std::abs(int(tinted[k + 2]) - 13 * (1 - fx.alpha)) <= 2);
            ++verified;
        }
    }
    CHECK(verified > 100);
    CHECK(capture.save((std::filesystem::path(argv[1]) / "flame-tint-alpha.png").c_str()));
    std::cout << "Flame RGB and system alpha: " << verified << " exact opaque-texel blend checks\n";
    if (argc == 3)
    {
        GameSession g;
        Config c;
        c.data_dir = argv[2];
        CHECK(g.initialize() && g.resources.mount_game(c, error));
        std::uint64_t previous = 0;
        for (unsigned cycle = 0; cycle < 2; ++cycle)
        {
            CHECK(g.request_world("01_SubwayA", error) && g.enter_player(error));
            auto h = test_explosion_flame::create(g);
            CHECK(h && test_explosion_flame::trigger(g, h) && g.step(.35));
            CHECK(!g.objects.get(h) && g.server.explosion_flames.active.size() == 1);
            const auto& active = g.server.explosion_flames.active[0];
            CHECK(active.id > previous);
            previous = active.id;
            const auto updates = active.updates;
            // Render the live session's checked flame entries without an inspector.
            scene.clear();
            renderer.clear_geometry();
            CHECK(scene.begin_frame({}, {}, error));
            for (unsigned i = 0; i < active.particles.size(); ++i)
            {
                const auto& v = active.particles[i];
                CHECK(scene.add_particle(active.texture, vector_add(active.settings.position, v.position),
                                         active.settings.radius / 640, v.color, active.alpha, active.id, i, g.resources,
                                         error) &&
                      scene.entries.back().error.empty());
            }
            CHECK(scene.entries.size() == 48 && scene.entries[0].partial_alpha > 0);
            scene.sort();
            CHECK(render(runtime, renderer, scene, {}, {}, capture, tinted, error) && active.updates == updates);
            unsigned ink = 0;
            for (unsigned i = 0; i < 400 * 300; ++i)
                if (tinted[i * 4] != 6 || tinted[i * 4 + 1] != 9 || tinted[i * 4 + 2] != 13)
                    ++ink;
            CHECK(ink > 100 && ink < 100000);
            CHECK(capture.save((std::filesystem::path(argv[1]) / "original-tesla-particles.png").c_str()));
            std::cout << "Private diagnostic Tesla GPU cycle " << cycle << ": " << ink << " covered pixels, "
                      << scene.entries[0].partial_alpha << " partial-alpha texels\n";
            CHECK(g.unload_world() && g.server.explosion_flames.active.empty());
            scene.clear();
            renderer.clear_geometry();
            CHECK(scene.draws.empty());
        }
    }
    return 0;
}
