#include "app/display_options.h"
#include <iostream>
using namespace OpenBlood2;
#define CHECK(x)                                                                                                       \
    do                                                                                                                 \
    {                                                                                                                  \
        if (!(x))                                                                                                      \
        {                                                                                                              \
            std::cerr << __LINE__ << ": " #x " " << error << '\n';                                                     \
            return 1;                                                                                                  \
        }                                                                                                              \
    } while (false)
int main()
{
    std::string error;
    Runtime runtime;
    CHECK(runtime.initialize(true, error));
    DisplayOptions display(runtime);
    Menu menu;
    CHECK(display.apply(menu, error) && menu.video.window);
    CHECK(menu.video.resolutions.size() > 1);
    const auto present = [&]
    {
        SDL_PumpEvents();
        auto* commands = SDL_AcquireGPUCommandBuffer(runtime.gpu);
        if (!commands)
            return false;
        SDL_GPUTexture* target = nullptr;
        Uint32 w = 0, h = 0;
        if (!SDL_WaitAndAcquireGPUSwapchainTexture(commands, runtime.window, &target, &w, &h))
        {
            SDL_CancelGPUCommandBuffer(commands);
            return false;
        }
        if (target)
        {
            SDL_GPUColorTargetInfo color{};
            color.texture = target;
            color.clear_color = {.05f, .05f, .05f, 1};
            color.load_op = SDL_GPU_LOADOP_CLEAR;
            color.store_op = SDL_GPU_STOREOP_STORE;
            auto* pass = SDL_BeginGPURenderPass(commands, &color, 1, nullptr);
            if (!pass)
            {
                SDL_CancelGPUCommandBuffer(commands);
                return false;
            }
            SDL_EndGPURenderPass(pass);
        }
        return SDL_SubmitGPUCommandBuffer(commands);
    };
    CHECK(present());
    // Pixel dimensions survive high-density window coordinates.
    menu.settings.resolution_width = 1280;
    menu.settings.resolution_height = 720;
    CHECK(display.apply(menu, error));
    int width = 0, height = 0;
    CHECK(SDL_GetWindowSizeInPixels(runtime.window, &width, &height) && width == 1280 && height == 720);
    CHECK(present());
    if (menu.video.vsync_off)
    {
        menu.settings.vsync = false;
        CHECK(display.apply(menu, error) && present());
        menu.settings.vsync = true;
        CHECK(display.apply(menu, error) && present());
    }
    menu.settings.fullscreen = true;
    CHECK(display.apply(menu, error) && (SDL_GetWindowFlags(runtime.window) & SDL_WINDOW_FULLSCREEN));
    CHECK(present());
    menu.settings.fullscreen_mode = 1;
    menu.settings.resolution_width = menu.settings.resolution_height = 0;
    CHECK(display.apply(menu, error) && SDL_GetWindowFullscreenMode(runtime.window));
    CHECK(present());
    // Unsupported modes fail before publication; preserve both menu and actual mode.
    const auto previous = menu.settings;
    menu.settings.resolution_width = 777;
    menu.settings.resolution_height = 555;
    CHECK(!display.apply(menu, error) && !error.empty() && menu.settings == previous);
    CHECK(SDL_GetWindowFlags(runtime.window) & SDL_WINDOW_FULLSCREEN);
    CHECK(present());
    menu.settings = {};
    CHECK(display.apply(menu, error) && !(SDL_GetWindowFlags(runtime.window) & SDL_WINDOW_FULLSCREEN));
    CHECK(present());
    // Ceiling pacing waits for the remaining budget; a late frame never waits to catch up.
    const auto start = SDL_GetTicksNS();
    DisplayOptions::pace(start, 30);
    CHECK(SDL_GetTicksNS() - start >= 1000000000ULL / 30);
    DisplayOptions::pace(SDL_GetTicksNS() - 100000000, 240);
    DisplayOptions::pace(SDL_GetTicksNS(), 0);
    std::cout << "Display: pixel resolution, borderless/exclusive/windowed, supported VSync, rollback, pacing passed\n";
}
