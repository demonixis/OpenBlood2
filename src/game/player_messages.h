#pragma once
#include "basedefs_de.h"
#include <array>
#include <cstdint>
#include <optional>
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
    struct PlayerAltFire
    {
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
    // Original COMMAND_NEXTINVENTORY/PREVINVENTORY (client to server).
    struct PlayerItemCursor
    {
        bool next = true;
    };
    // Original COMMAND_USEINVENTORY (client to server).
    struct PlayerUseInventory
    {
    };
    // One SMSG_ITEMCHANGED item: original picture names (interface/statusbar/items/),
    // the cres.dll name resource (0 for keys) or a key's authored display name, and
    // GetItemCharges.
    struct InventoryItemView
    {
        DBYTE type = 0;
        std::string icon, highlighted_icon, name;
        std::uint32_t name_id = 0;
        DBYTE charges = 0;
        bool operator==(const InventoryItemView&) const = default;
    };
    // Original SMSG_ITEMCHANGED values: current, previous and next items around the
    // cursor; show is ITEMF_SHOWITEMS (a one-shot request to slide the tabs in).
    struct InventoryItems
    {
        std::optional<InventoryItemView> current, previous, next;
        bool show = false;
        bool operator==(const InventoryItems&) const = default;
    };
    // Original SMSG_ZOOMVIEW: zoom flag and crosshair override (0 keeps the current one).
    struct ZoomView
    {
        bool on = false;
        DBYTE type = 0;
    };
    struct OrbView
    {
        HOBJECT orb = nullptr;
        bool active = false;
    };
    struct SeeingEyeView
    {
        HOBJECT eye = nullptr;
        bool active = false;
        float blind_seconds = 0;
    };
    struct CreatureAttach
    {
        HOBJECT creature = nullptr, enemy = nullptr;
        bool active = true;
    };
    struct CreatureResponse
    {
        HOBJECT creature = nullptr;
        bool proceed = false;
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
        Unavailable,
        // A valid shape change refused because a movement transaction (contact callback) owns
        // the body. The body is unchanged, like Blocked; callers decide whether to continue.
        InTransaction
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
    enum class CTFEventKind : DBYTE
    {
        Team,
        Grabbed,
        Returned,
        Dropped,
        NeedFlag,
        Captured
    };
    struct CTFEvent
    {
        CTFEventKind kind = CTFEventKind::Team;
        DBYTE team = 0;
        int points = 0;
        HOBJECT capturer = nullptr; // Identity only; never dereferenced by presentation.
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
                                     ScreenFade, VoiceEvent, VoiceStop, PlayerDead, MusicCommand, ZoomView,
                                     SeeingEyeView, CreatureAttach, OrbView, CTFEvent, InventoryItems>;
} // namespace OpenBlood2
