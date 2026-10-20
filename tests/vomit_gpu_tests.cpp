#include "app/sprite_scene.h"
#include "blood_spurt_fixture.h"
#include "engine/capture.h"
#include "engine/runtime.h"
#include "engine/world_renderer.h"
#include "game/original/CampaignProjectiles.h"

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
    g.client_shell.view.position = {50, 0, -12}; // Resolve the small original trail near its current emitter.
    auto owner = g.objects.create(g.objects.find_class("BaseClass"), {});
    CHECK(owner);
    ObjectCreateStruct state;
    state.m_Pos = {-50, 0, 0};
    auto h = g.objects.create(g.objects.find_class("CVomitProjectile"), state);
    auto* prime = dynamic_cast<CVomitProjectile*>(g.server.HandleToObject(h));
    CHECK(prime && prime->Setup({1, 0, 0}, 43, 25, 2000, 0, owner));
    CHECK(g.objects.advance(.02));
    CHECK(g.server.object_particles.advance(g.objects.time(), g.server));
    DVector to{50, 0, 0};
    CHECK(g.server.TeleportObject(h, &to) == DE_OK && g.objects.advance(.03));
    CHECK(g.server.object_particles.advance(g.objects.time(), g.server));
    CHECK(g.server.object_particles.particle_count() > 10);
    CHECK(update_sprite_scene(g, scene, error));
    for (const auto& entry : scene.entries)
        CHECK(entry.error.empty());
    std::vector<std::uint8_t> pixels;
    CHECK(render(runtime, renderer, scene, g.client_shell.view.position, {}, capture, pixels, error));
    unsigned lit = 0;
    for (unsigned i = 0; i < pixels.size(); i += 4)
        if (pixels[i] > 40 || pixels[i + 1] > 40 || pixels[i + 2] > 40)
            ++lit;
    std::cout << "flight lit=" << lit << " entries=" << scene.entries.size() << "\n";
    CHECK(capture.save((std::filesystem::path(argv[1]) / "vomit-flight.png").c_str()));
    CHECK(lit > 100);
    prime->m_bExplode = true;
    CHECK(g.objects.advance(.002) && !g.server.IsObjectAlive(h));
    CHECK(g.server.object_particles.advance(g.objects.time(), g.server));
    CHECK(update_sprite_scene(g, scene, error));
    CHECK(render(runtime, renderer, scene, g.client_shell.view.position, {}, capture, pixels, error));
    CHECK(capture.save((std::filesystem::path(argv[1]) / "vomit-expired.png").c_str()));
    CHECK(g.server.explosion_sprites.active.empty() && g.server.explosion_models.active.empty());
    for (const auto& entry : scene.entries)
        CHECK(entry.error.empty());
    CHECK(g.server.explosion_sprites.advance(4) && g.server.explosion_lights.advance(4));
    CHECK(update_sprite_scene(g, scene, error) && scene.entries.empty());
    CHECK(render(runtime, renderer, scene, g.client_shell.view.position, {}, capture, pixels, error));
    for (unsigned i = 0; i < pixels.size(); i += 4)
        CHECK(pixels[i] == 6 && pixels[i + 1] == 9 && pixels[i + 2] == 13);
    CHECK(g.unload_world());
    std::cout << lit << " Vomit/trail pixels, source no-impact FX and expiry verified\n";
    return 0;
}
