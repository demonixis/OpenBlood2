#pragma once
#include <cstdint>
#include <optional>
#include <string>

namespace OpenBlood2
{
    // Selected original CPlayerObj value block, not a whole player checkpoint.
    // Engine body/model state, aggregate state and client data are separate.
    struct PlayerState
    {
        float smell_time_offset = 0, smell_delay = .5f;
        bool dead = false, movement_blocked = false;
        bool imprisoned = false;
        float imprison_start_offset = 0, imprison_length = 0;
        bool falling = false, server_falling = false;
        float fall_start_offset = -1, min_fall_y = 0, max_fall_y = 0;
        float pitch = 0, yaw = 0, air = 100;
        std::optional<std::string> trigger, name; // Preserve null versus allocated empty HSTRING.
        bool last_crouch = false, forced_crouch = false;
        std::uint8_t character = 0, skin = 0;
        std::uint8_t strength = 1, speed = 1, resistance = 1, magic = 1;
        std::uint8_t move_state = 0; // Original 0..14 or PMOVE_DEAD (16); humiliation (15) is unavailable.
        bool operator==(const PlayerState&) const = default;
    };
} // namespace OpenBlood2
