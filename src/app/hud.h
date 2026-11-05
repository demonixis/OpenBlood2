#pragma once
#include "app/touch_controls.h"
#include "engine/string_resources.h"
#include "game/original/Credits.h"
#include "game/original/MessageMgr.h"
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
        // A new world hides the weapon tabs, objectives and power bar (original CNewStatusBar::Reset).
        std::uint64_t world_revision = 0;
        // Original UpdateObjectives calls (SMSG_OBJECTIVES, LoadClientData): the shell's update
        // counter with its current title and text; `objectives_valid` false is the null reset.
        std::uint64_t objective_updates = 0;
        bool objectives_valid = false;
        std::string objectives_title, objectives_text;
        // Original SetPowerBarLevel/TurnOffPowerBar calls: the latest request and its time.
        std::uint64_t power_requests = 0;
        DBYTE power_bar = 0;
        float power_percent = 0, power_time = 0;
    };
    HudState hud_state(const PlayerStats&, bool in_world, bool external_camera, bool dead = false,
                       bool show_crosshair = true, DBYTE crosshair = 1, DBYTE ctf_team = 0, DBYTE ctf_flag = 0);
    // The client shell's objectives and power bar requests (CBloodClientShell).
    template <class Shell> void hud_panels(HudState& h, const Shell& client)
    {
        h.objective_updates = client.objective_updates;
        h.objectives_valid = client.objectives_resolved;
        h.objectives_title = client.objectives.title;
        h.objectives_text = client.objectives.text;
        h.power_requests = client.power_bar.requests;
        h.power_bar = client.power_bar.bar;
        h.power_percent = client.power_bar.percent;
        h.power_time = client.power_bar.time;
    }
    class HudRenderer
    {
      public:
        explicit HudRenderer(SDL_GPUDevice* device) : device_(device) {}
        ~HudRenderer();
        HudRenderer(const HudRenderer&) = delete;
        HudRenderer& operator=(const HudRenderer&) = delete;
        bool draw(const HudState&, const Resources&, std::string& error);
        // Original shell F1 (without Shift): CNewStatusBar::ToggleWeapons.
        void toggle_weapons(float time)
        {
            if (status_)
                status_->ToggleWeapons(time);
        }
        // Original shell VK_TAB: CNewStatusBar::ToggleObjectives; true when missionobj.wav plays.
        bool toggle_objectives(float time)
        {
            return status_ && status_->ToggleObjectives(time);
        }
        const CNewStatusBar* status_bar() const
        {
            return status_.get();
        }

      private:
        // The crosshair slot fits the widest original zoom cursor (crosshair98, 97x41).
        // The second row holds the three item tabs, the current item tab and the item name;
        // the third the ten weapon tabs, five per line; the fourth band the objectives page,
        // its updated icon and the power bar.
        static constexpr unsigned atlas_width = 1024, atlas_height = 512, crosshair_x = 656, team_x = 768, flag_x = 896,
                                  items_y = 128, weapons_y = 192, panels_y = 256, objectives_icon_x = 512,
                                  power_x = 768;
        const HudSurface* crosshair_ = nullptr;
        DBYTE crosshair_number_ = 0;
        SDL_GPUDevice* device_ = nullptr;
        SDL_GPUTexture* texture_ = nullptr;
        SDL_GPUTransferBuffer* transfer_ = nullptr;
        std::unique_ptr<CNewStatusBar> status_;
        const Resources* resources_ = nullptr;
        std::uint64_t uploaded_revision_ = 0;
        std::uint64_t item_messages_ = UINT64_MAX, item_shows_ = 0, world_revision_ = 0;
        std::uint64_t objective_updates_ = 0, power_requests_ = 0;
        // cres.dll IDS_ITEM_* names, read once on first need; failure leaves names empty.
        StringResources item_names_;
        bool item_names_read_ = false;
        bool upload(std::string& error);
    };
    // Original CMessageMgr::Draw (expiring lines) and the CInputLine editor ("SAY:" with a cursor in
    // the second half of each second) at the manager's coordinates, 16 pixels per line, white
    // over the scene with no shadow. Replacement font: Dear ImGui's embedded default at the
    // original 16-pixel height instead of the GDI Arial request. `notice` is the port's own
    // control feedback, drawn as one more line. Returns the number of drawn lines.
    int draw_messages(CMessageMgr&, float time, const std::string* editor, const std::string& notice);
    bool draw_screen_fade(float scale, bool in_world);
    // Black rows over a letterbox camera's top and bottom sixth (the scene clears to the fog colour).
    bool draw_letterbox(bool letterbox);
    // The last drawn CCredit, centered in the 640x480 menu viewport. Replacement font:
    // Dear ImGui's embedded default instead of the original GDI Arial request.
    bool draw_credits(const CCredits&);
    // The on-screen touch controls: outlined circles, filled while held, and the floating stick.
    void draw_touch_controls(const TouchControls&);
} // namespace OpenBlood2
