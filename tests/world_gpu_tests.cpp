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
    mesh.ranges.push_back({0, static_cast<unsigned>(mesh.vertices.size()), {}, "synthetic"});
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
        CHECK(swap);
        CHECK(renderer.draw(cmd, capture.texture, mesh, std::span(&draw, 1), eye, view, false, error));
        CHECK(capture.enqueue(cmd, swap));
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
    std::cout << "GPU readback: depth occlusion, opposite winding, near clipping, +X/+Y orientation, camera/object "
                 "transforms, resize and reupload pass\n";
    return 0;
}
