#include "app/model_scene.h"
#include "app/sprite_scene.h"
#include "engine/capture.h"
#include "engine/collision.h"
#include "engine/runtime.h"
#include "engine/world_renderer.h"
#include "game/original/AI_Mgr.h"
#include "game/original/Corpse.h"
#include "game/original/Gib.h"
#include <algorithm>
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
    bool render(Runtime& runtime, WorldRenderer& renderer, const WorldMesh& mesh, std::uint64_t revision,
                const std::vector<WorldDraw>& draws, DVector eye, DRotation camera, Capture& capture,
                std::vector<std::uint8_t>& pixels, std::string& error)
    {
        if ((!capture.texture && !capture.initialize(runtime.gpu, SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM, 400, 300)) ||
            !renderer.prepare(mesh, revision, SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM, 400, 300, error, true))
            return false;
        auto* commands = SDL_AcquireGPUCommandBuffer(runtime.gpu);
        if (!commands)
            return false;
        if (!renderer.draw(commands, capture.texture, mesh, draws, eye, camera, false, error))
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
    std::string error;
    Runtime runtime;
    CHECK(runtime.initialize(true, error));
    WorldRenderer renderer(runtime.gpu, DrawStyle::SpriteAlpha);
    GameSession game;
    CHECK(game.initialize());
    Config config;
    config.data_dir = argv[2];
    CHECK(game.resources.mount_game(config, error));
    WorldModel floor;
    floor.points = {{-4000, 0, -4000}, {4000, 0, -4000}, {4000, 0, 4000}, {-4000, 0, 4000}};
    floor.surfaces.resize(1);
    floor.surfaces[0].flags = 1;
    floor.polygons = {{0, {0, 1, 2, 3}}};
    CHECK(game.collision.build(floor, error));
    game.server.collision_ready = true;
    std::filesystem::create_directories(argv[1]);
    Capture capture;
    std::vector<std::uint8_t> pixels, again;
    for (auto surface : {10u, 20u, 30u, 50u, 70u, 90u, 100u, 40u, 200u})
    {
        DebrisEffect event;
        event.source = "GPU destruction";
        event.flags = surface / 10 | 0x200;
        event.position = {0, 50, 0};
        event.velocity = {0, 15, 0};
        event.amount = 1;
        event.scale = 1;
        game.server.RecordDebrisEffect(event);
        CHECK(game.server.debris_effects.back().error.empty() && game.server.destruction_debris.active.size() == 1);
        // Inspect the broad faces, not the initially edge-on glass sheet.
        game.server.destruction_debris.active[0].rotation = rotation_from_euler({.6f, .7f, .2f});
        ModelView scene;
        CHECK(update_explosion_model_scene(game, scene, error));
        CHECK(scene.entries.size() == 1 && scene.entries[0].triangles > 0 && scene.entries[0].error.empty());
        DVector low{1e6f, 1e6f, 1e6f}, high{-1e6f, -1e6f, -1e6f};
        for (const auto& vertex : scene.mesh.vertices)
        {
            const auto point = rotate_vector(game.server.destruction_debris.active[0].rotation, vertex.position);
            low.x = std::min(low.x, point.x);
            low.y = std::min(low.y, point.y);
            low.z = std::min(low.z, point.z);
            high.x = std::max(high.x, point.x);
            high.y = std::max(high.y, point.y);
            high.z = std::max(high.z, point.z);
        }
        const float radius = std::max({high.x - low.x, high.y - low.y, high.z - low.z, 10.f});
        const DVector eye{(low.x + high.x) * .5f, 50 + (low.y + high.y) * .5f, (low.z + high.z) * .5f - radius * 1.5f};
        CHECK(
            render(runtime, renderer, scene.mesh, scene.revision, scene.world_draws, eye, {}, capture, pixels, error));
        unsigned ink = 0;
        for (unsigned i = 0; i < 400 * 300; ++i)
            if (pixels[i * 4] != 6 || pixels[i * 4 + 1] != 9 || pixels[i * 4 + 2] != 13)
                ++ink;
        CHECK(ink > 30);
        CHECK(capture.save((std::filesystem::path(argv[1]) / (std::to_string(surface) + ".png")).string().c_str()));
        CHECK(
            render(runtime, renderer, scene.mesh, scene.revision, scene.world_draws, eye, {}, capture, again, error) &&
            pixels == again);
        if (surface == 50)
        {
            CHECK(scene.entries[0].alpha == .4f);
            game.server.destruction_debris.active[0].alpha = 1;
            CHECK(update_explosion_model_scene(game, scene, error));
            CHECK(render(runtime, renderer, scene.mesh, scene.revision, scene.world_draws, eye, {}, capture, again,
                         error) &&
                  pixels != again);
            game.server.destruction_debris.active[0].alpha = .4f;
        }
        CHECK(game.server.destruction_debris.advance(.1, game.server));
        CHECK(update_explosion_model_scene(game, scene, error));
        CHECK(
            render(runtime, renderer, scene.mesh, scene.revision, scene.world_draws, eye, {}, capture, again, error) &&
            pixels != again);
        std::cout << "Surface " << surface << ": " << ink << " pixels, moving and independently owned\n";
        game.server.reset_world_time();
        CHECK(update_explosion_model_scene(game, scene, error) && scene.entries.empty());
    }
    return 0;
}
