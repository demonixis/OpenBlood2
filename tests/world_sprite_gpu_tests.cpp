#include "engine/capture.h"
#include "engine/runtime.h"
#include "engine/world_renderer.h"
#include "game/server_adapter.h"
#include <SDL3/SDL.h>
#include <algorithm>
#include <cmath>
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
    void quad(WorldMesh& mesh, float z, float half, std::uint32_t material, bool alpha, DVector color = {1, 1, 1})
    {
        const auto first = static_cast<std::uint32_t>(mesh.vertices.size());
        for (const auto& v : {WorldVertex{{-half, half, z}, color, {0, 0}},
                              {{half, half, z}, color, {1, 0}},
                              {{half, -half, z}, color, {1, 1}},
                              {{-half, half, z}, color, {0, 0}},
                              {{half, -half, z}, color, {1, 1}},
                              {{-half, -half, z}, color, {0, 1}}})
            mesh.vertices.push_back(v);
        mesh.ranges.push_back({first, 6, {}, "quad", {{first, 6, material, alpha, {0, 0, z}}}});
    }
    bool readback(SDL_GPUDevice* gpu, SDL_GPUCommandBuffer* cmd, Capture& c)
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
    if (argc != 2)
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
    mesh.materials = {{"white", "", {1, 1, 1, 0, 0, {255, 255, 255, 255}}},
                      {"red", "", {1, 1, 1, 0, 0, {255, 0, 0, 255}}},
                      {"green", "", {1, 1, 1, 0, 0, {0, 255, 0, 255}}},
                      {"animated.spr", "", {1, 1, 0, 0, 0, {}}, 8, {1, 2}}};
    quad(mesh, 2, 1, 3, false);
    WorldDraw draw;
    for (unsigned cycle = 0; cycle < 2; ++cycle)
    {
        mesh.ranges[0].batches[0].translucent = cycle != 0;
        Capture capture;
        CHECK(capture.initialize(runtime.gpu, SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM, 128, 128));
        CHECK(renderer.prepare(mesh, cycle, SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM, 128, 128, error));
        const auto uploads = renderer.stats.uploads;
        for (double time : {0., .125, .125, .25, 0.})
        {
            auto* cmd = SDL_AcquireGPUCommandBuffer(runtime.gpu);
            CHECK(cmd);
            CHECK(renderer.draw(cmd, capture.texture, mesh, std::span(&draw, 1), {}, {}, false, error, DrawLoad::Clear,
                                nullptr, WorldPass::Opaque, time));
            CHECK(renderer.draw(cmd, capture.texture, mesh, std::span(&draw, 1), {}, {}, false, error, DrawLoad::Load,
                                nullptr, WorldPass::Translucent, time));
            CHECK(readback(runtime.gpu, cmd, capture));
            const auto* pixels =
                static_cast<const Uint8*>(SDL_MapGPUTransferBuffer(runtime.gpu, capture.transfer, false));
            CHECK(pixels);
            const auto offset = (64 * 128 + 64) * 4;
            const auto red = pixels[offset], green = pixels[offset + 1], blue = pixels[offset + 2];
            SDL_UnmapGPUTransferBuffer(runtime.gpu, capture.transfer);
            CHECK((time == .125 ? green == 255 && red == 0 : red == 255 && green == 0) && blue == 0);
            CHECK(renderer.stats.uploads == uploads);
            CHECK(capture.save(
                (std::string(argv[1]) + "-" + std::to_string(cycle) + "-" + std::to_string(time) + ".png").c_str()));
        }
        renderer.clear_geometry();
    }
    std::cout << "World SPR: opaque/translucent frame pixels, repeated time, wrap/reset, unload/reload; no per-frame "
                 "uploads\n";
    return 0;
}
