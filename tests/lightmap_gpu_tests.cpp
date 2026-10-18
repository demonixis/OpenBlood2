// Lightmapped world faces on a real device: texture * 2 * min(lightmap + radial light, 1)
// with the linear lightmap sampler, beside an unchanged vertex-lit face.
#include "engine/capture.h"
#include "engine/runtime.h"
#include "engine/world_renderer.h"
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
    bool download(SDL_GPUDevice* gpu, SDL_GPUCommandBuffer* cmd, Capture& c, std::vector<Uint8>& pixels)
    {
        auto* pass = SDL_BeginGPUCopyPass(cmd);
        if (!pass)
            return false;
        SDL_GPUTextureRegion source{};
        source.texture = c.texture;
        source.w = c.width;
        source.h = c.height;
        source.d = 1;
        SDL_GPUTextureTransferInfo target{};
        target.transfer_buffer = c.transfer;
        target.pixels_per_row = c.width;
        target.rows_per_layer = c.height;
        SDL_DownloadFromGPUTexture(pass, &source, &target);
        SDL_EndGPUCopyPass(pass);
        if (!SDL_SubmitGPUCommandBuffer(cmd) || !SDL_WaitForGPUIdle(gpu))
            return false;
        auto* bytes = static_cast<const Uint8*>(SDL_MapGPUTransferBuffer(gpu, c.transfer, false));
        if (!bytes)
            return false;
        pixels.assign(bytes, bytes + c.width * c.height * 4);
        SDL_UnmapGPUTransferBuffer(gpu, c.transfer);
        return true;
    }
    // Left half: lightmapped (uniform page texel 64/128/32); right half: vertex light .2.
    WorldMesh quads()
    {
        WorldMesh m;
        m.materials.push_back({"", "", {1, 1, 1, 0, 0, {255, 255, 255, 255}, {}}});
        WorldMaterial page;
        page.path = "lightmap page 0";
        page.image.width = page.image.height = 4;
        for (int i = 0; i < 16; ++i)
            page.image.rgba.insert(page.image.rgba.end(), {64, 128, 32, 255});
        m.materials.push_back(page);
        auto quad = [&](float x0, float x1, DVector color, std::array<float, 2> lightmap)
        {
            for (DVector p : {DVector{x0, 5, 5}, {x1, 5, 5}, {x1, -5, 5}, {x0, 5, 5}, {x1, -5, 5}, {x0, -5, 5}})
                m.vertices.push_back({p, color, {0, 0}, lightmap});
        };
        quad(-5, 0, {1, 1, 1}, {.5f, .5f});
        quad(0, 5, {.2f, .2f, .2f}, {-1, -1});
        WorldRange range{0, 12, {}, "quads", {}};
        range.batches.push_back({0, 6, 0, false, {}, {}, false, 1});
        range.batches.push_back({6, 6, 0, false, {}, {}, false, 0});
        m.ranges.push_back(range);
        return m;
    }
} // namespace
int main()
{
    std::string error;
    Runtime runtime;
    CHECK(runtime.initialize(true, error));
    std::cout << "GPU="
              << SDL_GetStringProperty(SDL_GetGPUDeviceProperties(runtime.gpu), SDL_PROP_GPU_DEVICE_NAME_STRING,
                                       "unknown")
              << " backend=" << SDL_GetGPUDeviceDriver(runtime.gpu) << '\n';
    WorldRenderer renderer(runtime.gpu);
    const auto mesh = quads();
    constexpr unsigned w = 320, h = 240;
    Capture capture;
    CHECK(capture.initialize(runtime.gpu, SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM, w, h));
    CHECK(renderer.prepare(mesh, 1, SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM, w, h, error));
    WorldDraw draw{};
    draw.rotation = {{0, 0, 0}, 1};
    std::vector<Uint8> pixels;
    for (int lit = 0; lit < 2; ++lit)
    {
        // A dim radial light covering both halves adds .1 before the lightmap scale.
        PointLight light{{0, 0, 5}, {.1f, .1f, .1f}, lit ? 1000.f : 0.f};
        auto* cmd = SDL_AcquireGPUCommandBuffer(runtime.gpu);
        CHECK(cmd);
        CHECK(renderer.draw(cmd, capture.texture, mesh, std::span(&draw, 1), {}, {{0, 0, 0}, 1}, false, error,
                            DrawLoad::Clear, nullptr, WorldPass::Opaque, 0, light));
        CHECK(download(runtime.gpu, cmd, capture, pixels));
        const auto* left = &pixels[(h / 2 * w + w / 4) * 4];
        const auto* right = &pixels[(h / 2 * w + w * 3 / 4) * 4];
        const float add = lit ? .1f : 0;
        const int expected_left[] = {int(std::lround(255 * std::min(1.f, 2 * std::min(1.f, 64 / 255.f + add)))),
                                     int(std::lround(255 * std::min(1.f, 2 * std::min(1.f, 128 / 255.f + add)))),
                                     int(std::lround(255 * std::min(1.f, 2 * std::min(1.f, 32 / 255.f + add))))};
        const int expected_right = int(std::lround(255 * (.2f + add)));
        for (int c = 0; c < 3; ++c)
        {
            CHECK(std::abs(int(left[c]) - expected_left[c]) <= 2);
            CHECK(std::abs(int(right[c]) - expected_right) <= 2);
        }
        std::cout << "lit=" << lit << " lightmapped " << int(left[0]) << " " << int(left[1]) << " " << int(left[2])
                  << " vertex " << int(right[0]) << '\n';
    }
    // A batch naming a missing page is rejected before any pass is recorded.
    auto broken = mesh;
    broken.ranges[0].batches[0].lightmap = 7;
    CHECK(renderer.prepare(broken, 2, SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM, w, h, error));
    auto* cmd = SDL_AcquireGPUCommandBuffer(runtime.gpu);
    CHECK(cmd);
    CHECK(!renderer.draw(cmd, capture.texture, broken, std::span(&draw, 1), {}, {{0, 0, 0}, 1}, false, error));
    CHECK(SDL_CancelGPUCommandBuffer(cmd));
    renderer.clear_geometry();
    std::cout << "GPU lightmap scale, additive radial light and vertex-lit faces pass\n";
    return 0;
}
