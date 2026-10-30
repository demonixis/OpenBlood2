#pragma once
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
    // Stable tags are these alternatives plus one. Append only with a protocol
    // revision. Init/physics/force messages are handled by admission/movement.
    using PrivateValue =
        std::variant<BloodbathStart, PlayerRotation, PlayerStats, MissionObjectives, WorldExit, BossHealth,
                     Conversation, InventoryAction, ScreenFade, VoiceEvent, VoiceStop, PlayerDead, MusicCommand,
                     ZoomView, EyeEvent, AttachEvent, OrbEvent, FlagEvent, InventoryItems, PlayerText>;
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
