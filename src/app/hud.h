#pragma once
#include "game/player_messages.h"
#include <string>
namespace OpenBlood2
{
    // Presentation values only, read from the checked typed client snapshot.
    struct HudState
    {
        bool visible = false;
        unsigned health = 0, armor = 0, ammo = 0, alternate_ammo = 0;
        bool uses_ammo = false, alternate_pool = false;
        DBYTE slot = 0;
        std::array<DBYTE, 10> slots{};
        std::string weapon;
    };
    HudState hud_state(const PlayerStats&, bool in_world, bool external_camera);
    bool draw_hud(const HudState&);
    bool draw_screen_fade(float scale, bool in_world);
} // namespace OpenBlood2
