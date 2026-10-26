#include "app/sprite_scene.h"
#include "blood_splat_fixture.h"
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
    renderer.point_sprites = true; // Exact texel colours of the 4x2 fixture.
    Capture capture;
    SpriteView scene;
    Resources resources;
    CHECK(test_blood_splat::mount(resources, argv[1], error));
    SpriteStore store(resources);
    BloodSplatSettings settings;
    settings.position = {0, 0, 2};
    settings.scale = .5f;
    settings.grow_scale = 0;
    settings.resource = "sprites/blood1.spr";
    CBloodSplatFX effect;
    std::minstd_rand random(1);
    CHECK(effect.Init(settings, error) && effect.CreateObject(store, 0, random, error));
    CHECK(!effect.Term() && effect.Update(2, random) && effect.Update(3, random));
    std::vector<std::uint8_t> white, tinted;
    for (bool tint : {false, true})
    {
        CHECK(scene.begin_frame({}, {}, error) &&
              scene.add(effect.sprite.get(), effect.track, {effect.position, effect.rotation}, effect.scale,
                        FLAG_VISIBLE | FLAG_ROTATEABLESPRITE, {}, resources, error, tint ? effect.alpha : 1,
                        tint ? DVector{.1f, .1f, .1f} : DVector{1, 1, 1}));
        CHECK(scene.entries[0].error.empty() &&
              render(runtime, renderer, scene, {}, {}, capture, tint ? tinted : white, error));
    }
    unsigned verified = 0;
    for (unsigned k = 0; k < white.size(); k += 4)
        if (white[k] == 255 && white[k + 1] == 0 && white[k + 2] == 0)
        {
            CHECK(std::abs(int(tinted[k]) - (25.5f * effect.alpha + 6 * (1 - effect.alpha))) <= 2);
            CHECK(std::abs(int(tinted[k + 1]) - 9 * (1 - effect.alpha)) <= 2 &&
                  std::abs(int(tinted[k + 2]) - 13 * (1 - effect.alpha)) <= 2);
            ++verified;
        }
    CHECK(verified > 100 && capture.save((std::filesystem::path(argv[1]) / "splat-tint-alpha.png").c_str()));
    std::cout << "Original blood splat tint/Term alpha: " << verified << " exact opaque-texel blends\n";
    if (argc == 3)
    {
        GameSession g;
        Config config;
        config.data_dir = argv[2];
        CHECK(g.initialize() && g.resources.mount_game(config, error));
        std::uint64_t last = 0;
        for (int cycle = 0; cycle < 3; ++cycle)
        {
            CHECK(g.request_world("01_SubwayA", error));
            auto h = g.objects.create(g.objects.find_class("CClientSplatFX"), {});
            auto* publisher = dynamic_cast<CClientSplatFX*>(g.server.HandleToObject(h));
            CHECK(publisher && publisher->Setup({0, 5000, 0}, {0, 1, 0}, .1f, .02f));
            CHECK(g.objects.advance(.001) && !g.server.IsObjectAlive(h) && g.server.blood_splats.active.size() == 1);
            CHECK(g.server.blood_splats.active.front().id > last);
            last = g.server.blood_splats.active.front().id;
            for (int variant = 1; variant <= 3; ++variant)
            {
                auto& pool = g.server.blood_splats.active.front();
                auto s = pool.settings;
                s.resource = "sprites/blood" + std::to_string(variant) + ".spr";
                CBloodSplatFX f;
                CHECK(f.Init(s, error) && f.CreateObject(g.sprites, 0, random, error) && f.Update(.25, random));
                // Top-down view of original normal-aligned pool; no billboard substitution.
                const auto eye = vector_add(f.position, {0, 35, 0});
                DRotation camera;
                CHECK(align_rotation({0, -1, 0}, camera));
                CHECK(scene.begin_frame(eye, camera, error) &&
                      scene.add(f.sprite.get(), f.track, {f.position, f.rotation}, f.scale,
                                FLAG_VISIBLE | FLAG_ROTATEABLESPRITE, {}, g.resources, error, f.alpha,
                                {.1f, .1f, .1f}));
                CHECK(scene.entries[0].error.empty() && scene.draws.size() == 1);
                std::vector<std::uint8_t> pixels;
                CHECK(render(runtime, renderer, scene, eye, camera, capture, pixels, error));
                unsigned ink = 0;
                for (unsigned k = 0; k < pixels.size(); k += 4)
                    if (pixels[k] != 6 || pixels[k + 1] != 9 || pixels[k + 2] != 13)
                        ++ink;
                CHECK(ink > 50 && ink < 400 * 300);
                CHECK(capture.save(
                    (std::filesystem::path(argv[1]) / ("private-blood" + std::to_string(variant) + ".png")).c_str()));
                std::cout << "Private cycle " << cycle << " variant " << variant << " " << scene.entries[0].frame_path
                          << " changed pixels " << ink << "\n";
            }
            CHECK(g.unload_world() && g.server.blood_splats.active.empty());
            scene.clear();
            renderer.clear_geometry();
            CHECK(update_sprite_scene(g, scene, error) && scene.entries.empty());
        }
    }
    return 0;
}
