#pragma once
#include "engine/object_id.h"
#include "game/player_messages.h"
#include <span>
#include <variant>
#include <vector>

namespace OpenBlood2::Net
{
    // Reports travel on the unreliable State channel (1200-byte packets).
    inline constexpr std::size_t movement_packet_limit = 1200;
    // A report carries the CMoveMgr substeps integrated since the previous one.
    // The client reports after 1/30 s or 12 steps, and one frame adds at most 12.
    inline constexpr std::size_t movement_step_limit = 24;
    // CMoveMgr substeps are at most 1/120 s; the slack covers float rounding.
    inline constexpr float movement_step_seconds = 1.0f / 120 + 1e-6f;
    // Coarse server leash (units/second) for replayed steps and catch-ups. It
    // stops teleports through open space, not a modified client running faster
    // than the original speeds; jumps, falls and impulses stay well below it.
    inline constexpr float movement_speed_limit = 8000;
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
    // One CMoveMgr substep before collision: the integrated displacement, the
    // pre-collision velocity (original WritePositionInfo; impacts and falling
    // damage read it) and the frame's acceleration/ground/surface values.
    struct MovementStep
    {
        DVector offset{}, velocity{}, acceleration{};
        float seconds = 0;
        bool on_ground = false;
        DBYTE surface = 0;
    };
    // `move` is the client's predicted result after the last step (position,
    // pre-collision velocity), used for diagnostics; the server replays `steps`
    // from its own body and resolves each one like the solo submission. `origin`
    // is the predicted body position before the first step, used only to catch
    // up after a lost report.
    struct MovementReport
    {
        MovementIdentity owner;
        std::uint64_t sequence = 0;
        PlayerMove move;
        PlayerControls controls;
        float pitch = 0;
        DVector origin{};
        std::vector<MovementStep> steps;
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
