#include "app/hud.h"
#include "app/ui_runtime.h"
#include "engine/capture.h"
#include "game/original/ScreenFade.h"
#include "game/original/WeaponDefs.h"
#include <cmath>
#include <filesystem>
#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_sdlgpu3.h>
#include <iostream>
using namespace OpenBlood2;
#define CHECK(x)                                                                                                       \
    do                                                                                                                 \
    {                                                                                                                  \
        if (!(x))                                                                                                      \
        {                                                                                                              \
            std::cerr << __LINE__ << ": " #x "\n";                                                                     \
            return false;                                                                                              \
        }                                                                                                              \
    } while (false)
namespace
{
    bool run(const std::filesystem::path& root, const std::filesystem::path& content)
    {
        Runtime runtime;
        std::string error;
        Config config;
        config.data_dir = content;
        Resources artwork;
        CHECK(artwork.mount_game(config, error));
        CHECK(runtime.initialize(true, error));
        std::cout << "GPU device: "
                  << SDL_GetStringProperty(SDL_GetGPUDeviceProperties(runtime.gpu), SDL_PROP_GPU_DEVICE_NAME_STRING,
                                           "unreported")
                  << '\n';
        std::filesystem::create_directories(root);
        for (int cycle = 0; cycle < 2; ++cycle)
        {
            UiRuntime ui(runtime);
            CHECK(ui.initialize(error));
            HudRenderer renderer(runtime.gpu);
            const Uint32 width = cycle ? 640 : 1000, height = cycle ? 400 : 680;
            Capture capture;
            CHECK(capture.initialize(runtime.gpu, SDL_GetGPUSwapchainTextureFormat(runtime.gpu, runtime.window), width,
                                     height));
            CScreenFade fade;
            for (int frame = 0; frame < 4; ++frame)
            {
                if (frame == 1)
                {
                    fade.Begin(false);
                    for (int i = 0; i < 12; ++i)
                        CHECK(fade.Update(.1));
                    CHECK(fade.Update(.075));
                }
                if (frame == 2)
                    for (int i = 0; i < 13; ++i)
                        CHECK(fade.Update(.1));
                if (frame == 3)
                    fade = {}; // New world resets fade before its first presentation.
                const auto value = fade.value;
                const auto messages = fade.messages;
                ImGui_ImplSDLGPU3_NewFrame();
                ImGui_ImplSDL3_NewFrame();
                ImGui::GetIO().DisplaySize = {float(width), float(height)};
                ImGui::GetIO().DisplayFramebufferScale = {1, 1};
                ImGui::NewFrame();
                CHECK(draw_screen_fade(fade.Scale(), true) == (frame == 1 || frame == 2));
                // UI is intentionally above the scene multiplication.
                PlayerStats stats;
                stats.health = 100;
                stats.armor = 20;
                stats.weapon = WEAP_BERETTA;
                CHECK(renderer.draw(hud_state(stats, true, false), artwork, error));
                ImGui::SetNextWindowPos({20, 20});
                ImGui::SetNextWindowSize({230, 70});
                ImGui::Begin("Fade inspection", nullptr, ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoInputs);
                ImGui::Text("Scene scale %.3f", fade.Scale());
                ImGui::End();
                ImGui::Render();
                CHECK(fade.value == value && fade.messages == messages);
                auto* commands = SDL_AcquireGPUCommandBuffer(runtime.gpu);
                CHECK(commands);
                ImGui_ImplSDLGPU3_PrepareDrawData(ImGui::GetDrawData(), commands);
                SDL_GPUColorTargetInfo color{};
                color.texture = capture.texture;
                color.clear_color = {.8f, .8f, .8f, 1};
                color.load_op = SDL_GPU_LOADOP_CLEAR;
                color.store_op = SDL_GPU_STOREOP_STORE;
                auto* pass = SDL_BeginGPURenderPass(commands, &color, 1, nullptr);
                CHECK(pass);
                ImGui_ImplSDLGPU3_RenderDrawData(ImGui::GetDrawData(), commands, pass);
                SDL_EndGPURenderPass(pass);
                auto* copy = SDL_BeginGPUCopyPass(commands);
                CHECK(copy);
                SDL_GPUTextureRegion region{};
                region.texture = capture.texture;
                region.w = width;
                region.h = height;
                region.d = 1;
                SDL_GPUTextureTransferInfo target{};
                target.transfer_buffer = capture.transfer;
                target.pixels_per_row = width;
                target.rows_per_layer = height;
                SDL_DownloadFromGPUTexture(copy, &region, &target);
                SDL_EndGPUCopyPass(copy);
                CHECK(SDL_SubmitGPUCommandBuffer(commands) && SDL_WaitForGPUIdle(runtime.gpu));
                auto* data =
                    static_cast<const std::uint8_t*>(SDL_MapGPUTransferBuffer(runtime.gpu, capture.transfer, false));
                CHECK(data);
                const auto center = (height / 2 * width + width / 2) * 4;
                const int expected = int(std::lround(204 * fade.Scale()));
                for (int channel = 0; channel < 3; ++channel)
                    CHECK(std::abs(int(data[center + channel]) - expected) <= 2);
                unsigned hud_lit = 0, inspector_lit = 0;
                for (Uint32 y = 0; y < height; ++y)
                    for (Uint32 x = 0; x < width; ++x)
                    {
                        const auto i = (y * width + x) * 4;
                        if (data[i] > 30 || data[i + 1] > 30 || data[i + 2] > 30)
                        {
                            if (y > height - 90)
                                ++hud_lit;
                            if (y < 90 && x < 250)
                                ++inspector_lit;
                        }
                    }
                SDL_UnmapGPUTransferBuffer(runtime.gpu, capture.transfer);
                CHECK(hud_lit > 1000 && inspector_lit > 1000); // Original bitmap HUD survives a black scene.
                CHECK(capture.save(
                    (root / ("fade-" + std::to_string(cycle) + "-" + std::to_string(frame) + ".png")).c_str()));
                std::cout << "cycle=" << cycle << " scale=" << fade.Scale() << " expected RGB=" << expected
                          << " HUD=" << hud_lit << " inspector=" << inspector_lit << '\n';
            }
        }
        return true;
    }
} // namespace
int main(int argc, char** argv)
{
    return argc == 3 && run(argv[1], argv[2]) ? 0 : 1;
}
