#include "app/sprite_scene.h"
#include "engine/capture.h"
#include "engine/runtime.h"
#include "engine/world_renderer.h"
#include "explosion_fragment_fixture.h"
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
    Capture capture;
    CHECK(test_explosion_fragment::mount(resources, argv[1], error));
    CObjectParticleFX trail;
    NodePose parent{{-2, 0, 20}, {}};
    std::minstd_rand random(1);
    CHECK(trail.Init(1, 1, error) && trail.CreateObject(resources, parent, 0, error) &&
          trail.Update(0, &parent, random));
    parent.position.x = 0;
    CHECK(trail.Update(.02, &parent, random) && trail.particles.size() == 1);
    auto add = [&](const Resources& r, bool white = false)
    {
        if (!scene.begin_frame({}, {}, error))
            return false;
        const auto& v = trail.particles.front();
        return scene.add_particle(trail.texture, vector_add(trail.origin, v.position), trail.radius / 640.f,
                                  white ? DVector{1, 1, 1} : v.color, trail.alpha, 1, 0, r, error) &&
               scene.entries.back().error.empty();
    };
    std::vector<std::uint8_t> white, tinted;
    CHECK(add(resources, true) && render(runtime, renderer, scene, {}, {}, capture, white, error));
    const auto updates = trail.updates;
    CHECK(add(resources) && render(runtime, renderer, scene, {}, {}, capture, tinted, error) &&
          trail.updates == updates);
    unsigned opaque = 0, ink = 0;
    const auto gray = trail.particles[0].color.x;
    for (unsigned i = 0; i < 400 * 300; ++i)
    {
        const auto k = i * 4;
        if (tinted[k] != 6 || tinted[k + 1] != 9 || tinted[k + 2] != 13)
            ++ink;
        if (white[k] == 255 && white[k + 1] == 0 && white[k + 2] == 0)
        {
            CHECK(std::abs(int(tinted[k]) - 255 * gray) <= 1 && tinted[k + 1] == 0 && tinted[k + 2] == 0);
            ++opaque;
        }
    }
    CHECK(ink > 50 && ink < 40000 && opaque > 10 && trail.alpha == 1 && trail.requested_alpha == .25f);
    CHECK(capture.save((std::filesystem::path(argv[1]) / "smoke-trail-tint.png").c_str()));
    std::cout << "Original smoke stream: " << ink << " covered pixels, " << opaque
              << " opaque tint checks; authored alpha remains unused\n";
    if (argc == 3)
    {
        Resources original;
        Config config;
        config.data_dir = argv[2];
        CHECK(original.mount_game(config, error));
        parent = {{-10, 0, 20}, {}};
        CHECK(trail.Init(1, 2, error) && trail.CreateObject(original, parent, 0, error) &&
              trail.Update(0, &parent, random));
        parent.position.x = 0;
        CHECK(trail.Update(.02, &parent, random) && trail.particles.size() == 5);
        scene.clear();
        renderer.clear_geometry();
        CHECK(scene.begin_frame({}, {}, error));
        for (unsigned i = 0; i < trail.particles.size(); ++i)
        {
            const auto& v = trail.particles[i];
            CHECK(scene.add_particle(trail.texture, vector_add(trail.origin, v.position), trail.radius / 640.f, v.color,
                                     trail.alpha, 2, i, original, error) &&
                  scene.entries.back().error.empty());
        }
        scene.sort();
        CHECK(render(runtime, renderer, scene, {}, {}, capture, tinted, error));
        ink = 0;
        for (unsigned i = 0; i < 400 * 300; ++i)
            if (tinted[i * 4] != 6 || tinted[i * 4 + 1] != 9 || tinted[i * 4 + 2] != 13)
                ++ink;
        CHECK(ink > 50 && ink < 100000 && scene.entries[0].partial_alpha > 0);
        CHECK(capture.save((std::filesystem::path(argv[1]) / "original-smoke-trail.png").c_str()));
        std::cout << "Original smoke64_2: five stream quads, " << ink << " covered pixels, "
                  << scene.entries[0].partial_alpha << " partial-alpha texels\n";
        CHECK(!trail.Update(.03, nullptr, random) && trail.error.empty());
        scene.clear();
        renderer.clear_geometry();
        CHECK(scene.draws.empty());
    }
    return 0;
}
