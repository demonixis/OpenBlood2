#pragma once
#include "game/menu.h"
#include <SDL3/SDL.h>
namespace OpenBlood2
{
    class MenuRenderer
    {
      public:
        explicit MenuRenderer(SDL_GPUDevice* device) : device_(device) {}
        ~MenuRenderer();
        MenuRenderer(const MenuRenderer&) = delete;
        MenuRenderer& operator=(const MenuRenderer&) = delete;
        bool draw(MenuCanvas&, std::string& error);

      private:
        SDL_GPUDevice* device_;
        SDL_GPUTexture* texture_ = nullptr;
        SDL_GPUTransferBuffer* transfer_ = nullptr;
        std::uint64_t uploaded_ = 0;
    };
} // namespace OpenBlood2
