#include "app/model_scene.h"
#include "app/sprite_scene.h"
#include "blood_spurt_fixture.h"
#include "brush_fixture.h"
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
    bool render(Runtime& runtime, WorldRenderer& renderer, WorldRenderer& model_renderer, ModelView& models,
                SpriteView& scene, DVector eye, DRotation camera, Capture& capture, std::vector<std::uint8_t>& pixels,
                std::string& error)
    {
        if ((!capture.texture && !capture.initialize(runtime.gpu, SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM, 400, 300)) ||
            !model_renderer.prepare(models.mesh, models.revision, SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM, 400, 300, error,
                                    true) ||
            !renderer.prepare(scene.mesh, scene.revision, SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM, 400, 300, error, true))
            return false;
        auto* commands = SDL_AcquireGPUCommandBuffer(runtime.gpu);
        if (!commands)
            return false;
        if (!model_renderer.draw(commands, capture.texture, models.mesh, models.world_draws, eye, camera, false,
                                 error) ||
            !renderer.draw(commands, capture.texture, scene.mesh, scene.draws, eye, camera, false, error,
                           DrawLoad::Load, model_renderer.depth_target()))
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
    WorldRenderer renderer(runtime.gpu, DrawStyle::SpriteAlpha), model_renderer(runtime.gpu, DrawStyle::SpriteAlpha);
    ModelView models;
    SpriteView scene;
    Capture capture;
    std::vector<std::uint8_t> pixels;
    GameSession g;
    Config config;
    config.data_dir = argv[2];
    CHECK(g.initialize() && g.resources.mount_game(config, error));
    std::error_code ec;
    std::filesystem::create_directories(argv[1], ec);
    CHECK(!ec);
    ObjectCreateStruct state;
    state.m_Pos = {0, 100, 0};
    auto owner = g.objects.create(g.objects.find_class("BaseClass"), state);
    CHECK(owner);
    g.client_shell.view.position = {0, 75, -60};
    CHECK(g.server.PublishObjectParticles(owner, 18, error));
    CHECK(g.objects.advance(.01) && g.server.object_particles.advance(g.objects.time(), g.server));
    CHECK(g.objects.advance(.1) && g.server.object_particles.advance(g.objects.time(), g.server));
    CHECK(update_sprite_scene(g, scene, error));
    CHECK(render(runtime, renderer, model_renderer, models, scene, g.client_shell.view.position, {}, capture, pixels,
                 error));
    unsigned blue = 0;
    for (unsigned i = 0; i < pixels.size(); i += 4)
        if (pixels[i + 2] > 40 && pixels[i + 2] > pixels[i] + 10)
            ++blue;
    std::cout << "Heal blue pixels=" << blue << " particles=" << g.server.object_particles.particle_count() << "\n";
    CHECK(capture.save((std::filesystem::path(argv[1]) / "ishmael-heal.png").c_str()));
    CHECK(blue > 100);
    g.server.object_particles.clear();
    SceneExplosionReport effect;
    CHECK(PublishDivineShockball(owner, {0, 75, 0}, {0, 1, 0}, effect));
    CHECK(g.objects.advance(.2) && g.server.explosion_models.advance(g.objects.time()) &&
          g.server.explosion_rings.advance(g.objects.time()) && g.server.explosion_lights.advance(g.objects.time()));
    g.client_shell.view.position = {0, 75, -2000};
    CHECK(update_explosion_model_scene(g, models, error) && update_sprite_scene(g, scene, error));
    CHECK(render(runtime, renderer, model_renderer, models, scene, g.client_shell.view.position, {}, capture, pixels,
                 error));
    unsigned red = 0;
    for (unsigned i = 0; i < pixels.size(); i += 4)
        if (pixels[i] > 40 && pixels[i] > pixels[i + 2] + 10)
            ++red;
    std::cout << "Divine shockball red pixels=" << red << " models=" << models.entries.size() << "\n";
    CHECK(capture.save((std::filesystem::path(argv[1]) / "ishmael-shockball.png").c_str()));
    CHECK(red > 100 && red < 400 * 300 / 2);
    CHECK(g.server.explosion_models.advance(2) && g.server.explosion_rings.advance(2) &&
          g.server.explosion_lights.advance(2));
    CHECK(update_explosion_model_scene(g, models, error) && update_sprite_scene(g, scene, error));
    CHECK(render(runtime, renderer, model_renderer, models, scene, g.client_shell.view.position, {}, capture, pixels,
                 error));
    for (unsigned i = 0; i < pixels.size(); i += 4)
        CHECK(pixels[i] == 6 && pixels[i + 1] == 9 && pixels[i + 2] == 13);
    CHECK(g.unload_world());
    return 0;
}
