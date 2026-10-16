#include "app/model_scene.h"
#include "brush_fixture.h"
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
    CHECK(test_explosion_fragment::mount(game.resources, argv[1], error));
    auto floor = test_brush::box("floor", {-1000, -10, -1000}, {1000, 0, 1000});
    for (auto& s : floor.surfaces)
        s.flags = 1;
    CHECK(game.collision.build(floor, error));
    game.server.collision_ready = true;
    ExplosionFragSettings settings;
    settings.position = {0, 1, 0};
    settings.normal = {0, -1, 0};
    settings.scale = {1, 1, 1};
    settings.velocity = 1;
    settings.gravity = 0;
    settings.duration = 1;
    settings.fade_time = 1;
    CExplosionFragFX fx;
    std::minstd_rand random(1);
    CHECK(fx.Init(settings, error) && fx.CreateObject(game.models, game.resources, 0, random, error) &&
          fx.Update(0, game.server));
    CHECK(fx.impacts == 1);
    ModelView scene;
    Capture capture;
    std::vector<std::uint8_t> full, half;
    auto add = [&](const Resources& resources)
    {
        scene.begin_frame();
        return scene.add(fx.model.get(), fx.animation, fx.skin_path, {fx.position, fx.rotation}, fx.settings.scale,
                         true, false, {}, fx.model_path, resources, error, {}, fx.alpha, true) &&
               scene.entries[0].error.empty();
    };
    CHECK(add(game.resources));
    auto camera_for = [&](float distance, DVector& eye, DRotation& camera)
    {
        if (scene.mesh.vertices.size() < 3)
            return false;
        auto a = scene.mesh.vertices[0].position, b = scene.mesh.vertices[1].position,
             c = scene.mesh.vertices[2].position;
        auto normal = cross(sub(b, a), sub(c, a));
        float length = std::sqrt(vector_dot(normal, normal));
        if (length <= 0 || !align_rotation(normal, camera))
            return false;
        normal = vector_scale(normal, 1 / length);
        const DVector center{(a.x + b.x + c.x) / 3 + fx.position.x, (a.y + b.y + c.y) / 3 + fx.position.y,
                             (a.z + b.z + c.z) / 3 + fx.position.z};
        eye = vector_subtract(center, vector_scale(normal, distance));
        return true;
    };
    DVector eye;
    DRotation camera;
    CHECK(camera_for(10, eye, camera));
    CHECK(render(runtime, renderer, scene, eye, camera, capture, full, error));
    CHECK(fx.Update(.5, game.server) && !fx.moving && std::abs(fx.alpha - .5f) < .0001f && add(game.resources));
    CHECK(render(runtime, renderer, scene, eye, camera, capture, half, error));
    unsigned ink = 0;
    const int background[] = {6, 9, 13};
    for (unsigned i = 0; i < 400 * 300; ++i)
    {
        bool changed = false;
        for (unsigned c = 0; c < 3; ++c)
        {
            auto index = i * 4 + c;
            changed |= full[index] != background[c];
            CHECK(std::abs(int(half[index]) - int((full[index] + background[c]) / 2)) <= 1);
        }
        if (changed)
            ++ink;
    }
    CHECK(ink > 50 && ink < 40000);
    CHECK(capture.save((std::filesystem::path(argv[1]) / "fragment-alpha.png").c_str()));
    std::cout << "Original fragment after collision/rest: " << ink << " pixels, half-opacity blend verified\n";
    if (argc == 3)
    {
        Resources original;
        ModelStore models(original);
        Config config;
        config.data_dir = argv[2];
        CHECK(original.mount_game(config, error));
        settings.model = "Models\\Gibs\\gibmetal2.abc";
        settings.skin = "Skins\\Gibs\\gibmetal3.dtx";
        CHECK(fx.Init(settings, error) && fx.CreateObject(models, original, 0, random, error) &&
              fx.Update(0, game.server));
        scene.clear();
        renderer.clear_geometry();
        CHECK(add(original));
        CHECK(camera_for(20, eye, camera) && render(runtime, renderer, scene, eye, camera, capture, full, error));
        ink = 0;
        for (unsigned i = 0; i < 400 * 300; ++i)
            if (full[i * 4] != 6 || full[i * 4 + 1] != 9 || full[i * 4 + 2] != 13)
                ++ink;
        CHECK(ink > 50 && ink < 100000);
        CHECK(capture.save((std::filesystem::path(argv[1]) / "original-metal-fragment.png").c_str()));
        std::cout << "Original gibmetal2/gibmetal3: " << scene.entries[0].triangles << " triangles, " << ink
                  << " pixels\n";
        CHECK(!fx.Update(1, game.server) && fx.error.empty());
        scene.clear();
        renderer.clear_geometry();
    }
    return 0;
}
