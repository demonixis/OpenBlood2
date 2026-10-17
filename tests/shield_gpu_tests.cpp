#include "app/sprite_scene.h"
#include "engine/capture.h"
#include "engine/runtime.h"
#include "engine/world_renderer.h"
#include "shield_fixture.h"
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
    GameSession g;
    CHECK(g.initialize());
    std::filesystem::create_directories(argv[1]);
    if (argc == 3)
    {
        Config config;
        config.data_dir = argv[2];
        CHECK(g.resources.mount_game(config, error));
    }
    else
        CHECK(test_shield::mount(g.resources, argv[1], error));
    ObjectCreateStruct state;
    state.m_Pos = {0, 0, 100};
    auto h = g.objects.create(g.objects.find_class("BaseClass"), state);
    CHECK(h);
    CHECK(g.server.PublishObjectParticles(h, 19, error));
    CHECK(g.server.PublishObjectParticles(h, 36, error));
    auto& manager = g.server.object_particles;
    CHECK(manager.advance(0, g.server) && manager.advance(.11, g.server));
    SpriteView scene;
    Capture capture;
    std::vector<std::uint8_t> pixels, again;
    CHECK(update_sprite_scene(g, scene, error) && scene.entries.size() == 42);
    for (const auto& entry : scene.entries)
        CHECK(entry.error.empty() && entry.object_effect);
    const auto updates = manager.active[0].effect.updates;
    CHECK(render(runtime, renderer, scene, {}, {}, capture, pixels, error));
    CHECK(update_sprite_scene(g, scene, error) && render(runtime, renderer, scene, {}, {}, capture, again, error));
    CHECK(pixels == again && manager.active[0].effect.updates == updates);
    unsigned ink = 0;
    for (unsigned i = 0; i < 400 * 300; ++i)
        if (pixels[i * 4] != 6 || pixels[i * 4 + 1] != 9 || pixels[i * 4 + 2] != 13)
            ++ink;
    CHECK(ink > 25 && ink < 100000);
    CHECK(capture.save((std::filesystem::path(argv[1]) / "shield-powerup.png").c_str()));
    std::cout << (argc == 3 ? "Original external" : "Synthetic") << " shield/powerup: 42 textured quads, " << ink
              << " changed pixels; repeated readback identical, clocks unchanged.\n";
    CHECK(manager.advance(2.375, g.server) && manager.active.size() == 2);
    CHECK(manager.active[1].effect.alpha == .25f);
    CHECK(update_sprite_scene(g, scene, error) && render(runtime, renderer, scene, {}, {}, capture, pixels, error));
    CHECK(g.objects.remove(h) && g.objects.finish_frame() && manager.advance(2.4, g.server));
    CHECK(manager.active.empty() && update_sprite_scene(g, scene, error) && scene.draws.empty());
    CHECK(g.unload_world());
    scene.clear();
    renderer.clear_geometry();
    return 0;
}
