#pragma once
#include "engine/audio.h"
#include "game/fire_result.h"
#include "game/weapon_messages.h"
namespace OpenBlood2
{
    // One hand's checked owner/selection for a CViewWeapon update. The solo bridge
    // delivers fire synchronously; a network consumer will predict presentation
    // and submit a typed request. Neither exposes original server objects here.
    class WeaponServices
    {
      public:
        virtual ~WeaponServices() = default;
        virtual float time() const = 0;
        virtual bool has_player() const = 0;
        virtual bool player_alive() const = 0;
        virtual bool weapon_selected(DBYTE type, bool left) const = 0;
        virtual float ammunition(bool alternate) const = 0;
        virtual FireResult fire(const WeaponFire&) = 0;
        virtual SoundId play_sound(const SoundRequest&, std::string& error) = 0;
    };
} // namespace OpenBlood2
