#include "app/sprite_scene.h"
#include "blood_spurt_fixture.h"
#include "engine/capture.h"
#include "engine/runtime.h"
#include "engine/world_renderer.h"
#include "game/original/ClientSplatFX.h"
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
    std::cout << "GPU device: "
              << SDL_GetStringProperty(SDL_GetGPUDeviceProperties(runtime.gpu), SDL_PROP_GPU_DEVICE_NAME_STRING,
                                       "unreported")
              << " | backend " << SDL_GetGPUDeviceDriver(runtime.gpu) << "\n";
    WorldRenderer renderer(runtime.gpu, DrawStyle::SpriteAlpha);
    Capture capture;
    SpriteView scene;
    GameSession g;
    if (argc == 3)
    {
        Config config;
        config.data_dir = argv[2];
        CHECK(g.initialize() && g.resources.mount_game(config, error));
        std::error_code ec;
        std::filesystem::create_directories(argv[1], ec);
        CHECK(!ec);
    }
    else
        CHECK(test_player::setup(g, argv[1], false) && test_blood_spurt::mount(g.resources, argv[1], error));
    std::uint64_t last = 0;
    for (int cycle = 0; cycle < 3; ++cycle)
    {
        if (argc == 3)
            CHECK(g.request_world("01_SubwayA", error) && g.objects.clear());
        CHECK(test_blood_spurt::floor(g, error));
        g.client_shell.view.position = {0, 10, -5};
        g.client_shell.view.rotation = {};
        for (unsigned i = 1; i <= 3; ++i)
        {
            BloodSpurtSettings s;
            s.position = {float(i) - 2, 10, 0};
            s.texture = i;
            s.min_velocity = s.max_velocity = {0, 10, 0};
            CHECK(g.server.PublishClientBloodSpurt(s, "GPU drop variant", true, error) && error.empty());
        }
        CHECK(g.server.blood_spurts.active.front().id > last);
        last = g.server.blood_spurts.active.back().id;
        CHECK(update_sprite_scene(g, scene, error) && scene.entries.size() == 3); // One entry per spurt.
        for (const auto& e : scene.entries)
            CHECK(e.blood_spurt && e.error.empty() && e.particles == 12);
        const auto callbacks = g.server.blood_spurts.active.front().updates;
        CHECK(update_sprite_scene(g, scene, error) && g.server.blood_spurts.active.front().updates == callbacks);
        std::vector<std::uint8_t> pixels;
        CHECK(render(runtime, renderer, scene, g.client_shell.view.position, {}, capture, pixels, error));
        unsigned ink = 0;
        for (unsigned k = 0; k < pixels.size(); k += 4)
            if (pixels[k] != 6 || pixels[k + 1] != 9 || pixels[k + 2] != 13)
                ++ink;
        CHECK(ink > 100 && ink < 400 * 300);
        CHECK(capture.save((std::filesystem::path(argv[1]) / "blood-drops.png").c_str()));
        std::cout << "cycle " << cycle << " three drop textures: " << ink << " changed pixels\n";
        g.server.blood_spurts.clear();
        BloodSpurtSettings hit;
        hit.position = {0, 1, 0};
        hit.min_velocity = hit.max_velocity = {0, -20, 0};
        CHECK(g.server.PublishClientBloodSpurt(hit, "GPU emitter collision", false, error) && error.empty());
        CHECK(g.objects.advance(.1) && g.server.blood_spurts.advance(g.objects.time(), g.server));
        CHECK(g.server.blood_spurts.active.front().impacts == 1 && g.server.blood_splats.active.size() == 1);
        CHECK(g.server.blood_splats.active.front().settings.scale > 0);
        g.server.blood_spurts.clear(); // Remaining mark must have independent GPU lifetime.
        g.client_shell.view.position = {0, argc == 3 ? 25.f : 3.f, 0};
        CHECK(align_rotation({0, -1, 0}, g.client_shell.view.rotation));
        CHECK(update_sprite_scene(g, scene, error) && scene.entries.size() == 1 && scene.entries[0].blood_splat &&
              scene.entries[0].error.empty());
        CHECK(render(runtime, renderer, scene, g.client_shell.view.position, g.client_shell.view.rotation, capture,
                     pixels, error));
        ink = 0;
        for (unsigned k = 0; k < pixels.size(); k += 4)
            if (pixels[k] != 6 || pixels[k + 1] != 9 || pixels[k + 2] != 13)
                ++ink;
        CHECK(ink > 5 && ink < 400 * 300 / 2);
        CHECK(capture.save((std::filesystem::path(argv[1]) / "blood-impact.png").c_str()));
        std::cout << "cycle " << cycle << " original emitter collision mark: " << ink << " changed pixels\n";
        CHECK(g.unload_world() && g.server.blood_spurts.active.empty() && g.server.blood_splats.active.empty());
        CHECK(update_sprite_scene(g, scene, error) && scene.entries.empty());
        renderer.clear_geometry();
        scene.clear();
    }
    return 0;
}
