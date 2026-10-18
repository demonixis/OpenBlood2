#include "engine/capture.h"
#include "engine/runtime.h"
#include "engine/world_renderer.h"
#include <SDL3/SDL.h>
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
int main(int argc, char** argv)
{
    std::string error;
    Runtime runtime;
    CHECK(runtime.initialize(true, error));
    WorldMesh mesh;
    auto triangle = [&](DVector a, DVector b, DVector c, DVector color)
    {
        for (auto p : {a, b, c})
            mesh.vertices.push_back({p, color});
    };
    triangle({-1, -1, 2}, {1, -1, 2}, {0, 1, 2}, {1, 0, 0}); // Near red, submitted before far blue.
    triangle({-2, -2, 4}, {2, -2, 4}, {2, 2, 4}, {0, 0, 1});
    triangle({-2, -2, 4}, {-2, 2, 4}, {2, 2, 4}, {0, 0, 1});            // Opposite winding is visible too.
    triangle({-1, -1, .5f}, {1, -1, .5f}, {0, 1, .5f}, {1, 1, 0});      // Must be near-clipped.
    triangle({1, .6f, 2}, {1.5f, .6f, 2}, {1.25f, 1.2f, 2}, {0, 1, 0}); // +X right / +Y up.
    mesh.ranges.push_back({0, static_cast<unsigned>(mesh.vertices.size()), {}, "synthetic", {}});
    WorldRenderer renderer(runtime.gpu);
    for (int cycle = 0; cycle < 3; ++cycle)
    {
        const Uint32 w = cycle == 1 ? 240 : 320, h = cycle == 1 ? 320 : 240;
        CHECK(SDL_SetWindowSize(runtime.window, int(w), int(h)));
        SDL_PumpEvents();
        Capture capture;
        CHECK(capture.initialize(runtime.gpu, SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM, w, h));
        CHECK(renderer.prepare(mesh, cycle, SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM, w, h, error));
        CHECK(renderer.prepare(mesh, cycle, SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM, w, h, error));
        CHECK(renderer.stats.uploads == unsigned(cycle + 1));
        WorldDraw draw;
        DVector eye{};
        DRotation view;
        if (cycle == 2)
        {
            eye = draw.position = {100, 20, -50};
            view = draw.rotation = rotation_from_euler({0, 1.57079632679f, 0});
        }
        auto* cmd = SDL_AcquireGPUCommandBuffer(runtime.gpu);
        CHECK(cmd);
        SDL_GPUTexture* swap = nullptr;
        Uint32 sw = 0, sh = 0;
        CHECK(SDL_WaitAndAcquireGPUSwapchainTexture(cmd, runtime.window, &swap, &sw, &sh));
        CHECK(swap && sw && sh); // Compositor size may lag this test's offscreen target.
        CHECK(renderer.draw(cmd, capture.texture, mesh, std::span(&draw, 1), eye, view, false, error));
        CHECK(capture.enqueue(cmd, swap, sw, sh));
        CHECK(SDL_SubmitGPUCommandBuffer(cmd));
        CHECK(SDL_WaitForGPUIdle(runtime.gpu));
        auto* pixels = static_cast<const Uint8*>(SDL_MapGPUTransferBuffer(runtime.gpu, capture.transfer, false));
        CHECK(pixels);
        auto rgb = [&](int x, int y, int r, int g, int b)
        {
            auto* p = pixels + (y * w + x) * 4;
            bool ok = p[0] == r && p[1] == g && p[2] == b && p[3] == 255;
            if (!ok)
                std::cerr << "cycle " << cycle << " pixel " << x << "," << y << " = " << int(p[0]) << "," << int(p[1])
                          << "," << int(p[2]) << "," << int(p[3]) << "\n";
            return ok;
        };
        bool correct = rgb(w / 2, h / 2, 255, 0, 0) && rgb(w * 3 / 10, h / 2, 0, 0, 255) &&
                       rgb(w * 13 / 16, int(h / 2 - w * .2f), 0, 255, 0);
        SDL_UnmapGPUTransferBuffer(runtime.gpu, capture.transfer);
        if (!correct && argc == 2)
            capture.save(argv[1]);
        CHECK(correct);
        if (cycle == 2 && argc == 2)
            CHECK(capture.save(argv[1]));
        // The original camera type-1 viewport leaves one-sixth bars above/below.
        auto* bars = SDL_AcquireGPUCommandBuffer(runtime.gpu);
        CHECK(bars);
        CHECK(renderer.draw(bars, capture.texture, mesh, std::span(&draw, 1), eye, view, true, error));
        SDL_GPUCopyPass* copy = SDL_BeginGPUCopyPass(bars);
        CHECK(copy);
        SDL_GPUTextureRegion region{};
        region.texture = capture.texture;
        region.w = w;
        region.h = h;
        region.d = 1;
        SDL_GPUTextureTransferInfo destination{};
        destination.transfer_buffer = capture.transfer;
        destination.pixels_per_row = w;
        destination.rows_per_layer = h;
        SDL_DownloadFromGPUTexture(copy, &region, &destination);
        SDL_EndGPUCopyPass(copy);
        CHECK(SDL_SubmitGPUCommandBuffer(bars));
        CHECK(SDL_WaitForGPUIdle(runtime.gpu));
        pixels = static_cast<const Uint8*>(SDL_MapGPUTransferBuffer(runtime.gpu, capture.transfer, false));
        CHECK(pixels);
        correct = rgb(w / 2, 1, 6, 9, 13) && rgb(w / 2, h - 2, 6, 9, 13) && rgb(w / 2, h / 2, 255, 0, 0);
        SDL_UnmapGPUTransferBuffer(runtime.gpu, capture.transfer);
        CHECK(correct);
        renderer.clear_geometry();
    }
    // Known 2x2 colors, repeated twice in both directions. GPU pixels test the
    // sampler, row direction, material binding, opaque policy and reload ownership.
    for (int cycle = 0; cycle < 2; ++cycle)
    {
        WorldMesh textured;
        textured.materials = {
            {"white", "", {1, 1, 1, 0, 0, {255, 255, 255, 255}, {}}},
            {"synthetic", "", {2, 2, 1, 0, 0, {255, 0, 0, 0, 0, 255, 0, 0, 0, 0, 255, 0, 255, 255, 255, 0}, {}}}};
        if (cycle)
            textured.materials[1].image.rgba[2] = 255;
        textured.vertices = {{{-1, 1, 2}, {1, 1, 1}, {0, 0}}, {{1, 1, 2}, {1, 1, 1}, {2, 0}},
                             {{1, -1, 2}, {1, 1, 1}, {2, 2}}, {{-1, 1, 2}, {1, 1, 1}, {0, 0}},
                             {{1, -1, 2}, {1, 1, 1}, {2, 2}}, {{-1, -1, 2}, {1, 1, 1}, {0, 2}}};
        textured.ranges.push_back({0, 6, {}, "textured", {{0, 6, 1}}});
        WorldDraw draw;
        Capture capture;
        CHECK(capture.initialize(runtime.gpu, SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM, 320, 240));
        CHECK(renderer.prepare(textured, 10 + cycle, SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM, 320, 240, error));
        auto* cmd = SDL_AcquireGPUCommandBuffer(runtime.gpu);
        CHECK(cmd);
        CHECK(renderer.draw(cmd, capture.texture, textured, std::span(&draw, 1), {}, {}, false, error));
        auto* copy = SDL_BeginGPUCopyPass(cmd);
        CHECK(copy);
        SDL_GPUTextureRegion region{};
        region.texture = capture.texture;
        region.w = 320;
        region.h = 240;
        region.d = 1;
        SDL_GPUTextureTransferInfo target{};
        target.transfer_buffer = capture.transfer;
        target.pixels_per_row = 320;
        target.rows_per_layer = 240;
        SDL_DownloadFromGPUTexture(copy, &region, &target);
        SDL_EndGPUCopyPass(copy);
        CHECK(SDL_SubmitGPUCommandBuffer(cmd));
        CHECK(SDL_WaitForGPUIdle(runtime.gpu));
        auto* pixels = static_cast<const Uint8*>(SDL_MapGPUTransferBuffer(runtime.gpu, capture.transfer, false));
        CHECK(pixels);
        auto rgb = [&](int x, int y, int r, int g, int b)
        {
            const auto* p = pixels + (y * 320 + x) * 4;
            return p[0] == r && p[1] == g && p[2] == b && p[3] == 255;
        };
        const bool correct = rgb(90, 50, 255, 0, cycle ? 255 : 0) && rgb(130, 50, 0, 255, 0) &&
                             rgb(170, 50, 255, 0, cycle ? 255 : 0) && rgb(210, 50, 0, 255, 0) &&
                             rgb(90, 90, 0, 0, 255) && rgb(130, 90, 255, 255, 255) &&
                             rgb(90, 130, 255, 0, cycle ? 255 : 0);
        SDL_UnmapGPUTransferBuffer(runtime.gpu, capture.transfer);
        if (argc == 2)
            CHECK(capture.save((std::string(argv[1]) + ".textures.png").c_str()));
        CHECK(correct);
        // Rejected uploads must not leave a usable stale material cache.
        textured.materials[1].image.rgba.pop_back();
        CHECK(!renderer.prepare(textured, 100 + cycle, SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM, 320, 240, error));
        CHECK(!error.empty());
        renderer.clear_geometry();
    }
    std::cout << "GPU readback: depth occlusion, opposite winding, near clipping, +X/+Y orientation, camera/object "
                 "transforms, resize, textured repeat/row orientation/opaque sampling and reupload pass\n";
    return 0;
}
