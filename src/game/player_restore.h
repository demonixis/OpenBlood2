#pragma once
#include "engine/model.h"
#include "game/destructable_state.h"
#include "game/inventory_state.h"
#include "game/player_state.h"

namespace OpenBlood2
{
    // Owned values for the supported local player. No retail save format,
    // client token, source object, contact identity or borrowed resource.
    struct PlayerRestoreState
    {
        std::string world, name, model, skin;
        DVector position{}, scale{1, 1, 1}, dimensions{}, velocity{}, acceleration{};
        DRotation rotation;
        AnimationTrack animation;
        std::vector<std::uint8_t> hidden_nodes;
        DDWORD flags = 0, user_flags = 0;
        DBYTE blocking_priority = 0;
        double next_update = 0; // Remaining time, not a source-world deadline.
        bool crouched = false;
        float eye_level = 36;
        InventoryState inventory;
        DestructableState damage;
        PlayerState player;
    };
} // namespace OpenBlood2
