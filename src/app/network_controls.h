#pragma once
#include "app/movement_controls.h"
#include "net/player_commands.h"
namespace OpenBlood2
{
    // Samples existing bindings; device Press state becomes one reliable command
    // on its rising edge, while queued commands retain the original ordering.
    class NetworkControls
    {
      public:
        void sample(MovementControls& controls, bool blocked, PlayerRotation aim, std::vector<Net::CommandAction>& out)
        {
            out.clear();
            using Kind = Net::CommandKind;
            if (const int slot = controls.sample_weapon_slot(blocked); slot >= 0)
                out.push_back({Kind::WeaponSlot, DBYTE(slot)});
            for (const auto [action, kind] : {std::pair{InputAction::NextWeapon, Kind::NextWeapon},
                                              {InputAction::PrevWeapon, Kind::PreviousWeapon}})
                if (controls.sample_command(action, blocked))
                    out.push_back({kind});
            unsigned index = 0;
            const auto edge = [&](InputAction action)
            {
                const bool down = controls.sample_press(action, blocked);
                const bool result = down && !previous_[index];
                previous_[index++] = down;
                return result;
            };
            // Original UpdateUse order, including the detonate position.
            for (const auto [action, type] : {std::pair{InputAction::NightGoggles, 3},
                                              {InputAction::TheEye, 4},
                                              {InputAction::Flashlight, 2},
                                              {InputAction::RemoteBomb, 8},
                                              {InputAction::Detonate, 0},
                                              {InputAction::TimeBomb, 9},
                                              {InputAction::ProximityBomb, 7},
                                              {InputAction::Binoculars, 5},
                                              {InputAction::Medkit, 1}})
                if (edge(action))
                    out.push_back({type ? Kind::Item : Kind::Detonate, DBYTE(type)});
            for (const auto [action, kind] : {std::pair{InputAction::NextItem, Kind::NextItem},
                                              {InputAction::PrevItem, Kind::PreviousItem},
                                              {InputAction::UseItem, Kind::UseItem}})
                if (controls.sample_command(action, blocked))
                    out.push_back({kind});
            if (edge(InputAction::Use))
                out.push_back({Kind::Use, 0, aim});
            // Original UpdateWeapons toggles sniper zoom on the alternate edge.
            if (edge(InputAction::FireAlternate))
                out.push_back({Kind::Alternate});
        }

      private:
        std::array<bool, 11> previous_{};
    };
} // namespace OpenBlood2
