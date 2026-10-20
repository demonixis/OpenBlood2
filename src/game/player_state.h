#pragma once
#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace OpenBlood2
{
    // Original CPlayerObj Save fields this port keeps but does not interpret. They start at
    // the original constructor values and are carried unchanged by Save/Load
    // (PlayerObj.cpp:6541-6873), so a save reproduces the original field layout.
    struct PlayerCarried
    {
        std::uint8_t demo_playback = 0, firing_weapon = 0, last_jump = 0;
        std::uint8_t client_flags = 1; // CDATA_SHOWGUN.
        std::uint32_t control_flags = 0, frags = 0, melee_kills = 0;
        float binding_regen = 0, server_regen = 0, mouse_axis0 = 0, mouse_axis1 = 0;
        std::array<float, 3> last_position{}, last_velocity{}, scale{1, 1, 1}, forward{};
        std::array<float, 3> gun_muzzle{}, left_gun_muzzle{};
        std::array<float, 4> view_rotation{0, 0, 0, 1};           // x, y, z, w.
        std::array<std::uint8_t, 4> binding_attributes{};         // Strength, speed, resistance, magic.
        std::array<std::uint8_t, 8> bindings{};                   // Constitution .. impact resistance, as saved.
        std::uint8_t injured_leg = 0, node_hit = 0, side_hit = 0; // Saved as (DBYTE) in a Word.
        float death_timer = 0, old_air = 100, burning_time = 0;
        std::uint8_t slow_death = 0, swimming_jump = 0, can_swim_jump = 0, burning = 0;
        bool operator==(const PlayerCarried&) const = default;
    };
    // Original CPlayerObj value block, not a whole player checkpoint. Engine body/model
    // state, aggregate state and client data are separate. Clocks are relative offsets.
    struct PlayerState
    {
        float smell_time_offset = 0, smell_delay = .5f;
        bool dead = false, movement_blocked = false, spectator = false;
        bool imprisoned = false;
        float imprison_start_offset = 0, imprison_length = 0;
        bool falling = false, server_falling = false;
        float fall_start_offset = -1, min_fall_y = 0, max_fall_y = 0, idle_start_offset = 0;
        // PU_INVULNERABLE .. PU_SILENT flags, expiry offsets and the power sound stack (bottom first).
        std::array<bool, 5> powers{};
        std::array<float, 5> power_offsets{};
        std::vector<std::uint8_t> power_sounds;
        float pitch = 0, yaw = 0, air = 100;
        std::optional<std::string> trigger, name; // Preserve null versus allocated empty HSTRING.
        bool last_crouch = false, forced_crouch = false;
        std::uint8_t character = 0, skin = 0;
        std::uint8_t strength = 1, speed = 1, resistance = 1, magic = 1;
        std::uint8_t move_state = 0; // Original 0..14 or PMOVE_DEAD (16); humiliation (15) is unavailable.
        PlayerCarried carried;
        bool operator==(const PlayerState&) const = default;
    };
} // namespace OpenBlood2
