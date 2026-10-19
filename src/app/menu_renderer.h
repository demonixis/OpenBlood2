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
        bool draw_surface(const HudSurface&, std::uint64_t revision, std::string& error);

      private:
        SDL_GPUDevice* device_;
        SDL_GPUTexture* texture_ = nullptr;
        SDL_GPUTransferBuffer* transfer_ = nullptr;
        std::uint64_t uploaded_ = UINT64_MAX;
    };
} // namespace OpenBlood2
