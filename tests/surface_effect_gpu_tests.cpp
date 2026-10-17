#include "engine/capture.h"
#include "engine/runtime.h"
#include "engine/world_renderer.h"
#include "game/server_adapter.h"
#include <SDL3/SDL.h>
#include <cstring>
#include <iostream>
#include <limits>
using namespace OpenBlood2;
#define CHECK(x)                                                                                                       \
    do                                                                                                                 \
    {                                                                                                                  \
        if (!(x))                                                                                                      \
        {                                                                                                              \
            std::cerr << __LINE__ << ": " #x " " << error << ' ' << SDL_GetError() << '\n';                            \
            return 1;                                                                                                  \
        }                                                                                                              \
    } while (false)
namespace
{
    bool readback(SDL_GPUDevice* gpu, SDL_GPUCommandBuffer* cmd, Capture& c, std::vector<Uint8>& pixels)
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
        if (!SDL_SubmitGPUCommandBuffer(cmd) || !SDL_WaitForGPUIdle(gpu))
            return false;
        const auto* bytes = static_cast<const Uint8*>(SDL_MapGPUTransferBuffer(gpu, c.transfer, false));
        if (!bytes)
            return false;
        pixels.assign(bytes, bytes + c.width * c.height * 4);
        SDL_UnmapGPUTransferBuffer(gpu, c.transfer);
        return true;
    }
} // namespace
int main(int argc, char** argv)
{
    if (argc != 2 && argc != 3)
        return 2;
    std::string error;
    Runtime runtime;
    CHECK(runtime.initialize(true, error));
    std::cout << "GPU device: "
              << SDL_GetStringProperty(SDL_GetGPUDeviceProperties(runtime.gpu), SDL_PROP_GPU_DEVICE_NAME_STRING,
                                       "unreported")
              << '\n';
    WorldRenderer renderer(runtime.gpu);
    WorldMesh mesh;
    mesh.materials = {
        {"pattern", "", {2, 2, 1, 0, 0, {255, 0, 0, 255, 0, 255, 0, 255, 0, 0, 255, 255, 255, 255, 255, 255}}}};
    mesh.ranges.push_back({0, 24, {}, "same material", {}});
    for (unsigned i = 0; i < 4; ++i)
    {
        const float x = i % 2 ? .1f : -.9f, y = i < 2 ? .1f : -.9f;
        const auto first = static_cast<std::uint32_t>(mesh.vertices.size());
        for (auto p : {DVector{x, y, 2},
                       {x + .8f, y, 2},
                       {x + .8f, y + .8f, 2},
                       {x, y, 2},
                       {x + .8f, y + .8f, 2},
                       {x, y + .8f, 2}})
            mesh.vertices.push_back({p, {1, 1, 1}, {.125f, .125f}});
        mesh.ranges[0].batches.push_back({first,
                                          6,
                                          0,
                                          i >= 2,
                                          {x + .4f, y + .4f, 2},
                                          i == 1   ? std::array<double, 2>{0, .5}
                                          : i == 3 ? std::array<double, 2>{.5, 0}
                                                   : std::array<double, 2>{}});
    }
    WorldDraw draw;
    for (unsigned cycle = 0; cycle < 2; ++cycle)
    {
        Capture capture;
        const unsigned size = cycle ? 240 : 320;
        CHECK(capture.initialize(runtime.gpu, SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM, size, size));
        CHECK(renderer.prepare(mesh, cycle, SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM, size, size, error));
        const auto uploads = renderer.stats.uploads;
        std::vector<Uint8> at_zero, at_one, pixels;
        for (double time : {0., 1., 1., 2., 0., 100000001.})
        {
            CHECK(renderer.prepare(mesh, cycle, SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM, size, size, error));
            auto* cmd = SDL_AcquireGPUCommandBuffer(runtime.gpu);
            CHECK(cmd);
            CHECK(renderer.draw(cmd, capture.texture, mesh, std::span(&draw, 1), {}, {}, false, error, DrawLoad::Clear,
                                nullptr, WorldPass::Opaque, time));
            CHECK(renderer.draw(cmd, capture.texture, mesh, std::span(&draw, 1), {}, {}, false, error, DrawLoad::Load,
                                nullptr, WorldPass::Translucent, time));
            CHECK(readback(runtime.gpu, cmd, capture, pixels));
            auto pixel = [&](unsigned x, unsigned y, unsigned channel) { return pixels[(y * size + x) * 4 + channel]; };
            const unsigned left = size * 3 / 8, right = size * 5 / 8, top = size * 3 / 8, bottom = size * 5 / 8;
            CHECK(pixel(left, top, 0) == 255 &&
                  pixel(left, bottom, 0) == 255); // Static batches keep the shared image still.
            const bool moved = time == 1 || time == 100000001.;
            CHECK(pixel(right, top, moved ? 2 : 0) == 255 && pixel(right, top, moved ? 0 : 2) == 0);
            CHECK(pixel(right, bottom, moved ? 1 : 0) == 255 && pixel(right, bottom, moved ? 0 : 1) == 0);
            if (at_zero.empty())
                at_zero = pixels;
            if (moved && at_one.empty())
                at_one = pixels;
            CHECK(pixels == (moved ? at_one : at_zero));
            CHECK(renderer.stats.pan_batches == 1 && renderer.stats.alpha_pan_batches == 1 &&
                  renderer.stats.surface_time == time);
            CHECK(renderer.stats.uploads == uploads);
        }
        auto* bad = SDL_AcquireGPUCommandBuffer(runtime.gpu);
        CHECK(bad);
        CHECK(!renderer.draw(bad, capture.texture, mesh, std::span(&draw, 1), {}, {}, false, error, DrawLoad::Clear,
                             nullptr, WorldPass::Opaque, std::numeric_limits<double>::quiet_NaN()));
        CHECK(SDL_CancelGPUCommandBuffer(bad));
        mesh.ranges[0].batches[0].uv_rate[1] = std::numeric_limits<double>::infinity();
        bad = SDL_AcquireGPUCommandBuffer(runtime.gpu);
        CHECK(bad);
        CHECK(!renderer.draw(bad, capture.texture, mesh, std::span(&draw, 1), {}, {}, false, error));
        CHECK(SDL_CancelGPUCommandBuffer(bad));
        mesh.ranges[0].batches[0].uv_rate = {};
        CHECK(capture.save((std::string(argv[1]) + "-synthetic.png").c_str()));
        renderer.clear_geometry();
        CHECK(renderer.stats.pan_batches == 0 && renderer.stats.alpha_pan_batches == 0 &&
              renderer.stats.surface_time == 0);
    }
    if (argc == 3)
    {
        GameSession game;
        CHECK(game.initialize());
        Config config;
        config.data_dir = argv[2];
        CHECK(game.resources.mount_game(config, error));
        for (unsigned cycle = 0; cycle < 2; ++cycle)
        {
            CHECK(game.request_world("01_SubwayA", error) && game.enter_player(error) && game.step(1. / 60));
            CHECK(mesh.build(game.world, error, &game.resources) && mesh.effects.size() == 60);
            const auto view = game.client_shell.view;
            CHECK(view.object); // Original intro camera; freeze pose/objects to isolate texture motion.
            std::vector<WorldDraw> draws;
            WorldViewStats stats;
            CHECK(world_draws(mesh, game.objects, draws, stats, error));
            Capture capture;
            CHECK(capture.initialize(runtime.gpu, SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM, 640, 420));
            CHECK(renderer.prepare(mesh, 10 + cycle, SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM, 640, 420, error));
            const auto uploads = renderer.stats.uploads;
            std::vector<Uint8> a, b, c;
            const auto clock = game.server.world_time();
            for (unsigned frame = 0; frame < 3; ++frame)
            {
                const double time = frame == 1 ? clock + 1. / 64 : clock;
                auto* cmd = SDL_AcquireGPUCommandBuffer(runtime.gpu);
                CHECK(cmd);
                CHECK(renderer.draw(cmd, capture.texture, mesh, draws, view.position, view.rotation, true, error,
                                    DrawLoad::Clear, nullptr, WorldPass::Opaque, time));
                CHECK(renderer.draw(cmd, capture.texture, mesh, draws, view.position, view.rotation, true, error,
                                    DrawLoad::Load, nullptr, WorldPass::Translucent, time));
                CHECK(readback(runtime.gpu, cmd, capture, frame == 0 ? a : frame == 1 ? b : c));
                CHECK(renderer.stats.uploads == uploads && renderer.stats.pan_batches > 0);
                CHECK(capture.save(
                    (std::string(argv[1]) + "-subway-" + std::to_string(cycle) + "-" + std::to_string(frame) + ".png")
                        .c_str()));
            }
            CHECK(a == c && a != b && game.server.world_time() == clock);
            std::size_t changed = 0;
            for (std::size_t i = 0; i < a.size(); i += 4)
                changed += std::memcmp(a.data() + i, b.data() + i, 4) != 0;
            CHECK(changed > 100);
            for (unsigned y = 160; y < 240; ++y)
                for (unsigned x = 260; x < 380; ++x)
                    CHECK(std::memcmp(a.data() + (y * 640 + x) * 4, b.data() + (y * 640 + x) * 4, 4) ==
                          0); // Train front stays still.
            std::cout << "SubwayA intro: " << changed
                      << " pixels changed by 1/64s Pan advance with identical geometry/camera; reset exact, no extra "
                         "upload\n";
            CHECK(game.unload_world());
            mesh = {};
            renderer.clear_geometry();
        }
    }
    std::cout << "Pan GPU: independent static/animated shared material, opaque/alpha axes, clock repeat/reset/wrap, "
                 "invalid draws, resize/reload pass\n";
    return 0;
}
