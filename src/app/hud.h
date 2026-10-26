#pragma once
#include "app/touch_controls.h"
#include "engine/string_resources.h"
#include "game/original/Credits.h"
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
        // Latest SMSG_ITEMCHANGED values, the message and show counters, and the client clock.
        InventoryItems items;
        std::uint64_t item_messages = 0, item_shows = 0;
        float time = 0;
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
        // The second row holds the three item tabs, the current item tab and the item name.
        static constexpr unsigned atlas_width = 1024, atlas_height = 192, crosshair_x = 656, team_x = 768, flag_x = 896,
                                  items_y = 128;
        const HudSurface* crosshair_ = nullptr;
        DBYTE crosshair_number_ = 0;
        SDL_GPUDevice* device_ = nullptr;
        SDL_GPUTexture* texture_ = nullptr;
        SDL_GPUTransferBuffer* transfer_ = nullptr;
        std::unique_ptr<CNewStatusBar> status_;
        const Resources* resources_ = nullptr;
        std::uint64_t uploaded_revision_ = 0;
        std::uint64_t item_messages_ = UINT64_MAX, item_shows_ = 0;
        // cres.dll IDS_ITEM_* names, read once on first need; failure leaves names empty.
        StringResources item_names_;
        bool item_names_read_ = false;
        bool upload(std::string& error);
    };
    bool draw_screen_fade(float scale, bool in_world);
    // Black rows over a letterbox camera's top and bottom sixth (the scene clears to the fog colour).
    bool draw_letterbox(bool letterbox);
    // The last drawn CCredit, centered in the 640x480 menu viewport. Replacement font:
    // Dear ImGui's embedded default instead of the original GDI Arial request.
    bool draw_credits(const CCredits&);
    // The on-screen touch controls: outlined circles, filled while held, and the floating stick.
    void draw_touch_controls(const TouchControls&);
} // namespace OpenBlood2
