#include "app/sprite_scene.h"
#include "engine/capture.h"
#include "engine/runtime.h"
#include "engine/world_renderer.h"
#include "explosion_ring_fixture.h"
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
    renderer.point_sprites = true; // Exact texel colours of the 4x2 fixture.
    Resources resources;
    SpriteView scene;
    CHECK(test_explosion_ring::mount(resources, argv[1], error));
    ExplosionRingSettings settings;
    settings.position = {0, 0, 10};
    settings.normal = {0, 0, 1};
    settings.align = 1;
    settings.position_radius = 3;
    settings.radius = 640;
    settings.count = 4;
    settings.velocity = 0;
    settings.duration = 1;
    settings.fade = 1;
    settings.color = {.5f, 1, .25f};
    settings.texture = "alpha.dtx";
    CExplosionRingFX fx;
    CHECK(fx.Init(settings, error) && fx.CreateObject(resources, 0, error));
    auto add = [&](const CExplosionRingFX& f, const Resources& r)
    {
        for (unsigned i = 0; i < f.particles.size(); ++i)
        {
            auto pos = rotate_vector(f.rotation, f.particles[i].position);
            pos = {pos.x + f.settings.position.x, pos.y + f.settings.position.y, pos.z + f.settings.position.z};
            if (!scene.add_particle(f.texture, pos, f.settings.radius / 640, f.settings.color, f.alpha, 1, i, r,
                                    error) ||
                !scene.entries.back().error.empty())
                return false;
        }
        scene.sort();
        return true;
    };
    std::vector<std::uint8_t> full, half;
    Capture capture;
    CHECK(scene.begin_frame({}, {}, error) && add(fx, resources) && scene.draws.size() == 4);
    CHECK(render(runtime, renderer, scene, {}, {}, capture, full, error));
    CHECK(fx.Update(.5) && std::abs(fx.alpha - .5f) < .0001f);
    CHECK(scene.begin_frame({}, {}, error) && add(fx, resources));
    CHECK(render(runtime, renderer, scene, {}, {}, capture, half, error));
    unsigned ink = 0, red = 0;
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
        if (full[i * 4] >= 127 && full[i * 4] <= 128 && full[i * 4 + 1] == 0 && full[i * 4 + 2] == 0)
            ++red;
        if (changed)
            ++ink;
    }
    CHECK(ink > 50 && ink < 40000 && red > 10);
    CHECK(capture.save((std::filesystem::path(argv[1]) / "ring-alpha.png").c_str()));
    std::cout << "Original ring: " << ink << " pixels, tint and half-alpha linear blend verified\n";
    if (argc == 3)
    {
        GameSession game;
        Config config;
        config.data_dir = argv[2];
        CHECK(game.initialize() && game.resources.mount_game(config, error));
        ExplosionRings rings;
        CHECK(rings.publish(3, {}, {0, 1, 0}, "GPU ring", game.resources, 0, error) && error.empty());
        CHECK(rings.advance(.25) && rings.active.size() == 4);
        scene.clear();
        renderer.clear_geometry();
        const DVector eye{0, 0, -300};
        CHECK(scene.begin_frame(eye, {}, error));
        for (const auto& f : rings.active)
            CHECK(add(f, game.resources));
        CHECK(scene.draws.size() >= 32);
        CHECK(render(runtime, renderer, scene, eye, {}, capture, full, error));
        ink = 0;
        for (unsigned i = 0; i < 400 * 300; ++i)
            if (full[i * 4] != 6 || full[i * 4 + 1] != 9 || full[i * 4 + 2] != 13)
                ++ink;
        CHECK(ink > 50 && ink < 100000);
        CHECK(capture.save((std::filesystem::path(argv[1]) / "original-explosion-rings.png").c_str()));
        std::cout << "Original Firepart64/Smoke32 particles: " << scene.draws.size() << " quads, " << ink
                  << " pixels\n";
        // Activate every delayed smoke ring, then run its first visible fade update.
        CHECK(rings.advance(.6) && rings.advance(.7) && rings.active.size() == 3);
        CHECK(scene.begin_frame(eye, {}, error));
        for (const auto& f : rings.active)
        {
            CHECK(f.alpha > 0 && f.alpha < .5f && f.particles.size() == 32);
            CHECK(add(f, game.resources));
        }
        CHECK(scene.draws.size() == 96);
        CHECK(render(runtime, renderer, scene, eye, {}, capture, half, error));
        ink = 0;
        for (unsigned i = 0; i < 400 * 300; ++i)
            if (half[i * 4] != 6 || half[i * 4 + 1] != 9 || half[i * 4 + 2] != 13)
                ++ink;
        CHECK(ink > 50 && ink < 100000 && full != half);
        CHECK(capture.save((std::filesystem::path(argv[1]) / "original-explosion-smoke-rings.png").c_str()));
        std::cout << "Three original Smoke32 textures after delay: 96 visible quads, " << ink << " pixels\n";
        rings.clear();
        scene.clear();
        renderer.clear_geometry();
        CHECK(scene.draws.empty());
    }
    return 0;
}
