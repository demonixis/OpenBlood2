#include "engine/runtime.h"
#include "build_info.h"
#include "engine/world_shaders.h"

namespace OpenBlood2
{

    bool Runtime::initialize(bool gpu_debug, std::string& error)
    {
        SDL_SetAppMetadata("OpenBlood 2", build::version, build::game_identifier);
        if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD))
        {
            error = SDL_GetError();
            return false;
        }
        sdl_initialized = true;
        window = SDL_CreateWindow("OpenBlood 2", 1200, 780, SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY);
        if (!window)
        {
            error = SDL_GetError();
            return false;
        }
        gpu = SDL_CreateGPUDevice(WorldShaderFormats, gpu_debug, nullptr);
        if (!gpu || !SDL_ClaimWindowForGPUDevice(gpu, window))
        {
            error = SDL_GetError();
            return false;
        }
        window_claimed = true;
        if (!SDL_SetGPUSwapchainParameters(gpu, window, SDL_GPU_SWAPCHAINCOMPOSITION_SDR, SDL_GPU_PRESENTMODE_VSYNC))
        {
            error = SDL_GetError();
            return false;
        }
        return true;
    }

    bool Runtime::initialize_window(std::string& error)
    {
        SDL_SetAppMetadata("OpenBlood 2", build::version, build::game_identifier);
        if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD))
        {
            error = SDL_GetError();
            return false;
        }
        sdl_initialized = true;
        window = SDL_CreateWindow("OpenBlood 2", 1200, 780, SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY);
        if (!window)
        {
            error = SDL_GetError();
            return false;
        }
        return true;
    }

    Runtime::~Runtime()
    {
        if (gpu && owns_gpu)
        {
            SDL_WaitForGPUIdle(gpu);
            if (window_claimed)
                SDL_ReleaseWindowFromGPUDevice(gpu, window);
            SDL_DestroyGPUDevice(gpu);
        }
        if (window)
            SDL_DestroyWindow(window);
        if (sdl_initialized)
            SDL_Quit();
    }

} // namespace OpenBlood2
