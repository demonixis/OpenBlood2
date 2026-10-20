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
    Config c;
    c.data_dir = argv[2];
    CHECK(g.initialize() && g.resources.mount_game(c, error));
    std::error_code ec;
    std::filesystem::create_directories(argv[1], ec);
    CHECK(!ec);
    auto owner = g.objects.create(g.objects.find_class("BaseClass"), {});
    CHECK(owner);
    g.client_shell.view.position = {0, 0, -1000};
    CHECK(g.server.PublishLaserBeam(owner, {-300, 0, 0}, {300, 0, 0}, error, 1) && error.empty());
    CHECK(g.server.laser_beams.advance(0) && g.server.laser_beams.advance(.2));
    CHECK(update_explosion_model_scene(g, models, error) && update_sprite_scene(g, scene, error));
    CHECK(models.mesh.vertices.size() >= 3);
    CHECK(models.world_draws.size() == 1);
    const auto& draw = models.world_draws[0];
    auto world = [&](DVector p) { return vector_add(draw.position, rotate_vector(draw.rotation, p)); };
    const auto a = world(models.mesh.vertices[0].position), b = world(models.mesh.vertices[1].position),
               third = world(models.mesh.vertices[2].position);
    const auto u = vector_subtract(b, a), v = vector_subtract(third, a);
    DVector normal{u.y * v.z - u.z * v.y, u.z * v.x - u.x * v.z, u.x * v.y - u.y * v.x};
    normal = vector_scale(normal, 1 / std::sqrt(vector_dot(normal, normal)));
    DRotation camera;
    CHECK(align_rotation(normal, camera));
    const auto center = vector_scale(vector_add(vector_add(a, b), third), 1.f / 3);
    float extent = 1;
    for (auto point : {a, b, third})
    {
        auto d = vector_subtract(point, center);
        extent = std::max(extent, std::sqrt(vector_dot(d, d)));
    }
    const auto eye = vector_subtract(center, vector_scale(normal, extent * 2));
    CHECK(render(runtime, renderer, model_renderer, models, scene, eye, camera, capture, pixels, error));
    CHECK(capture.save((std::filesystem::path(argv[1]) / "ancient-beam.png").c_str()));
    unsigned red = 0;
    for (unsigned i = 0; i < pixels.size(); i += 4)
        red += pixels[i] > 40 && pixels[i] > pixels[i + 2] + 10;
    std::cout << "Ancient red-large beam pixels=" << red << "\n";
    CHECK(red > 100);
    CHECK(capture.save((std::filesystem::path(argv[1]) / "ancient-beam.png").c_str()));
    g.server.laser_beams.clear();
    CHECK(g.server.PublishExplosionModels(owner, 4, {}, {0, 1, 0}, error) && error.empty());
    CHECK(g.server.PublishExplosionSprites(owner, 4, {}, {0, 1, 0}, error) && error.empty());
    CHECK(g.server.PublishExplosionRings(owner, 4, {}, {0, 1, 0}, error) && error.empty());
    CHECK(g.server.explosion_models.active.size() == 3 && g.server.explosion_sprites.active.size() == 2 &&
          g.server.explosion_rings.active.size() == 1);
    CHECK(g.server.explosion_models.advance(.2) && g.server.explosion_sprites.advance(.2) &&
          g.server.explosion_rings.advance(.2));
    g.client_shell.view.position = {0, 100, -2000};
    CHECK(update_explosion_model_scene(g, models, error) && update_sprite_scene(g, scene, error));
    CHECK(render(runtime, renderer, model_renderer, models, scene, g.client_shell.view.position, {}, capture, pixels,
                 error));
    unsigned lit = 0;
    for (unsigned i = 0; i < pixels.size(); i += 4)
        lit += pixels[i] > 40 || pixels[i + 1] > 40 || pixels[i + 2] > 40;
    std::cout << "Ancient napalm impact pixels=" << lit << " models=" << models.entries.size()
              << " sprites=" << scene.entries.size() << "\n";
    CHECK(lit > 100 && lit < 400 * 300 / 2);
    CHECK(capture.save((std::filesystem::path(argv[1]) / "ancient-impact.png").c_str()));
    CHECK(g.server.explosion_models.advance(6) && g.server.explosion_sprites.advance(6) &&
          g.server.explosion_rings.advance(6));
    CHECK(update_explosion_model_scene(g, models, error) && update_sprite_scene(g, scene, error));
    CHECK(render(runtime, renderer, model_renderer, models, scene, g.client_shell.view.position, {}, capture, pixels,
                 error));
    for (unsigned i = 0; i < pixels.size(); i += 4)
        CHECK(pixels[i] == 6 && pixels[i + 1] == 9 && pixels[i + 2] == 13);
    CHECK(g.unload_world());
    return 0;
}
