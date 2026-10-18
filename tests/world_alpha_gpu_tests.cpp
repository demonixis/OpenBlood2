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
    if (argc != 2 && argc != 3)
        return 2;
    std::string error;
    Runtime runtime;
    CHECK(runtime.initialize(true, error));
    WorldRenderer renderer(runtime.gpu), overlay(runtime.gpu);
    WorldMesh mesh;
    mesh.materials = {
        {"white", "", {1, 1, 1, 0, 0, {255, 255, 255, 255}, {}}},
        {"red alpha", "", {2, 2, 1, 0, 0, {255, 0, 0, 0, 255, 0, 0, 128, 255, 0, 0, 255, 0, 0, 0, 255}, {}}},
        {"green alpha", "", {1, 1, 1, 0, 0, {0, 255, 0, 128}, {}}}};
    quad(mesh, 2, 1, 1, true); // Near alpha FIRST, far alpha LAST: renderer must sort faces.
    quad(mesh, 6, 6, 0, false, {0, 0, 1});
    quad(mesh, 3, 1.5f, 2, true);
    std::vector<WorldDraw> draws = {{0, {}, {}}, {1, {}, {}}, {2, {}, {}}};
    for (unsigned cycle = 0; cycle < 5; ++cycle)
    {
        const unsigned w = cycle == 1 ? 400 : 320, h = cycle == 1 ? 300 : 240;
        Capture capture;
        CHECK(capture.initialize(runtime.gpu, SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM, w, h));
        CHECK(renderer.prepare(mesh, cycle, SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM, w, h, error));
        WorldMesh solid;
        // Separate opaque model between panes, or opaque object behind glass submitted AFTER alpha
        // to verify the alpha pass does not write depth. Final cycle tests a foreground occluder.
        quad(solid, cycle == 3 ? 1.5f : 2.5f, 20, 0, false, {1, 1, 0});
        CHECK(overlay.prepare(solid, cycle, SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM, w, h, error));
        WorldDraw single;
        DVector eye{};
        DRotation view;
        if (cycle == 4)
        {
            eye = {100, 20, -50};
            view = rotation_from_euler({0, 1.57079632679f, 0});
            for (auto& d : draws)
            {
                d.position = eye;
                d.rotation = view;
            }
        }
        auto* cmd = SDL_AcquireGPUCommandBuffer(runtime.gpu);
        CHECK(cmd);
        CHECK(renderer.draw(cmd, capture.texture, mesh, draws, eye, view, false, error));
        if (cycle == 1 || cycle == 3)
            CHECK(overlay.draw(cmd, capture.texture, solid, std::span(&single, 1), {}, {}, false, error, DrawLoad::Load,
                               renderer.depth_target()));
        CHECK(renderer.draw(cmd, capture.texture, mesh, draws, eye, view, false, error, DrawLoad::Load, nullptr,
                            WorldPass::Translucent));
        CHECK(renderer.stats.alpha_batches == 2);
        if (cycle == 2)
            CHECK(overlay.draw(cmd, capture.texture, solid, std::span(&single, 1), {}, {}, false, error, DrawLoad::Load,
                               renderer.depth_target()));
        CHECK(readback(runtime.gpu, cmd, capture));
        auto* pixels = static_cast<const Uint8*>(SDL_MapGPUTransferBuffer(runtime.gpu, capture.transfer, false));
        CHECK(pixels);
        auto rgb = [&](int x, int y, int r, int g, int b)
        {
            auto* p = pixels + (y * w + x) * 4;
            bool ok = std::abs(int(p[0]) - r) <= 1 && std::abs(int(p[1]) - g) <= 1 && std::abs(int(p[2]) - b) <= 1 &&
                      p[3] == 255;
            if (!ok)
                std::cerr << "cycle=" << cycle << " pixel=" << x << ',' << y << " got=" << int(p[0]) << ',' << int(p[1])
                          << ',' << int(p[2]) << '\n';
            return ok;
        };
        bool ok = false;
        if (cycle == 2 || cycle == 3)
            ok = rgb(w * 3 / 8, h / 2 - w / 8, 255, 255, 0) && rgb(w * 5 / 8, h / 2 + w / 8, 255, 255, 0);
        else
            ok = rgb(w * 3 / 8, h / 2 - w / 8, cycle == 1 ? 255 : 0, cycle == 1 ? 255 : 128, cycle == 1 ? 0 : 127) &&
                 rgb(w * 5 / 8, h / 2 - w / 8, cycle == 1 ? 255 : 128, cycle == 1 ? 127 : 64, cycle == 1 ? 0 : 63) &&
                 rgb(w * 3 / 8, h / 2 + w / 8, 255, 0, 0) && rgb(w * 5 / 8, h / 2 + w / 8, 0, 0, 0);
        SDL_UnmapGPUTransferBuffer(runtime.gpu, capture.transfer);
        CHECK(capture.save((std::string(argv[1]) + "-" + std::to_string(cycle) + ".png").c_str()));
        CHECK(ok);
        // Invalid order/camera rejects before any render pass; subsequent clean draw can recover.
        auto bad = draws;
        bad[0].range = 99;
        cmd = SDL_AcquireGPUCommandBuffer(runtime.gpu);
        CHECK(cmd);
        CHECK(!renderer.draw(cmd, capture.texture, mesh, bad, eye, view, false, error, DrawLoad::Load, nullptr,
                             WorldPass::Translucent));
        CHECK(SDL_CancelGPUCommandBuffer(cmd));
        renderer.clear_geometry();
    }
    std::cout << "GPU world alpha: zero/partial/full/black texels, far-to-near faces, opaque model occlusion, no depth "
                 "writes, resize/reupload pass\n";
    if (argc == 3)
    {
        GameSession game;
        CHECK(game.initialize());
        Config config;
        config.data_dir = argv[2];
        CHECK(game.resources.mount_game(config, error));
        for (unsigned cycle = 0; cycle < 2; ++cycle)
        {
            CHECK(game.request_world("01_SubwayA", error));
            CHECK(game.enter_player(error));
            WorldMesh real;
            CHECK(real.build(game.world, error, &game.resources));
            std::vector<WorldDraw> all;
            WorldViewStats stats;
            CHECK(world_draws(real, game.objects, all, stats, error));
            auto it = std::find_if(all.begin(), all.end(),
                                   [&](const auto& d) { return real.ranges[d.range].name == "glass12"; });
            CHECK(it != all.end());
            const auto& batch = real.ranges[it->range].batches[0];
            CHECK(batch.translucent);
            auto center = vector_add(it->position, rotate_vector(batch.center, it->rotation));
            const auto camera = vector_add(center, {128, 0, 0});
            const auto rotation = rotation_from_euler({0, -1.57079632679f, 0});
            Capture capture;
            CHECK(capture.initialize(runtime.gpu, SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM, 320, 240));
            CHECK(renderer.prepare(real, 10 + cycle, SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM, 320, 240, error));
            auto* cmd = SDL_AcquireGPUCommandBuffer(runtime.gpu);
            CHECK(cmd);
            CHECK(renderer.draw(cmd, capture.texture, real, std::span(&*it, 1), camera, rotation, false, error));
            CHECK(renderer.draw(cmd, capture.texture, real, std::span(&*it, 1), camera, rotation, false, error,
                                DrawLoad::Load, nullptr, WorldPass::Translucent));
            CHECK(readback(runtime.gpu, cmd, capture));
            auto* pixels = static_cast<const Uint8*>(SDL_MapGPUTransferBuffer(runtime.gpu, capture.transfer, false));
            CHECK(pixels);
            std::size_t changed = 0, pink = 0;
            for (std::size_t i = 0; i < 320 * 240; ++i)
            {
                const auto* p = pixels + i * 4;
                changed += p[0] != 6 || p[1] != 9 || p[2] != 13;
                pink += p[0] == 166 && p[1] == 64 && p[2] == 128;
            }
            // Independent original surface UV + decoded texel at the central pixel ray.
            // This authored pane is axis-aligned; +X camera looks along -X, right is +Z.
            CHECK(it->rotation.m_Spin == 1 && it->rotation.m_Vec.x == 0 && it->rotation.m_Vec.y == 0 &&
                  it->rotation.m_Vec.z == 0);
            const auto& source = game.world.models[it->range];
            const Surface* surface = nullptr;
            for (const auto& polygon : source.polygons)
                if ((source.surfaces[polygon.surface].flags & (4 | 8 | 16)) == 8)
                    surface = &source.surfaces[polygon.surface];
            CHECK(surface);
            const auto authored = vector_add(vector_subtract(center, it->position),
                                             vector_add(real.ranges[it->range].pivot, {0, -.4f, .4f}));
            const auto delta = vector_subtract(authored, surface->origin);
            const auto& texture = real.materials[batch.material].image;
            auto wrap = [](int value, int extent) { return (value % extent + extent) % extent; };
            const auto tx = wrap(int(std::floor(vector_dot(delta, surface->u))), texture.width);
            const auto ty = wrap(int(std::floor(vector_dot(delta, surface->v))), texture.height);
            const auto* texel = texture.rgba.data() + (ty * texture.width + tx) * 4;
            const auto* actual = pixels + (120 * 320 + 160) * 4;
            const float clear[3] = {.025f, .035f, .05f};
            bool matched = true;
            for (unsigned channel = 0; channel < 3; ++channel)
            {
                const auto expected = int(std::lround(texel[channel] * texel[3] / 255.f +
                                                      std::round(clear[channel] * 255) * (1 - texel[3] / 255.f)));
                matched &= std::abs(int(actual[channel]) - expected) <= 1;
                std::cout << "glass channel " << channel << " expected=" << expected
                          << " actual=" << int(actual[channel]) << '\n';
            }
            CHECK(matched);
            SDL_UnmapGPUTransferBuffer(runtime.gpu, capture.transfer);
            CHECK(capture.save((std::string(argv[1]) + "-private-" + std::to_string(cycle) + ".png").c_str()));
            CHECK(changed > 1000 && pink == 0 && renderer.stats.alpha_batches == 1);
            std::cout << "Private glass12: changed=" << changed << " pink=" << pink
                      << " submitted=" << renderer.stats.alpha_batches << '\n';
            cmd = SDL_AcquireGPUCommandBuffer(runtime.gpu);
            CHECK(cmd);
            CHECK(renderer.draw(cmd, capture.texture, real, all, camera, rotation, false, error));
            CHECK(renderer.draw(cmd, capture.texture, real, all, camera, rotation, false, error, DrawLoad::Load,
                                nullptr, WorldPass::Translucent));
            CHECK(readback(runtime.gpu, cmd, capture));
            CHECK(capture.save((std::string(argv[1]) + "-scene-" + std::to_string(cycle) + ".png").c_str()));
            CHECK(game.unload_world());
            renderer.clear_geometry();
        }
    }
    return 0;
}
