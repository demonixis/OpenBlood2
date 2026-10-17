#include "app/sprite_scene.h"
#include "blood_spurt_fixture.h"
#include "engine/capture.h"
#include "engine/runtime.h"
#include "engine/world_renderer.h"
#include "game/original/Weapon.h"
#include "game/original/WeaponFX.h"
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
    if (argc != 3)
        return 2;
    Runtime runtime;
    std::string error;
    CHECK(runtime.initialize(true, error));
    std::cout << "GPU device: "
              << SDL_GetStringProperty(SDL_GetGPUDeviceProperties(runtime.gpu), SDL_PROP_GPU_DEVICE_NAME_STRING,
                                       "unreported")
              << " | backend " << SDL_GetGPUDeviceDriver(runtime.gpu) << "\n";
    WorldRenderer renderer(runtime.gpu, DrawStyle::SpriteAlpha);
    Capture capture;
    SpriteView scene;
    GameSession g;
    Config config;
    config.data_dir = argv[2];
    CHECK(g.initialize() && g.resources.mount_game(config, error));
    std::error_code ec;
    std::filesystem::create_directories(argv[1], ec);
    CHECK(!ec);
    std::uint64_t last = 0;
    for (int cycle = 0; cycle < 3; ++cycle)
    {
        CHECK(g.request_world("01_SubwayA", error) && g.objects.clear());
        auto owner = g.objects.create(g.objects.find_class("BaseClass"), {});
        CHECK(owner);
        g.client_shell.view.position = {0, 10, -35};
        g.client_shell.view.rotation = {};
        CWeaponFX fx;
        fx.m_nAmmoType = AMMO_BULLET;
        std::minstd_rand rng(1);
        fx.m_eSurfaceType = SURFTYPE_FLESH;
        fx.m_vDestPos = {-8, 10, 0};
        CHECK(fx.CreateSparks(g.server, owner, {0, 0, -1}, 50, false, rng));
        fx.m_eSurfaceType = SURFTYPE_METAL;
        fx.m_vDestPos = {8, 10, 0};
        CHECK(fx.CreateSparks(g.server, owner, {0, 0, -1}, 50, false, rng));
        CHECK(g.server.sparks.active.front().id > last);
        last = g.server.sparks.active.back().id;
        const auto count = g.server.sparks.live_particles();
        CHECK(update_sprite_scene(g, scene, error) && scene.entries.size() == count);
        for (const auto& e : scene.entries)
            CHECK(e.spark_system && e.error.empty());
        const auto callbacks = g.server.sparks.active[0].updates;
        CHECK(update_sprite_scene(g, scene, error) && g.server.sparks.active[0].updates == callbacks);
        std::vector<std::uint8_t> pixels;
        CHECK(render(runtime, renderer, scene, g.client_shell.view.position, {}, capture, pixels, error));
        unsigned red = 0, yellow = 0;
        for (unsigned i = 0; i < pixels.size(); i += 4)
        {
            if (pixels[i] > pixels[i + 1] * 2 && pixels[i] > 25)
                ++red;
            if (pixels[i] > pixels[i + 2] + 20 && pixels[i + 1] > pixels[i + 2] + 20)
                ++yellow;
        }
        CHECK(red > 50 && yellow > 5 && red + yellow < 400 * 300 / 2);
        CHECK(capture.save((std::filesystem::path(argv[1]) / "impact-sparks.png").c_str()));
        CHECK(g.objects.remove(owner) && g.objects.finish_frame());
        CHECK(g.objects.advance(.25) && g.server.sparks.advance(g.objects.time()));
        CHECK(std::abs(g.server.sparks.active[0].alpha - .875f) < .001);
        CHECK(update_sprite_scene(g, scene, error));
        CHECK(render(runtime, renderer, scene, g.client_shell.view.position, {}, capture, pixels, error));
        CHECK(capture.save((std::filesystem::path(argv[1]) / "impact-sparks-moving.png").c_str()));
        CHECK(g.objects.advance(.75) && g.server.sparks.advance(g.objects.time()));
        CHECK(g.objects.advance(1) && g.server.sparks.advance(g.objects.time()));
        CHECK(g.objects.advance(.1) && g.server.sparks.advance(g.objects.time()) && g.server.sparks.active.empty());
        CHECK(update_sprite_scene(g, scene, error) && scene.entries.empty());
        CHECK(render(runtime, renderer, scene, g.client_shell.view.position, {}, capture, pixels, error));
        for (unsigned i = 0; i < pixels.size(); i += 4)
            CHECK(pixels[i] == 6 && pixels[i + 1] == 9 && pixels[i + 2] == 13);
        std::cout << "cycle " << cycle << ": " << count << " source particles, red pixels " << red << ", yellow pixels "
                  << yellow << ", expiry clears GPU image\n";
        CHECK(g.unload_world() && g.server.sparks.reports.empty());
        renderer.clear_geometry();
        scene.clear();
    }
    return 0;
}
