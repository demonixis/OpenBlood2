#pragma once
#include "engine/object_id.h"
#include "game/player_messages.h"
#include <span>
#include <variant>
#include <vector>

namespace OpenBlood2::Net
{
    inline constexpr std::size_t movement_packet_limit = 256;
    struct MovementIdentity
    {
        std::uint64_t world = 0, player = 0;
        ObjectId object;
        std::uint64_t force_revision = 0;
        bool operator==(const MovementIdentity&) const = default;
    };
    // Reliable private setup. A changed force revision snaps position; ordinary
    // stance/physics updates do not reset the client's predicted position.
    struct MovementSetup
    {
        MovementIdentity owner;
        std::uint64_t sequence = 0;
        DBYTE code = 0, character = 0;
        bool crouched = false, can_move = true;
        float eye_level = 0;
        DVector position{}, dimensions{}, gravity{};
        float move_speed = 0, jump_speed = 600;
    };
    struct MovementReport
    {
        MovementIdentity owner;
        std::uint64_t sequence = 0;
        PlayerMove move;
        PlayerControls controls;
        float pitch = 0;
    };
    // Capture at report acceptance, before subsequent velocity advancement.
    // Using a later snapshot here would add a second velocity*time correction.
    struct MovementAck
    {
        MovementIdentity owner;
        std::uint64_t sequence = 0, setup_sequence = 0;
        PlayerMove accepted;
        DVector dimensions{};
    };
    using MovementMessage = std::variant<MovementSetup, MovementReport, MovementAck>;
    enum class DecodeMovement
    {
        Ok,
        Malformed,
        Protocol
    };
    bool valid_movement(const MovementMessage&);
    bool is_movement_packet(std::span<const std::uint8_t>);
    bool encode_movement(const MovementMessage&, std::vector<std::uint8_t>&);
    DecodeMovement decode_movement(std::span<const std::uint8_t>, MovementMessage&);
} // namespace OpenBlood2::Net
