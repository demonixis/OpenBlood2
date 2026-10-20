#include "app/sprite_scene.h"
#include "blood_spurt_fixture.h"
#include "engine/capture.h"
#include "engine/runtime.h"
#include "engine/world_renderer.h"
#include "game/original/ClientWarpGateSFX.h"

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
    g.client_shell.view.position = {0, 0, -150};
    auto owner = g.objects.create(g.objects.find_class("BaseClass"), {});
    CHECK(owner);
    WarpGateSettings settings;
    settings.sprite.resource = "Sprites\\BlaLoop1.spr";
    settings.sprite.max_scale = .25f;
    settings.sprite.alpha = 1;
    settings.ramp_up = settings.ramp_down = 1;
    settings.particles[0].resource = "Sprites\\flaregunflare.spr";
    settings.particles[0].position_radius = 32;
    settings.particles[0].min_life = 2;
    settings.particles[0].gravity = -15;
    settings.particles[1].count = 0;
    settings.particles[1].resource = "SpriteTextures\\";
    const auto event = g.server.warp_gates.publish(owner, settings, g.server, g.sprites, g.resources, 0, error);
    CHECK(event && error.empty());
    g.server.SetObjectUserFlags(owner, 1);
    for (unsigned i = 0; i <= 90; ++i)
        CHECK(g.server.warp_gates.advance(i / 60., g.server));
    CHECK(g.server.warp_gates.particle_count() >= 40 && update_sprite_scene(g, scene, error));
    for (const auto& e : scene.entries)
        CHECK(e.warp_gate == event && e.error.empty());
    std::vector<std::uint8_t> pixels;
    CHECK(render(runtime, renderer, scene, g.client_shell.view.position, {}, capture, pixels, error));
    unsigned lit = 0;
    for (unsigned i = 0; i < pixels.size(); i += 4)
        if (pixels[i] > 40 || pixels[i + 1] > 40 || pixels[i + 2] > 40)
            ++lit;
    CHECK(capture.save((std::filesystem::path(argv[1]) / "warp-gate.png").c_str()));
    std::cout << "warp lit=" << lit << " entries=" << scene.entries.size() << '\n';
    CHECK(lit > 100);
    // Read back the actual SPR-textured particles without the central sprite.
    g.server.warp_gates.active[0].sprite_ready = false;
    CHECK(update_sprite_scene(g, scene, error) &&
          render(runtime, renderer, scene, g.client_shell.view.position, {}, capture, pixels, error));
    unsigned particle_pixels = 0;
    for (unsigned i = 0; i < pixels.size(); i += 4)
        if (pixels[i] > 40 || pixels[i + 1] > 40 || pixels[i + 2] > 40)
            ++particle_pixels;
    CHECK(capture.save((std::filesystem::path(argv[1]) / "warp-particles.png").c_str()));
    std::cout << "animated particle pixels=" << particle_pixels << '\n';
    CHECK(particle_pixels > 100);
    CHECK(g.objects.remove(owner) && g.objects.finish_frame() && g.server.warp_gates.advance(2, g.server));
    CHECK(update_sprite_scene(g, scene, error) && scene.entries.empty());
    CHECK(render(runtime, renderer, scene, g.client_shell.view.position, {}, capture, pixels, error));
    for (unsigned i = 0; i < pixels.size(); i += 4)
        CHECK(pixels[i] == 6 && pixels[i + 1] == 9 && pixels[i + 2] == 13);
    CHECK(g.unload_world());
    return 0;
}
