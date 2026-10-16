#pragma once
#include <SDL3/SDL.h>

namespace OpenBlood2
{

    // Optional one-frame readback for visual verification. Normal frames render
    // directly to the swapchain; SDL swapchain textures themselves are write-only.
    struct Capture
    {
        SDL_GPUDevice* device = nullptr;
        SDL_GPUTexture* texture = nullptr;
        SDL_GPUTransferBuffer* transfer = nullptr;
        SDL_PixelFormat pixel_format = SDL_PIXELFORMAT_UNKNOWN;
        Uint32 width = 0, height = 0;

        Capture() = default;
        Capture(const Capture&) = delete;
        Capture& operator=(const Capture&) = delete;
        ~Capture();
        bool initialize(SDL_GPUDevice* gpu, SDL_GPUTextureFormat format, Uint32 w, Uint32 h);
        bool enqueue(SDL_GPUCommandBuffer* commands, SDL_GPUTexture* swapchain, Uint32 target_width,
                     Uint32 target_height);
        bool save(const char* filename);
    };

} // namespace OpenBlood2
