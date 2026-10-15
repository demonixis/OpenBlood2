#include "engine/runtime.h"

namespace OpenBlood2
{

    bool Runtime::initialize(bool gpu_debug, std::string& error)
    {
        SDL_SetAppMetadata("Blood II Reconstruction", "0.1.0", "org.blood2.reconstruction");
        if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD))
        {
            error = SDL_GetError();
            return false;
        }
        sdl_initialized = true;
        window = SDL_CreateWindow("Blood II - M0 Inspector", 1200, 780,
                                  SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY);
        if (!window)
        {
            error = SDL_GetError();
            return false;
        }
        gpu = SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_SPIRV | SDL_GPU_SHADERFORMAT_DXIL | SDL_GPU_SHADERFORMAT_MSL |
                                      SDL_GPU_SHADERFORMAT_METALLIB,
                                  gpu_debug, nullptr);
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

    Runtime::~Runtime()
    {
        if (gpu)
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
