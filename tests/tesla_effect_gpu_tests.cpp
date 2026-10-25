#include "app/model_scene.h"
#include "app/sprite_scene.h"
#include "engine/capture.h"
#include "engine/runtime.h"
#include "engine/world_renderer.h"

#include <cmath>
#include <iostream>
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
    auto owner = g.objects.create(g.objects.find_class("BaseClass"), {});
    CHECK(owner);
    g.client_shell.view.position = {0, 0, -400};
    for (unsigned component = 0; component < 5; ++component)
    {
        g.server.explosion_models.clear();
        g.server.explosion_sprites.clear();
        g.server.explosion_rings.clear();
        g.server.lightning.clear();
        if (component == 0)
            CHECK(g.server.PublishExplosionModels(owner, 7, {}, {0, 0, -1}, error) && error.empty());
        else if (component == 1)
            CHECK(g.server.PublishExplosionSprites(owner, 7, {}, {0, 0, -1}, error) && error.empty());
        else if (component == 2)
            CHECK(g.server.PublishExplosionRings(owner, 7, {}, {0, 0, -1}, error) && error.empty());
        else
            CHECK(g.server.PublishLightning(owner, {{-150, 0, 0}, {150, 0, 0}, 2, DBYTE(component == 3 ? 3 : 1), 2},
                                            error) &&
                  error.empty());
        CHECK(g.server.explosion_models.advance(.2) && g.server.explosion_sprites.advance(.2) &&
              g.server.explosion_rings.advance(.2));
        CHECK(g.server.explosion_rings.advance(.3));
        CHECK(update_explosion_model_scene(g, models, error) && update_sprite_scene(g, scene, error));
        CHECK(component == 0 ? models.world_draws.size() == 1 : !scene.draws.empty());
        for (const auto& entry : scene.entries)
            CHECK(entry.error.empty());
        for (const auto& material : models.mesh.materials)
            CHECK(material.error.empty());
        CHECK(render(runtime, renderer, model_renderer, models, scene, g.client_shell.view.position, {}, capture,
                     pixels, error));
        unsigned blue = 0;
        for (unsigned i = 0; i < pixels.size(); i += 4)
            blue += pixels[i + 2] > 40 && pixels[i + 2] >= pixels[i];
        CHECK(blue > 10 && blue < 400 * 300 / 2);
        CHECK(capture.save((std::filesystem::path(argv[1]) / ("tesla-" + std::to_string(component) + ".png")).c_str()));
        std::cout << "Tesla component " << component << ": " << blue << " blue pixels\n";
    }
    CHECK(g.objects.remove(owner) && g.objects.finish_frame());
    CHECK(g.server.lightning.advance(.2) && !g.server.lightning.active.empty());
    CHECK(g.server.lightning.advance(.5) && g.server.lightning.active.empty());
    CHECK(g.unload_world());
    return 0;
}
