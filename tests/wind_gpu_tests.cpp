#include "app/model_scene.h"
#include "engine/capture.h"
#include "engine/runtime.h"
#include "engine/world_renderer.h"
#include "wind_fixture.h"
using namespace OpenBlood2;
#define CHECK(x)                                                                                                       \
    do                                                                                                                 \
    {                                                                                                                  \
        if (!(x))                                                                                                      \
        {                                                                                                              \
            std::cerr << __LINE__ << ": " #x " " << error << " " << SDL_GetError() << "\n";                            \
            return 1;                                                                                                  \
        }                                                                                                              \
    } while (false)
namespace
{
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
    GameSession g;
    if (argc == 3)
    {
        Config config;
        config.data_dir = argv[2];
        CHECK(g.initialize() && g.resources.mount_game(config, error));
        CHECK(g.request_world("01_SubwayA", error));
    }
    else
    {
        CHECK(test_player::setup(g, argv[1]) && test_wind::mount(g.resources, argv[1], error));
    }
    auto* owner = test_energy::actor(g, {0, 50, 0});
    auto* weapon = test_wind::equip(owner);
    auto* projectile = test_wind::fire(g, weapon);
    CHECK(projectile);
    CHECK(g.objects.engine_message(projectile->m_hObject, MID_UPDATE, nullptr));
    CHECK(g.server.explosion_models.active.size() == 1 && g.objects.finish_frame());
    const auto start = g.server.explosion_models.active[0].start_time;
    CHECK(g.server.explosion_models.advance(start + .125));
    const auto& fx = g.server.explosion_models.active[0];
    CHECK(fx.scale.x == 850 && fx.alpha == .375f);
    // Isolate the actual model/skin/pose; translation only centers the diagnostic camera.
    ModelView scene;
    CHECK(scene.add(fx.model.get(), fx.animation, fx.skin_path, {{}, fx.rotation}, fx.scale, true, false, {},
                    "wind shockwave", g.resources, error, {}, fx.alpha, true));
    CHECK(scene.world_draws.size() == 1 && scene.entries[0].error.empty());
    std::error_code directory_error;
    std::filesystem::create_directories(argv[1], directory_error);
    CHECK(!directory_error);
    DVector eye{0, 0, -3000};
    DRotation camera;
    if (argc == 2)
    {
        const auto a = scene.mesh.vertices[0].position, b = scene.mesh.vertices[1].position,
                   c = scene.mesh.vertices[2].position;
        const auto ab = vector_subtract(b, a), ac = vector_subtract(c, a);
        DVector normal{ab.y * ac.z - ab.z * ac.y, ab.z * ac.x - ab.x * ac.z, ab.x * ac.y - ab.y * ac.x};
        normal = vector_scale(normal, 1 / std::sqrt(vector_dot(normal, normal)));
        CHECK(align_rotation(normal, camera));
        eye = vector_subtract(vector_scale(vector_add(vector_add(a, b), c), 1.f / 3), vector_scale(normal, 3000));
    }
    Capture capture;
    std::vector<std::uint8_t> pixels;
    CHECK(render(runtime, renderer, scene, eye, camera, capture, pixels, error));
    unsigned ink = 0;
    for (unsigned i = 0; i < 400 * 300; ++i)
        if (pixels[i * 4] != 6 || pixels[i * 4 + 1] != 9 || pixels[i * 4 + 2] != 13)
            ++ink;
    CHECK(ink > 50 && ink < 100000);
    CHECK(capture.save((std::filesystem::path(argv[1]) / "wind-shockwave.png").c_str()));
    std::cout << "Original shockwave scale850 alpha.375: " << scene.entries[0].triangles << " triangles, " << ink
              << " changed pixels\n";
    CHECK(g.server.explosion_models.advance(start + .5) && g.server.explosion_models.active.empty());
    scene.clear();
    renderer.clear_geometry();
    CHECK(g.unload_world() && g.server.explosion_lights.active.empty() && g.server.audio.voices().empty());
    CHECK(scene.world_draws.empty() && CDestructable::m_dwNumDestructables == 0);
    return 0;
}
