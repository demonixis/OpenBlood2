#include "app/sprite_scene.h"
#include "blood_spurt_fixture.h"
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
    g.client_shell.view.position = {0, 0, -500};
    auto owner = g.objects.create(g.objects.find_class("BaseClass"), {});
    CHECK(owner);
    LightningSettings bolt{{-150, 0, 0}, {150, 0, 0}, 2, 1, 2};
    CHECK(g.server.PublishLightning(owner, bolt, error) && error.empty());
    CHECK(update_sprite_scene(g, scene, error));
    const auto count = g.server.lightning.live_particles();
    CHECK(count > 10 && scene.entries.size() == count);
    for (const auto& entry : scene.entries)
        CHECK(entry.lightning_bolt && entry.error.empty());
    std::vector<std::uint8_t> pixels;
    CHECK(render(runtime, renderer, scene, g.client_shell.view.position, {}, capture, pixels, error));
    unsigned lit = 0;
    for (unsigned i = 0; i < pixels.size(); i += 4)
        if (pixels[i + 2] > 40 && pixels[i + 2] >= pixels[i])
            ++lit;
    CHECK(lit > 100 && lit < 400 * 300 / 2);
    CHECK(capture.save((std::filesystem::path(argv[1]) / "lightning.png").c_str()));
    CHECK(g.objects.remove(owner) && g.objects.finish_frame());
    CHECK(g.server.lightning.advance(.2));
    CHECK(update_sprite_scene(g, scene, error));
    CHECK(render(runtime, renderer, scene, g.client_shell.view.position, {}, capture, pixels, error));
    CHECK(capture.save((std::filesystem::path(argv[1]) / "lightning-fading.png").c_str()));
    CHECK(g.server.lightning.advance(.25) && update_sprite_scene(g, scene, error) && scene.entries.empty());
    CHECK(g.server.lightning.active.front().lights.size() == 3);
    CHECK(render(runtime, renderer, scene, g.client_shell.view.position, {}, capture, pixels, error));
    for (unsigned i = 0; i < pixels.size(); i += 4)
        CHECK(pixels[i] == 6 && pixels[i + 1] == 9 && pixels[i + 2] == 13);
    CHECK(g.server.lightning.advance(.5) && g.server.lightning.active.empty());
    CHECK(g.unload_world() && g.server.lightning.reports.empty());
    std::cout << count << " actual lightning particles, " << lit << " lit pixels, detached fade/expiry verified\n";
    return 0;
}
