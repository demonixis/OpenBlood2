#include "app/model_scene.h"
#include "engine/capture.h"
#include "engine/runtime.h"
#include "engine/world_renderer.h"
#include "remote_fixture.h"
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
    if (argc != 3)
        return 2;
    std::string error;
    Runtime runtime;
    CHECK(runtime.initialize(true, error));
    GameSession g;
    CHECK(g.initialize());
    Config cfg;
    cfg.data_dir = argv[1];
    CHECK(g.resources.mount_game(cfg, error));
    WorldRenderer renderer(runtime.gpu);
    ModelView scene;
    for (unsigned cycle = 0; cycle < 2; ++cycle)
    {
        CHECK(g.request_world("01_subwaya", error) && g.enter_player(error) && test_camera::complete_subway_intro(g));
        CHECK(test_remote::equip(g));
        for (unsigned i = 0; i < 100; ++i)
            CHECK(g.step(.01));
        CHECK(update_model_scene(g, scene, error) && scene.close_draws.size() == 1);
        unsigned close = 0;
        for (const auto& e : scene.entries)
            if (e.close)
            {
                CHECK(e.visible && e.triangles > 0 && e.error.empty() && e.skin.find("C4remote") != std::string::npos);
                ++close;
            }
        CHECK(close == 1);
        Capture capture;
        CHECK(capture.initialize(runtime.gpu, SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM, 640, 480));
        CHECK(
            renderer.prepare(scene.mesh, scene.revision, SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM, 640, 480, error, true));
        const auto& view = g.client_shell.view;
        auto* commands = SDL_AcquireGPUCommandBuffer(runtime.gpu);
        CHECK(commands && renderer.draw(commands, capture.texture, scene.mesh, scene.close_draws, view.position,
                                        view.rotation, false, error));
        auto* copy = SDL_BeginGPUCopyPass(commands);
        CHECK(copy);
        SDL_GPUTextureRegion region{};
        region.texture = capture.texture;
        region.w = 640;
        region.h = 480;
        region.d = 1;
        SDL_GPUTextureTransferInfo target{};
        target.transfer_buffer = capture.transfer;
        target.pixels_per_row = 640;
        target.rows_per_layer = 480;
        SDL_DownloadFromGPUTexture(copy, &region, &target);
        SDL_EndGPUCopyPass(copy);
        CHECK(SDL_SubmitGPUCommandBuffer(commands) && SDL_WaitForGPUIdle(runtime.gpu));
        auto* pixels = static_cast<const Uint8*>(SDL_MapGPUTransferBuffer(runtime.gpu, capture.transfer, false));
        CHECK(pixels);
        unsigned left = 0, right = 0;
        for (unsigned y = 0; y < 480; ++y)
            for (unsigned x = 0; x < 640; ++x)
            {
                const auto* p = pixels + (y * 640 + x) * 4;
                if (p[0] != pixels[0] || p[1] != pixels[1] || p[2] != pixels[2])
                    (x < 320 ? left : right)++;
            }
        SDL_UnmapGPUTransferBuffer(runtime.gpu, capture.transfer);
        std::cout << "Remote view GPU cycle=" << cycle << " left pixels=" << left << " right pixels=" << right << '\n';
        CHECK(left + right > 100);
        CHECK(capture.save((std::string(argv[2]) + std::to_string(cycle) + ".png").c_str()));
        // Render the actual independent grenade model, with the rest hidden.
        const auto camera_position = view.position;
        const auto camera_rotation = view.rotation;
        auto* grenade =
            test_remote::bomb(g, vector_add(camera_position, rotate_vector(camera_rotation, {0, 0, 25})), {0, 0, 1}, 0);
        CHECK(grenade);
        const auto handle = grenade->m_hObject;
        for (auto id : g.objects.snapshot())
        {
            auto* record = g.objects.get(id);
            if (record->handle != handle)
                g.server.SetObjectFlags(record->handle, record->state.m_Flags & ~FLAG_VISIBLE);
        }
        const auto clock = g.objects.time();
        CHECK(update_model_scene(g, scene, error) && scene.world_draws.size() == 1);
        CHECK(
            renderer.prepare(scene.mesh, scene.revision, SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM, 640, 480, error, true));
        commands = SDL_AcquireGPUCommandBuffer(runtime.gpu);
        CHECK(commands && renderer.draw(commands, capture.texture, scene.mesh, scene.world_draws, camera_position,
                                        camera_rotation, false, error));
        copy = SDL_BeginGPUCopyPass(commands);
        CHECK(copy);
        SDL_DownloadFromGPUTexture(copy, &region, &target);
        SDL_EndGPUCopyPass(copy);
        CHECK(SDL_SubmitGPUCommandBuffer(commands) && SDL_WaitForGPUIdle(runtime.gpu));
        pixels = static_cast<const Uint8*>(SDL_MapGPUTransferBuffer(runtime.gpu, capture.transfer, false));
        CHECK(pixels);
        unsigned grenade_pixels = 0;
        for (unsigned i = 1; i < 640 * 480; ++i)
            if (pixels[i * 4] != pixels[0] || pixels[i * 4 + 1] != pixels[1] || pixels[i * 4 + 2] != pixels[2])
                ++grenade_pixels;
        SDL_UnmapGPUTransferBuffer(runtime.gpu, capture.transfer);
        CHECK(grenade_pixels > 5 && g.objects.time() == clock && grenade->flight_steps == 0);
        std::cout << "Remote world GPU cycle=" << cycle << " pixels=" << grenade_pixels << '\n';
        CHECK(capture.save((std::string(argv[2]) + std::to_string(cycle) + "-bomb.png").c_str()));
        g.server.SetObjectFlags(handle, g.server.GetObjectFlags(handle) & ~FLAG_VISIBLE);
        CHECK(update_model_scene(g, scene, error) && scene.world_draws.empty());
        CHECK(g.objects.remove(handle) && g.objects.finish_frame() && !g.objects.get(handle));
        CHECK(g.unload_world() && update_model_scene(g, scene, error) && scene.close_draws.empty() &&
              scene.entries.empty());
        renderer.clear_geometry();
        scene.clear();
        CHECK(g.server.audio.voices().empty() && g.objects.attachments().empty());
    }
}
