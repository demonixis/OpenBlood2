#include "engine/capture.h"
#include "engine/runtime.h"
#include "engine/world_renderer.h"
#include "game/server_adapter.h"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>
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
    WorldMesh quad()
    {
        WorldMesh m;
        for (DVector p : {DVector{-5, 5, 5}, {5, 5, 5}, {5, -5, 5}, {-5, 5, 5}, {5, -5, 5}, {-5, -5, 5}})
            m.vertices.push_back({p, {.2f, .2f, .2f}, {0, 0}});
        m.ranges.push_back({0, 6, {}, "dark wall", {{0, 6, 0, false, {0, 0, 5}}}});
        return m;
    }
} // namespace
int main(int argc, char** argv)
{
    if (argc != 2 && argc != 3)
        return 2;
    std::string error;
    Runtime runtime;
    CHECK(runtime.initialize(true, error));
    std::cout << "GPU="
              << SDL_GetStringProperty(SDL_GetGPUDeviceProperties(runtime.gpu), SDL_PROP_GPU_DEVICE_NAME_STRING,
                                       "unknown")
              << " backend=" << SDL_GetGPUDeviceDriver(runtime.gpu) << '\n';
    WorldRenderer renderer(runtime.gpu), sprite(runtime.gpu, DrawStyle::SpriteAlpha);
    auto m = quad();
    WorldDraw draw;
    std::vector<Uint8> pixels;
    for (unsigned cycle = 0; cycle < 3; ++cycle)
    {
        unsigned w = cycle == 1 ? 400 : 320, h = cycle == 1 ? 300 : 240;
        Capture capture;
        CHECK(capture.initialize(runtime.gpu, SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM, w, h));
        CHECK(renderer.prepare(m, cycle, SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM, w, h, error));
        CHECK(sprite.prepare(m, cycle, SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM, w, h, error));
        DVector eye{};
        DRotation view;
        draw = {};
        // Radial light adds to the face light before the albedo multiply: .2 + .3 at the center.
        PointLight light{{0, 0, 5}, {.3f, .3f, .3f}, 2};
        if (cycle == 2)
        {
            eye = {100, 20, -50};
            view = rotation_from_euler({0, 1.57079632679f, 0});
            draw.position = eye;
            draw.rotation = view;
            light.position = vector_add(eye, rotate_vector(view, {0, 0, 5}));
        }
        for (unsigned mode = 0; mode < 6; ++mode)
        {
            auto use = light;
            if (mode == 0)
                use.radius = 0;
            if (mode == 2)
                use.position.x += 100;
            auto* cmd = SDL_AcquireGPUCommandBuffer(runtime.gpu);
            CHECK(cmd);
            auto& r = mode == 4 ? sprite : renderer;
            CHECK(r.draw(cmd, capture.texture, m, std::span(&draw, 1), eye, view, false, error,
                         mode == 3 ? DrawLoad::ClearDepth : DrawLoad::Clear, nullptr,
                         mode == 5 ? WorldPass::Sky : WorldPass::Opaque, 0, use));
            CHECK(download(runtime.gpu, cmd, capture, pixels));
            const int center = pixels[(h / 2 * w + w / 2) * 4], edge = pixels[(h / 2 * w + w * 9 / 10) * 4];
            CHECK(std::abs(center - (mode == 1 ? 127 : 51)) <= 3 && std::abs(edge - 51) <= 1 &&
                  pixels[(h / 2 * w + w / 2) * 4 + 3] == 255);
            std::cout << "Synthetic light cycle=" << cycle << " mode=" << mode << " center=" << center
                      << " outside=" << edge << '\n';
            if (mode == 1)
                CHECK(capture.save((std::string(argv[1]) + "-" + std::to_string(cycle) + ".png").c_str()));
        }
        auto* cmd = SDL_AcquireGPUCommandBuffer(runtime.gpu);
        CHECK(cmd);
        light.radius = std::numeric_limits<float>::quiet_NaN();
        CHECK(!renderer.draw(cmd, capture.texture, m, std::span(&draw, 1), eye, view, false, error, DrawLoad::Clear,
                             nullptr, WorldPass::Opaque, 0, light));
        CHECK(SDL_CancelGPUCommandBuffer(cmd));
        renderer.clear_geometry();
        sprite.clear_geometry();
    }
    if (argc == 3)
    {
        GameSession g;
        CHECK(g.initialize());
        Config cfg;
        cfg.data_dir = argv[2];
        CHECK(g.resources.mount_game(cfg, error));
        for (unsigned cycle = 0; cycle < 2; ++cycle)
        {
            CHECK(g.request_world("01_subwaya", error) && g.enter_player(error));
            auto* p = g.server_shell.Player(g.local_client);
            CHECK(p && p->m_InventoryMgr.AddItem(2, 1) == InventoryResult::Ok);
            auto& f = *p->m_InventoryMgr.flashlight;
            CHECK(f.ActivateItem());
            p->m_fPitch = 1.57079632679f;
            p->m_fYaw = 0;
            CHECK(f.Update() && g.client_shell.flashlight.Update(g.server, f.light, p->m_hObject));
            auto light = g.client_shell.flashlight.Current(g.server);
            CHECK(light.radius > 0);
            WorldMesh mesh;
            CHECK(mesh.build(g.world, error, &g.resources));
            std::vector<WorldDraw> draws;
            WorldViewStats stats;
            CHECK(world_draws(mesh, g.objects, draws, stats, error));
            auto eye = g.objects.get(f.light)->state.m_Pos;
            auto view = g.objects.get(f.light)->state.m_Rotation;
            Capture capture;
            CHECK(capture.initialize(runtime.gpu, SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM, 320, 240));
            CHECK(renderer.prepare(mesh, 10 + cycle, SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM, 320, 240, error));
            std::vector<Uint8> off;
            for (unsigned on = 0; on < 2; ++on)
            {
                auto* cmd = SDL_AcquireGPUCommandBuffer(runtime.gpu);
                CHECK(cmd);
                CHECK(renderer.draw(cmd, capture.texture, mesh, draws, eye, view, false, error, DrawLoad::Clear,
                                    nullptr, WorldPass::Opaque, 0, on ? light : PointLight{}));
                CHECK(download(runtime.gpu, cmd, capture, pixels));
                CHECK(capture.save(
                    (std::string(argv[1]) + "-private-" + std::to_string(cycle) + "-" + std::to_string(on) + ".png")
                        .c_str()));
                if (!on)
                    off = pixels;
            }
            unsigned brighter = 0, darker = 0;
            for (unsigned i = 0; i < 320 * 240; ++i)
                for (unsigned c = 0; c < 3; ++c)
                {
                    brighter += pixels[i * 4 + c] > off[i * 4 + c];
                    darker += pixels[i * 4 + c] < off[i * 4 + c];
                }
            std::cout << "Private original ray cycle=" << cycle << " distance=" << g.client_shell.flashlight.distance
                      << " radius=" << light.radius << " brighter channels=" << brighter << " darker=" << darker
                      << '\n';
            CHECK(brighter > 1000 && !darker);
            auto old = f.light;
            CHECK(g.leave_player() && !g.server.IsObjectAlive(old) &&
                  g.client_shell.flashlight.Current(g.server).radius == 0 && g.unload_world());
            renderer.clear_geometry();
        }
    }
    return 0;
}
