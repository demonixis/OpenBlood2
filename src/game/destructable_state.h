#pragma once
#include "game/weapon_messages.h"
#include <string>

namespace OpenBlood2
{
    // Original CDestructable Save/Load values; owner identity is deliberately
    // absent. Load keeps the destination binding, as the original player path did.
    struct DestructableState
    {
        bool apply_damage_physics = false, dead = false, nigh_invulnerable = false;
        bool god_mode = false, trigger_only = false;
        float mass = 1, max_health = 1, max_mega_health = 1, health = 1, death_health = 1;
        float max_armor = 1, max_necro_armor = 1, armor = 0, resistance = 1;
        DamageType last_type = DamageType::Normal;
        DBYTE last_location = 0;
        DVector last_direction{}, spawn_velocity{};
        float last_amount = 0, last_percent = 0, death_delay = 0;
        std::string damage_target, damage_message, death_target, death_message, spawn_object;
    };
} // namespace OpenBlood2
