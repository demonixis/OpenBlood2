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
    struct PlayerDetonate
    {
    };
    struct PlayerItemCommand
    {
        DBYTE type = 1;
    };
    struct InventoryAction
    {
        DBYTE type = 1;
        bool active = false;
    };
    // Original SMSG_ZOOMVIEW: zoom flag and crosshair override (0 keeps the current one).
    struct ZoomView
    {
        bool on = false;
        DBYTE type = 0;
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
        std::uint32_t ground_surface = 0;
        std::uint64_t stair_attempts = 0, stair_steps = 0, overlap_contacts = 0, start_departures = 0;
        float last_step_height = 0, last_overlap_depth = 0;
        HOBJECT ground_object = nullptr;
        std::uint32_t ground_polygon = UINT32_MAX;
    };
    struct PlayerMove
    {
        DBYTE move_code = 0;
        DVector position{}, velocity{}, acceleration{};
        bool on_ground = false;
        DBYTE surface = 0;
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
        float ammo = 0, alternate_ammo = 0;
        DBYTE weapon = 0, weapon_slot = 0, ammo_type = 0, alternate_ammo_type = 0;
        std::array<DBYTE, 10> weapon_slots{};
        float max_health = 100, max_armor = 100;
        bool operator==(const PlayerStats&) const = default;
    };
    struct VoiceEvent
    {
        bool group = true;
        int character = 0, event = 0;
    };
    struct VoiceStop
    {
    };
    struct PlayerDead
    {
    };
    enum class MusicCommandType : DBYTE
    {
        Level,
        Motif,
        MotifLoop,
        MotifStop,
        Break
    };
    struct MusicCommand
    {
        MusicCommandType type = MusicCommandType::Level;
        DBYTE level = 0;
        std::string name;
    };
    bool valid_music_command(const MusicCommand&);
    struct ScreenFade
    {
        bool in = false;
    };
    struct BossHealth
    {
        float fraction = 0;
    };
    struct Conversation
    {
        std::string file, text;
        DBYTE character = 0;
    };
    struct MissionObjectives
    {
        std::uint32_t resource = 0;
        std::string title, text;
    };
    enum class WorldExitType : DBYTE
    {
        World = 0,
        Subworld = 1,
        Episode = 2
    };
    struct WorldExit
    {
        std::string next_world; // Empty represents the source null HSTRING.
        WorldExitType type = WorldExitType::World;
    };
    using PlayerEvent = std::variant<PlayerInitRequest, PlayerRotation, PlayerPosition, PlayerPhysics, PlayerStats,
                                     MissionObjectives, WorldExit, BossHealth, Conversation, InventoryAction,
                                     ScreenFade, VoiceEvent, VoiceStop, PlayerDead, MusicCommand, ZoomView>;
} // namespace OpenBlood2
