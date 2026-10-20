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
        DBYTE ctf_team = 0, ctf_flag = 0;
        DBYTE crosshair = 1; // Original m_nCrosshair (zoom overrides it with 98/99).
    };
    HudState hud_state(const PlayerStats&, bool in_world, bool external_camera, bool dead = false,
                       bool show_crosshair = true, DBYTE crosshair = 1, DBYTE ctf_team = 0, DBYTE ctf_flag = 0);
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
        // The crosshair slot fits the widest original zoom cursor (crosshair98, 97x41).
        static constexpr unsigned atlas_width = 1024, atlas_height = 128, crosshair_x = 656, team_x = 768, flag_x = 896;
        const HudSurface* crosshair_ = nullptr;
        DBYTE crosshair_number_ = 0;
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
