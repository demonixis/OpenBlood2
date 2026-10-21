#pragma once

#include <SDL3/SDL.h>
#include <string>

namespace OpenBlood2
{

    // SDL and GPU lifetime only. The original object runtime belongs to M1.
    struct Runtime
    {
        SDL_Window* window = nullptr;
        SDL_GPUDevice* gpu = nullptr;
        bool sdl_initialized = false;
        bool window_claimed = false;
        bool owns_gpu = true;

        Runtime() = default;
        Runtime(const Runtime&) = delete;
        Runtime& operator=(const Runtime&) = delete;
        ~Runtime();
        bool initialize(bool gpu_debug, std::string& error);
        // SDL and the window only; the caller supplies a device it owns (the OpenXR session's)
        // with set_device(), and nothing is presented to the window.
        bool initialize_window(std::string& error);
        void set_device(SDL_GPUDevice* device)
        {
            gpu = device;
            owns_gpu = false;
        }
    };

} // namespace OpenBlood2
