#pragma once
#include "game/original/NewStatusBar.h"
#include "game/player_messages.h"
#include <SDL3/SDL.h>
namespace OpenBlood2
{
    // Presentation values only, read from the checked typed client snapshot.
    struct HudState
    {
        PlayerStats stats;
        bool visible = false, crosshair_visible = false;
    };
    HudState hud_state(const PlayerStats&, bool in_world, bool external_camera, bool dead = false);
    class HudRenderer
    {
      public:
        explicit HudRenderer(SDL_GPUDevice* device) : device_(device) {}
        ~HudRenderer();
        HudRenderer(const HudRenderer&) = delete;
        HudRenderer& operator=(const HudRenderer&) = delete;
        bool draw(const HudState&, const Resources&, std::string& error);
        const CNewStatusBar* status_bar() const
        {
            return status_.get();
        }

      private:
        static constexpr unsigned atlas_width = 720, atlas_height = 75, crosshair_x = 656;
        const HudSurface* crosshair_ = nullptr;
        SDL_GPUDevice* device_ = nullptr;
        SDL_GPUTexture* texture_ = nullptr;
        SDL_GPUTransferBuffer* transfer_ = nullptr;
        std::unique_ptr<CNewStatusBar> status_;
        const Resources* resources_ = nullptr;
        std::uint64_t uploaded_revision_ = 0;
        bool upload(std::string& error);
    };
    bool draw_screen_fade(float scale, bool in_world);
} // namespace OpenBlood2
