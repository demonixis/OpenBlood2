#pragma once

#include <SDL3/SDL.h>
#include <string>

namespace blood2 {

// SDL and GPU lifetime only. The original object runtime belongs to M1.
struct Runtime {
    SDL_Window* window = nullptr;
    SDL_GPUDevice* gpu = nullptr;
    bool sdl_initialized = false;
    bool window_claimed = false;

    Runtime() = default;
    Runtime(const Runtime&) = delete;
    Runtime& operator=(const Runtime&) = delete;
    ~Runtime();
    bool initialize(bool gpu_debug, std::string& error);
};

}
