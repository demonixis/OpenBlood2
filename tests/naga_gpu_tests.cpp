#include "app/model_scene.h"
#include "engine/capture.h"
#include "engine/runtime.h"
#include "engine/world_renderer.h"
#include "naga_fixture.h"
#include <algorithm>
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
    GameSession game;
    CHECK(game.initialize());
    if (argc == 3)
    {
        Config config;
        config.data_dir = argv[2];
        CHECK(game.resources.mount_game(config, error));
    }
    else
        CHECK(test_naga::mount(game.resources, argv[1], error));
    auto& beams = game.server.laser_beams;
    CHECK(beams.publish({}, {0, 0, argc == 3 ? 100.f : 2.f}, "GPU beam", game.models, game.resources, 0, error));
    CHECK(beams.advance(0) && beams.advance(.25));
    ModelView scene;
    CHECK(update_explosion_model_scene(game, scene, error));
    CHECK(scene.entries.size() == 1 && scene.entries[0].error.empty() && scene.entries[0].laser_effect);
    CHECK(scene.world_draws.size() == 1 && scene.entries[0].triangles > 0);
    // Face framing avoids zooming out for the length of a beam seen end-on.
    const auto a = scene.mesh.vertices[0].position, b = scene.mesh.vertices[1].position,
               c = scene.mesh.vertices[2].position;
    auto normal = cross(sub(b, a), sub(c, a));
    const auto length = std::sqrt(normal.x * normal.x + normal.y * normal.y + normal.z * normal.z);
    CHECK(length > 0);
    normal = {normal.x / length, normal.y / length, normal.z / length};
    DRotation camera;
    CHECK(align_rotation(normal, camera));
    const DVector center{(a.x + b.x + c.x) / 3, (a.y + b.y + c.y) / 3, (a.z + b.z + c.z) / 3};
    float extent = 1;
    for (const auto vertex : {a, b, c})
    {
        const auto delta = sub(vertex, center);
        extent = std::max(extent, std::sqrt(delta.x * delta.x + delta.y * delta.y + delta.z * delta.z));
    }
    DVector eye{center.x - normal.x * extent * 2, center.y - normal.y * extent * 2, center.z - normal.z * extent * 2};
    Capture capture;
    std::vector<std::uint8_t> full, half;
    CHECK(render(runtime, renderer, scene, eye, camera, capture, full, error));
    std::filesystem::create_directories(argv[1]);
    CHECK(capture.save((std::filesystem::path(argv[1]) / "laser-full.png").c_str()));
    unsigned full_ink = 0;
    for (unsigned i = 0; i < 400 * 300; ++i)
        full_ink += full[i * 4] != 6 || full[i * 4 + 1] != 9 || full[i * 4 + 2] != 13;
    CHECK(full_ink > 10);
    // A transparent tube has overlapping faces: its whole-model blend is not linear
    // in alpha. Check the exact pixel equation on one actual face of that same model.
    auto single_face = [&]
    {
        scene.mesh.ranges[0].count = 3;
        scene.mesh.ranges[0].batches.resize(1);
        scene.mesh.ranges[0].batches[0].count = 3;
        ++scene.revision;
    };
    single_face();
    CHECK(render(runtime, renderer, scene, eye, camera, capture, full, error));
    const auto pose = beams.active[0].animation;
    CHECK(beams.advance(.375));
    // Fixed pose isolates original alpha from model animation in the pixel comparison.
    beams.active[0].animation = pose;
    CHECK(update_explosion_model_scene(game, scene, error));
    single_face();
    CHECK(render(runtime, renderer, scene, eye, camera, capture, half, error));
    unsigned ink = 0, mismatch = 0;
    const int bg[]{6, 9, 13};
    for (unsigned i = 0; i < 400 * 300; ++i)
    {
        bool changed = false;
        for (unsigned c = 0; c < 3; ++c)
        {
            const auto j = i * 4 + c;
            changed |= full[j] != bg[c];
            mismatch += std::abs(int(half[j]) - int((full[j] + bg[c]) / 2)) > 1;
        }
        ink += changed;
    }
    std::cout << "Whole model pixels " << full_ink << "; selected face " << ink << "; mismatched channels " << mismatch
              << "\n";
    CHECK(ink > 10 && ink < 100000 && !mismatch);
    std::filesystem::create_directories(argv[1]);
    CHECK(capture.save((std::filesystem::path(argv[1]) / "laser-beam.png").c_str()));
    std::cout << (argc == 3 ? "Private" : "Synthetic") << " red beam: " << scene.entries[0].triangles << " triangles, "
              << ink << " pixels, source alpha .5 -> .25 verified\n";
    CHECK(beams.advance(.5) && update_explosion_model_scene(game, scene, error) && scene.world_draws.empty());
    CHECK(game.unload_world() && beams.reports.empty());
    renderer.clear_geometry();
    return 0;
}
