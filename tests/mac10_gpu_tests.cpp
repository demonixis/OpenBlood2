#include "app/model_scene.h"
#include "engine/capture.h"
#include "engine/runtime.h"
#include "engine/world_renderer.h"
#include "mac10_fixture.h"
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
        CHECK(test_mac10::equip(g) && test_mac10::fire_modes(g));
        CHECK(update_model_scene(g, scene, error) && scene.close_draws.size() == 2);
        unsigned close = 0;
        for (const auto& e : scene.entries)
            if (e.close)
            {
                CHECK(e.visible && e.triangles > 0 && e.error.empty() && e.skin.find("Mac10") != std::string::npos);
                ++close;
            }
        CHECK(close == 2);
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
        std::cout << "Mac10 GPU cycle=" << cycle << " left pixels=" << left << " right pixels=" << right << '\n';
        CHECK(left > 100 && right > 100);
        CHECK(capture.save((std::string(argv[2]) + std::to_string(cycle) + ".png").c_str()));
        CHECK(g.unload_world() && update_model_scene(g, scene, error) && scene.close_draws.empty() &&
              scene.entries.empty());
        renderer.clear_geometry();
        scene.clear();
        CHECK(g.server.audio.voices().empty() && g.objects.attachments().empty());
    }
}
