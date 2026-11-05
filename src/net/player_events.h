#pragma once
#include "engine/camera_types.h"
#include "game/player_messages.h"
#include "net/admission.h"

namespace OpenBlood2::Net
{
    // Pointer-bearing legacy messages are translated while their checked server
    // handles still exist. Receivers use identities only, never legacy objects.
    struct EyeEvent
    {
        ObjectId eye;
        bool active = false;
        float blind_seconds = 0;
    };
    struct OrbEvent
    {
        ObjectId orb;
        bool active = false;
    };
    struct AttachEvent
    {
        ObjectId creature, enemy;
        bool active = true;
    };
    struct FlagEvent
    {
        CTFEventKind kind = CTFEventKind::Team;
        DBYTE team = 0;
        int points = 0;
        ObjectId capturer;
    };
    struct CameraState
    {
        std::uint64_t force_revision = 0;
        ObjectId scripted, eye, orb;
        CameraSettings settings;
        float eye_level = 0;
        bool alive = false;
        bool operator==(const CameraState&) const = default;
    };
    struct WeaponLoadout
    {
        std::uint64_t force_revision = 0;
        DBYTE character = 0, slot = 0;
        std::array<DBYTE, 2> weapons{}; // Right, left; zero means absent.
        std::array<float, 2> ammunition{}, alternate_ammunition{};
        bool alive = false, input_enabled = false, fire_allowed = false, binoculars = false;
        bool operator==(const WeaponLoadout&) const = default;
    };
    // Stable tags are these alternatives plus one. Append only with a protocol
    // revision. Init/physics/force messages are handled by admission/movement.
    using PrivateValue = std::variant<BloodbathStart, PlayerRotation, PlayerStats, MissionObjectives, WorldExit,
                                      BossHealth, Conversation, InventoryAction, ScreenFade, VoiceEvent, VoiceStop,
                                      PlayerDead, MusicCommand, ZoomView, EyeEvent, AttachEvent, OrbEvent, FlagEvent,
                                      InventoryItems, PlayerText, CameraState, WeaponLoadout>;
    struct PrivateEvent
    {
        Joined owner;
        std::uint64_t sequence = 0, tick = 0;
        PrivateValue value;
    };
    inline constexpr std::size_t private_packet_limit = 2048, private_queue_limit = 256;
    bool valid_private(const PrivateEvent&);
    bool is_private_packet(std::span<const std::uint8_t>);
    bool encode_private(const PrivateEvent&, std::vector<std::uint8_t>&);
    enum class DecodePrivate
    {
        Ok,
        Malformed,
        Protocol
    };
    DecodePrivate decode_private(std::span<const std::uint8_t>, PrivateEvent&);
} // namespace OpenBlood2::Net
