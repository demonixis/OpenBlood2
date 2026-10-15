#pragma once
#include "basedefs_de.h"
#include <array>
#include <string>
#include <variant>

// Owned, typed local equivalents of the original entry messages. No wire ABI,
// pointers into packets, arbitrary void* client data or network implementation.
namespace OpenBlood2
{
    struct PlayerInitRequest
    {
    };
    struct PlayerInit
    {
        std::string name;
        DBYTE character = 0, skin = 0;
        DBYTE strength = 5, speed = 3, resistance = 3, magic = 1;
        std::array<DBYTE, 9> weapons{1}; // Original fresh single-player Beretta slot.
    };
    struct PlayerRotation
    {
        float pitch = 0, yaw = 0;
    };
    struct PlayerUse
    {
        PlayerRotation rotation;
    };
    struct PlayerPosition
    {
        DVector position;
        DBYTE move_code = 0;
    };
    // Device-independent commands; platform bindings never enter original game code.
    struct MovementInput
    {
        bool forward = false, backward = false, left = false, right = false, run = false, jump = false, crouch = false;
    };
    enum class ResizeResult
    {
        Resized,
        Blocked,
        Invalid,
        Unavailable
    };
    struct PlayerControls
    {
        MovementInput movement;
        bool firing = false;
        float yaw = 0;
    };
    struct MovementState
    {
        DVector position{}, velocity{}, acceleration{}, dimensions{}, ground_normal{};
        bool standing = false;
        std::uint64_t stair_attempts = 0, stair_steps = 0;
        float last_step_height = 0;
        HOBJECT ground_object = nullptr;
        std::uint32_t ground_polygon = UINT32_MAX;
    };
    struct PlayerMove
    {
        DBYTE move_code = 0;
        DVector position{}, velocity{}, acceleration{};
        bool on_ground = false;
    };
    struct PlayerPhysics
    {
        std::string model, skin;
        DVector dimensions, gravity;
        float move_speed = 0, jump_speed = 600;
    };
    struct PlayerStats
    {
        float health = 0, armor = 0, bullets = 0;
    };
    using PlayerEvent = std::variant<PlayerInitRequest, PlayerRotation, PlayerPosition, PlayerPhysics, PlayerStats>;
} // namespace OpenBlood2
