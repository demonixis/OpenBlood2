#pragma once
#include "app/touch_controls.h"
#include "client/hud.h"
#include "engine/localization.h"
#include "engine/string_resources.h"
#include "game/original/Credits.h"
#include "game/original/MessageMgr.h"
#include "game/original/NewStatusBar.h"
#include "game/player_messages.h"
#include <SDL3/SDL.h>
struct ImFont;
namespace OpenBlood2
{
    class LocalizedFont;
    // The client shell's objectives and power bar requests (CBloodClientShell).
    template <class Shell> void hud_panels(HudState& h, const Shell& client)
    {
        h.objective_updates = client.objective_updates;
        h.objectives_valid = client.objectives_resolved;
        h.objectives_resource = client.objectives.resource;
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
        void set_localization(const Localization* catalog, const LocalizedFont* font)
        {
            if (localization_ != catalog || localized_font_ != font)
                communication_width_ = 0;
            localization_ = catalog;
            localized_font_ = font;
        }
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
        // Inventory names and original objective pairs for save restoration, read once per archive.
        StringResources hud_strings_;
        std::string hud_strings_error_;
        bool hud_strings_read_ = false;
        const Localization* localization_ = nullptr;
        const LocalizedFont* localized_font_ = nullptr;
        Language language_ = Language::English;
        const Resources* communication_resources_ = nullptr;
        std::unique_ptr<HudCanvas> communication_canvas_;
        HudFont communication_font_;
        SDL_GPUTexture* communication_texture_ = nullptr;
        SDL_GPUTransferBuffer* communication_transfer_ = nullptr;
        CommLinkHudState drawn_communication_;
        unsigned communication_width_ = 0, communication_height_ = 0;
        Language communication_language_ = Language::English;
        bool draw_communication(const CommLinkHudState&, const Resources&, std::string& error);
        void read_hud_strings(const Resources&);
        bool upload(std::string& error);
    };
    // Original CMessageMgr::Draw (expiring lines) and the CInputLine editor ("SAY:" with a cursor in
    // the second half of each second) at the manager's coordinates, 16 pixels per line, white
    // over the scene with no shadow. Replacement font: Dear ImGui's embedded default at the
    // original 16-pixel height instead of the GDI Arial request. `notice` is the port's own
    // control feedback, drawn as one more line. Returns the number of drawn lines.
    int draw_messages(CMessageMgr&, float time, const std::string* editor, const std::string& notice,
                      ImFont* localized_font = nullptr, const Localization* localization = nullptr,
                      const Resources* resources = nullptr, const LocalizedFont* glyphs = nullptr);
    bool draw_screen_fade(float scale, bool in_world);
    // Black rows over a letterbox camera's top and bottom sixth (the scene clears to the fog colour).
    bool draw_letterbox(bool letterbox);
    // The last drawn CCredit, centered in the 640x480 menu viewport. Replacement font:
    // Dear ImGui's embedded default instead of the original GDI Arial request.
    bool draw_credits(const CCredits&, const Resources&, const Localization*, ImFont* localized_font,
                      const LocalizedFont* glyphs);
    // The on-screen touch controls: outlined circles, filled while held, and the floating stick.
    void draw_touch_controls(const TouchControls&, const Localization* = nullptr, ImFont* = nullptr,
                             const LocalizedFont* = nullptr);
} // namespace OpenBlood2
