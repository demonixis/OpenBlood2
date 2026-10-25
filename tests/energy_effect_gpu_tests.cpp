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
    g.client_shell.view.position = {0, 0, -700};
    for (unsigned type : {12u, 18u, 19u, 23u, 24u, 25u, 28u, 29u, 30u, 31u})
    {
        g.client_shell.view.position.z = type == 24                               ? -2000.f
                                         : type == 12 || type == 25 || type == 28 ? -200.f
                                                                                  : -700.f;
        g.server.explosion_flames.clear();
        g.server.explosion_models.clear();
        g.server.explosion_sprites.clear();
        g.server.explosion_rings.clear();
        g.server.explosion_fragments.clear();
        CHECK(g.server.PublishExplosionModels(owner, type, {}, {0, 0, -1}, error) && error.empty());
        CHECK(g.server.PublishExplosionSprites(owner, type, {}, {0, 0, -1}, error) && error.empty());
        CHECK(g.server.PublishExplosionRings(owner, type, {}, {0, 0, -1}, error) && error.empty());
        CHECK(g.server.PublishExplosionFragments(owner, type, {}, {0, 0, -1}, error) && error.empty());
        CHECK(g.server.PublishExplosionFlames(owner, type, {}, {0, 0, -1}, error) && error.empty());
        CHECK(g.server.explosion_flames.advance(type == 12 ? .8 : .3));
        CHECK(g.server.explosion_models.advance(type == 24 ? 2.0 : .2) && g.server.explosion_sprites.advance(.2) &&
              g.server.explosion_rings.advance(.2));
        CHECK(g.server.explosion_rings.advance(.31) && g.server.explosion_rings.advance(type == 24 ? 1.0 : .4));
        CHECK(update_explosion_model_scene(g, models, error) && update_sprite_scene(g, scene, error));
        CHECK(!models.world_draws.empty() || !scene.draws.empty());
        for (const auto& entry : scene.entries)
            CHECK(entry.error.empty());
        for (const auto& material : models.mesh.materials)
            CHECK(material.error.empty());
        CHECK(render(runtime, renderer, model_renderer, models, scene, g.client_shell.view.position, {}, capture,
                     pixels, error));
        unsigned ink = 0;
        for (unsigned i = 0; i < pixels.size(); i += 4)
            ink += pixels[i] > 40 || pixels[i + 1] > 40 || pixels[i + 2] > 40;
        std::cerr << "Recipe " << type << " visible pixels " << ink << "\n";
        CHECK(ink > 50 && ink < 400 * 300);
        CHECK(capture.save((std::filesystem::path(argv[1]) / ("explosion-" + std::to_string(type) + ".png")).c_str()));
        std::cout << "Explosion " << type << ": " << models.world_draws.size() << " model draws, " << scene.draws.size()
                  << " sprite/particle draws, " << ink << " lit pixels\n";
    }
    g.server.explosion_flames.clear();
    g.server.explosion_models.clear();
    g.server.explosion_sprites.clear();
    g.server.explosion_rings.clear();
    g.server.explosion_fragments.clear();
    g.client_shell.view.position = {0, 0, -100};
    for (unsigned recipe : {4u, 9u, 27u, 31u, 37u, 38u, 39u})
    {
        g.server.object_particles.clear();
        CObjectParticleFX fx;
        std::minstd_rand rng(1);
        NodePose pose{{-50, 0, 0}, {}};
        CHECK(fx.Init(recipe, 1, rng, error) && fx.CreateObject(g.resources, pose, 10, error) &&
              fx.Update(10, &pose, rng));
        pose.position.x = 50;
        CHECK(fx.Update(10.101, &pose, rng) && !fx.particles.empty());
        g.server.object_particles.active.push_back({owner, std::move(fx)});
        CHECK(update_explosion_model_scene(g, models, error) && update_sprite_scene(g, scene, error));
        for (const auto& entry : scene.entries)
            CHECK(entry.error.empty());
        CHECK(render(runtime, renderer, model_renderer, models, scene, g.client_shell.view.position, {}, capture,
                     pixels, error));
        unsigned ink = 0;
        for (unsigned i = 0; i < pixels.size(); i += 4)
            ink += pixels[i] > 20 || pixels[i + 1] > 20 || pixels[i + 2] > 20;
        std::cerr << "Particle recipe " << recipe << " visible pixels " << ink << "\n";
        CHECK(ink > 10 && ink < 400 * 300);
        CHECK(capture.save((std::filesystem::path(argv[1]) / ("smoke-" + std::to_string(recipe) + ".png")).c_str()));
    }
    CHECK(g.unload_world());
    return 0;
}
