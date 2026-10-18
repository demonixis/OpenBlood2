#include <SDL3/SDL.h>
#include <iostream>
#include <string_view>

namespace
{
    bool cycle(std::string_view mode)
    {
        if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD))
        {
            std::cerr << SDL_GetError() << '\n';
            SDL_Quit();
            return false;
        }
        std::cout << "Video driver: " << SDL_GetCurrentVideoDriver() << '\n';
        SDL_Window* window = nullptr;
        SDL_GPUDevice* gpu = nullptr;
        bool claimed = false, ok = true;
        if (mode != "video")
        {
            window = SDL_CreateWindow("SDL lifetime baseline", 640, 420, SDL_WINDOW_RESIZABLE);
            ok = window != nullptr;
        }
        if (ok && mode == "gpu")
        {
            gpu = SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_SPIRV | SDL_GPU_SHADERFORMAT_MSL, true, nullptr);
            ok = gpu != nullptr;
            if (ok)
            {
                std::cout << "GPU device: "
                          << SDL_GetStringProperty(SDL_GetGPUDeviceProperties(gpu), SDL_PROP_GPU_DEVICE_NAME_STRING,
                                                   "unreported")
                          << '\n';
                claimed = SDL_ClaimWindowForGPUDevice(gpu, window);
                ok = claimed;
            }
        }
        if (ok && gpu)
        {
            auto* command = SDL_AcquireGPUCommandBuffer(gpu);
            ok = command && SDL_SubmitGPUCommandBuffer(command) && SDL_WaitForGPUIdle(gpu);
        }
        SDL_Event event{};
        while (SDL_PollEvent(&event))
        {
        }
        if (!ok)
            std::cerr << SDL_GetError() << '\n';
        if (gpu)
        {
            ok = SDL_WaitForGPUIdle(gpu) && ok;
            if (claimed)
                SDL_ReleaseWindowFromGPUDevice(gpu, window);
            SDL_DestroyGPUDevice(gpu);
        }
        if (window)
            SDL_DestroyWindow(window);
        SDL_Quit();
        const auto allocations = SDL_GetNumAllocations();
        std::cout << "SDL teardown complete; allocation tracking: ";
        if (allocations < 0)
            std::cout << "unavailable\n";
        else
            std::cout << allocations << '\n';
        return ok;
    }
} // namespace

int main(int argc, char** argv)
{
    const std::string_view mode = argc > 1 ? argv[1] : "";
    if (argc != 3 || (mode != "video" && mode != "window" && mode != "gpu") ||
        (std::string_view(argv[2]) != "1" && std::string_view(argv[2]) != "3"))
        return 2;
    // Deliberately links only SDL: no engine, game, assets, renderer or ImGui.
    // No suppression, custom leak root or process-global D-Bus shutdown here.
    const unsigned cycles = argv[2][0] - '0';
    for (unsigned i = 0; i < cycles; ++i)
        if (!cycle(mode))
            return 1;
    return 0;
}
