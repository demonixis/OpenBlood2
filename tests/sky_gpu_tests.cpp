#include "engine/capture.h"
#include "engine/runtime.h"
#include "engine/world_renderer.h"
#include "game/server_adapter.h"
#include <SDL3/SDL.h>
#include <cmath>
#include <filesystem>
#include <iostream>
using namespace OpenBlood2;
#define CHECK(x)                                                                                                       \
    do                                                                                                                 \
    {                                                                                                                  \
        if (!(x))                                                                                                      \
        {                                                                                                              \
            std::cerr << __LINE__ << ": " #x " " << error << " " << SDL_GetError() << '\n';                            \
            return 1;                                                                                                  \
        }                                                                                                              \
    } while (false)
namespace
{
    void quad(WorldMesh& mesh, float z, float x, float half, DVector color, bool alpha = false, bool portal = false)
    {
        const auto first = unsigned(mesh.vertices.size());
        for (auto p : {DVector{x - half, half, z}, DVector{x + half, half, z}, DVector{x + half, -half, z},
                       DVector{x - half, half, z}, DVector{x + half, -half, z}, DVector{x - half, -half, z}})
            mesh.vertices.push_back({p, color, {}});
        mesh.ranges.push_back({first, 6, {}, "quad", {{first, 6, alpha ? 1u : 0u, alpha, {x, 0, z}, {}, portal}}});
    }
    bool download(SDL_GPUDevice* gpu, SDL_GPUCommandBuffer* cmd, Capture& c)
    {
        auto* copy = SDL_BeginGPUCopyPass(cmd);
        if (!copy)
            return false;
        SDL_GPUTextureRegion region{};
        region.texture = c.texture;
        region.w = c.width;
        region.h = c.height;
        region.d = 1;
        SDL_GPUTextureTransferInfo target{};
        target.transfer_buffer = c.transfer;
        target.pixels_per_row = c.width;
        target.rows_per_layer = c.height;
        SDL_DownloadFromGPUTexture(copy, &region, &target);
        SDL_EndGPUCopyPass(copy);
        return SDL_SubmitGPUCommandBuffer(cmd) && SDL_WaitForGPUIdle(gpu);
    }
} // namespace
int main(int argc, char** argv)
{
    if (argc < 2 || argc > 3)
        return 2;
    std::filesystem::create_directories(argv[1]);
    std::string error;
    Runtime runtime;
    CHECK(runtime.initialize(true, error));
    WorldRenderer renderer(runtime.gpu);
    WorldMesh mesh;
    mesh.materials = {{"white", "", {1, 1, 1, 0, 0, {255, 255, 255, 255}}},
                      {"half", "", {1, 1, 1, 0, 0, {255, 255, 255, 128}}}};
    quad(mesh, .5f, 0, 1, {0, 0, 1});                      // sky near plane .01, ordinary world near plane 1
    quad(mesh, 3, 0, 6, {1, 0, 0}, true);                  // farther but later: index order, no depth rejection
    quad(mesh, 10, 0, 20, {.25f, .4f, .65f}, false, true); // portal marker must not cover the sky
    quad(mesh, 100, -25, 15, {0, 1, 0});                   // ordinary solid is farther than sky, but must cover it
    std::vector<WorldDraw> sky{{0, {}, {}}, {1, {}, {}}}, world{{2, {}, {}}, {3, {}, {}}};
    for (unsigned cycle = 0; cycle < 3; ++cycle)
    {
        Capture c;
        const unsigned w = cycle == 1 ? 400 : 320, h = 240;
        CHECK(c.initialize(runtime.gpu, SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM, w, h));
        CHECK(renderer.prepare(mesh, cycle, SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM, w, h, error));
        if (cycle == 1)
            std::swap(sky[0], sky[1]);
        auto* cmd = SDL_AcquireGPUCommandBuffer(runtime.gpu);
        CHECK(cmd);
        CHECK(renderer.draw(cmd, c.texture, mesh, sky, {}, {}, false, error, DrawLoad::Clear, nullptr, WorldPass::Sky));
        CHECK(renderer.draw(cmd, c.texture, mesh, world, {}, {}, false, error, DrawLoad::Load, nullptr,
                            WorldPass::OpaqueWithSky));
        CHECK(download(runtime.gpu, cmd, c));
        const auto* p = static_cast<const Uint8*>(SDL_MapGPUTransferBuffer(runtime.gpu, c.transfer, false));
        CHECK(p);
        auto rgb = [&](unsigned x, unsigned y, int r, int g, int b)
        {
            auto i = (y * w + x) * 4;
            return std::abs(int(p[i]) - r) <= 1 && std::abs(int(p[i + 1]) - g) <= 1 && std::abs(int(p[i + 2]) - b) <= 1;
        };
        bool ok = rgb(w / 2, h / 2, cycle ? 0 : 128, 0, cycle ? 255 : 127) && rgb(w * 3 / 8, h / 2, 0, 255, 0);
        SDL_UnmapGPUTransferBuffer(runtime.gpu, c.transfer);
        CHECK(ok && renderer.stats.sky_batches == 2);
        CHECK(c.save((std::string(argv[1]) + "/synthetic-" + std::to_string(cycle) + ".png").c_str()));
        auto bad = sky;
        bad[0].range = 99;
        cmd = SDL_AcquireGPUCommandBuffer(runtime.gpu);
        CHECK(cmd);
        CHECK(
            !renderer.draw(cmd, c.texture, mesh, bad, {}, {}, false, error, DrawLoad::Clear, nullptr, WorldPass::Sky));
        CHECK(SDL_CancelGPUCommandBuffer(cmd));
        renderer.clear_geometry();
    }
    std::cout << "Sky GPU: index order, alpha, .01 near plane, no depth reads/writes, portal omission, resize/reload "
                 "passed\n";
    if (argc == 3)
    {
        GameSession g;
        CHECK(g.initialize());
        Config config;
        config.data_dir = argv[2];
        CHECK(g.resources.mount_game(config, error));
        for (unsigned cycle = 0; cycle < 2; ++cycle)
        {
            CHECK(g.request_world("02_cityhub", error));
            CHECK(g.enter_player(error));
            CHECK(g.step(.02));
            WorldMesh real;
            CHECK(real.build(g.world, error, &g.resources));
            Capture c;
            CHECK(c.initialize(runtime.gpu, SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM, 640, 480));
            CHECK(renderer.prepare(real, g.world_revision, SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM, 640, 480, error));
            std::vector<WorldDraw> skies, ordinary;
            DVector eye;
            CHECK(g.server.sky.draws(g.world, real, g.objects, g.client_shell.view.position, skies, eye, error) &&
                  skies.size() == 5);
            WorldViewStats stats;
            CHECK(world_draws(real, g.objects, ordinary, stats, error));
            for (unsigned shot = 0; shot < 3; ++shot)
            {
                auto rotation = rotation_from_euler({shot == 0 ? -.2f : 0, shot == 0 ? 0 : 1.57f, 0});
                auto* cmd = SDL_AcquireGPUCommandBuffer(runtime.gpu);
                CHECK(cmd);
                CHECK(renderer.draw(cmd, c.texture, real, skies, eye, rotation, false, error, DrawLoad::Clear, nullptr,
                                    WorldPass::Sky, g.server.world_time()));
                if (shot == 2)
                    CHECK(renderer.draw(cmd, c.texture, real, ordinary, g.client_shell.view.position, rotation, false,
                                        error, DrawLoad::Load, nullptr, WorldPass::OpaqueWithSky,
                                        g.server.world_time()));
                CHECK(download(runtime.gpu, cmd, c));
                const auto* p = static_cast<const Uint8*>(SDL_MapGPUTransferBuffer(runtime.gpu, c.transfer, false));
                CHECK(p);
                unsigned covered = 0;
                for (unsigned i = 0; i < 640 * 480; ++i)
                    covered += p[i * 4] > 10 || p[i * 4 + 1] > 12 || p[i * 4 + 2] > 16;
                SDL_UnmapGPUTransferBuffer(runtime.gpu, c.transfer);
                CHECK(covered > 10000 && renderer.stats.sky_batches > 0);
                CHECK(c.save(
                    (std::string(argv[1]) + "/cityhub-" + std::to_string(cycle) + "-" + std::to_string(shot) + ".png")
                        .c_str()));
                std::cout << "City sky cycle=" << cycle << " view=" << shot
                          << " submitted=" << renderer.stats.sky_batches << " covered=" << covered << '\n';
            }
            CHECK(g.unload_world());
            renderer.clear_geometry();
        }
    }
    return 0;
}
