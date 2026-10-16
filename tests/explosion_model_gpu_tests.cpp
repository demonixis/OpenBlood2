#include "app/model_scene.h"
#include "engine/capture.h"
#include "engine/runtime.h"
#include "engine/world_renderer.h"
#include "explosion_model_fixture.h"
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
    DVector sub(DVector a, DVector b)
    {
        return {a.x - b.x, a.y - b.y, a.z - b.z};
    }
    DVector cross(DVector a, DVector b)
    {
        return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
    }
    bool render(Runtime& runtime, WorldRenderer& renderer, ModelView& scene, DVector eye, DRotation camera,
                Capture& capture, std::vector<std::uint8_t>& pixels, std::string& error)
    {
        if ((!capture.texture && !capture.initialize(runtime.gpu, SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM, 400, 300)) ||
            !renderer.prepare(scene.mesh, scene.revision, SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM, 400, 300, error, true))
            return false;
        auto* commands = SDL_AcquireGPUCommandBuffer(runtime.gpu);
        if (!commands)
            return false;
        if (!renderer.draw(commands, capture.texture, scene.mesh, scene.world_draws, eye, camera, false, error))
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
    Resources resources;
    ModelStore models(resources);
    ModelView scene;
    CHECK(test_explosion_model::mount(resources, argv[1], error));
    ExplosionModelSettings settings;
    settings.scale1 = settings.scale2 = {1, 1, 1};
    settings.duration = 1;
    settings.fade = 1;
    CExplosionModelFX fx;
    std::minstd_rand random(1);
    CHECK(fx.Init(settings, error) && fx.CreateObject(models, resources, 0, random, error));
    std::vector<std::uint8_t> full, half;
    Capture capture;
    CHECK(scene.add(fx.model.get(), fx.animation, fx.skin_path, {}, fx.scale, true, false, {}, "alpha model", resources,
                    error, {}, fx.alpha, true));
    CHECK(scene.world_draws.size() == 1 && scene.entries[0].error.empty());
    const auto a = scene.mesh.vertices[0].position, b = scene.mesh.vertices[1].position,
               c = scene.mesh.vertices[2].position;
    auto normal = cross(sub(b, a), sub(c, a));
    const float length = std::sqrt(normal.x * normal.x + normal.y * normal.y + normal.z * normal.z);
    CHECK(length > 0);
    normal = {normal.x / length, normal.y / length, normal.z / length};
    DRotation camera;
    CHECK(align_rotation(normal, camera));
    DVector center{(a.x + b.x + c.x) / 3, (a.y + b.y + c.y) / 3, (a.z + b.z + c.z) / 3};
    const DVector eye{center.x - normal.x * 10, center.y - normal.y * 10, center.z - normal.z * 10};
    CHECK(render(runtime, renderer, scene, eye, camera, capture, full, error));
    // Same pose/material, half alpha from the original Update formula.
    CHECK(fx.Update(.5) && std::abs(fx.alpha - .5f) < .0001f);
    scene.begin_frame();
    AnimationTrack original_pose;
    CHECK(scene.add(fx.model.get(), original_pose, fx.skin_path, {}, fx.scale, true, false, {}, "half alpha model",
                    resources, error, {}, fx.alpha, true));
    CHECK(render(runtime, renderer, scene, eye, camera, capture, half, error));
    unsigned ink = 0;
    const int background[] = {6, 9, 13};
    for (unsigned i = 0; i < 400 * 300; ++i)
    {
        bool changed = false;
        for (unsigned channel = 0; channel < 3; ++channel)
        {
            const auto index = i * 4 + channel;
            changed |= full[index] != background[channel];
            CHECK(std::abs(int(half[index]) - int((full[index] + background[channel]) / 2)) <= 1);
        }
        if (changed)
            ++ink;
    }
    CHECK(ink > 50 && ink < 40000);
    CHECK(capture.save((std::filesystem::path(argv[1]) / "model-alpha.png").c_str()));
    std::cout << "Original model opacity: " << ink << " covered pixels, half-opacity linear blend verified\n";
    if (argc == 3)
    {
        GameSession game;
        Config config;
        config.data_dir = argv[2];
        CHECK(game.initialize() && game.resources.mount_game(config, error));
        // Isolated original default model recipe, without copying any commercial resource.
        ExplosionModels effects;
        CHECK(effects.publish(3, {}, {0, 1, 0}, "GPU model", game.models, game.resources, 0, error) && error.empty());
        CHECK(effects.advance(.25) && effects.active.size() == 1);
        const auto& model = effects.active.front();
        scene.clear();
        renderer.clear_geometry();
        CHECK(scene.add(model.model.get(), model.animation, model.skin_path, {model.settings.position, model.rotation},
                        model.scale, true, false, {}, model.model_path, game.resources, error, {}, model.alpha, true));
        CHECK(scene.world_draws.size() == 1 && scene.entries[0].error.empty() && scene.entries[0].triangles > 0);
        CHECK(render(runtime, renderer, scene, {0, 10, -800}, {}, capture, full, error));
        ink = 0;
        for (unsigned i = 0; i < 400 * 300; ++i)
            if (full[i * 4] != 6 || full[i * 4 + 1] != 9 || full[i * 4 + 2] != 13)
                ++ink;
        CHECK(ink > 50 && ink < 100000);
        CHECK(capture.save((std::filesystem::path(argv[1]) / "original-explosion-model.png").c_str()));
        std::cout << "Original exp_sphere/Explosion_1: " << scene.entries[0].triangles << " triangles, " << ink
                  << " pixels, alpha " << model.alpha << "\n";
        CHECK(effects.advance(.5) && effects.active.empty());
        effects.clear();
        scene.clear();
        renderer.clear_geometry();
        CHECK(scene.world_draws.empty());
    }
    return 0;
}
