#include "client/replica_movement.h"
#include <cmath>

namespace OpenBlood2
{
    void ReplicaMovement::clear()
    {
        state = {};
        revision_ = 0;
        code_ = surface_ = 0;
        character_ = 0;
        eye_level_ = report_timer_ = 0;
        crouched_ = positioned_ = false;
        controls_ = {};
        // Reports remain distinct if this adapter is rebound to the same owner.
    }
    bool ReplicaMovement::bind(ClientObjectId owner, std::uint64_t revision, DBYTE code, int character, float eye_level,
                               bool crouched)
    {
        if (state.owner.world || !owner.world || owner.world != collision_.frame().world || !revision ||
            character < 0 || character > 3 || !std::isfinite(eye_level) || eye_level < 0 || eye_level > 1000)
            return false;
        const auto* collider = collision_.get(owner.object);
        if (!collider || collider->type != OT_MODEL || collider->attached)
            return false;
        ClientMotionState initial;
        initial.position = collider->position;
        initial.dimensions = collider->dimensions;
        initial.velocity = collider->velocity;
        initial.acceleration = collider->acceleration;
        if (!collision_.support(owner, initial))
            return false;
        state.owner = owner;
        state.body = initial;
        revision_ = revision;
        code_ = code;
        character_ = character;
        eye_level_ = eye_level;
        crouched_ = crouched;
        return true;
    }
    bool ReplicaMovement::valid() const
    {
        return revision_ && state.owner.world == collision_.frame().world && collision_.get(state.owner.object);
    }
    bool ReplicaMovement::synchronize()
    {
        // A repeated substep must not reset prediction to an older server pose.
        return valid();
    }
    bool ReplicaMovement::force_position(const PlayerPosition& value)
    {
        if (!valid() || value.move_code != code_ || !physics_vector(value.position))
            return false;
        auto next = state.body;
        next.position = value.position;
        next.velocity = next.acceleration = {};
        if (!collision_.support(state.owner, next))
            return false;
        state.body = next;
        positioned_ = true;
        report_timer_ = 0;
        return true;
    }
    bool ReplicaMovement::controls(const PlayerControls& value, bool send, bool& crouched)
    {
        if (!valid() || !std::isfinite(value.yaw) || !std::isfinite(value.movement.mouse_forward) ||
            !std::isfinite(value.movement.mouse_strafe))
            return false;
        controls_ = send ? value : PlayerControls{{}, false, value.yaw};
        crouched = crouched_;
        return true;
    }
    bool ReplicaMovement::environment(ClientContainerState& value, int& character)
    {
        if (!valid())
            return false;
        auto head = state.body.position;
        head.y += eye_level_;
        character = character_;
        return collision_.environment(state.owner, state.body, head, value);
    }
    bool ReplicaMovement::finish(float seconds, ClientContainerState& value)
    {
        int character;
        return std::isfinite(seconds) && seconds >= 0 && environment(value, character);
    }
    bool ReplicaMovement::advance(const PlayerMove& move)
    {
        ClientMotionState predicted;
        if (!valid() || !positioned_ || move.move_code != code_ ||
            !collision_.predict(state.owner, body(), move.position, move.velocity, move.acceleration, predicted))
        {
            ++state.prediction_failures;
            return false;
        }
        body() = predicted;
        surface_ = move.surface;
        ++state.predictions;
        return true;
    }
    bool ReplicaMovement::report(double seconds, std::optional<ReplicaMoveReport>& out)
    {
        if (!valid() || !positioned_ || !std::isfinite(seconds) || seconds < 0 || seconds > 60 ||
            next_sequence_ == UINT64_MAX)
            return false;
        report_timer_ -= static_cast<float>(seconds);
        out.reset();
        if (report_timer_ > 0)
            return true;
        report_timer_ = .14f;
        const auto& b = body();
        out = ReplicaMoveReport{
            state.owner,
            revision_,
            ++next_sequence_,
            {code_, b.position, b.velocity, b.acceleration, b.standing && b.ground_normal.y >= .5f, surface_},
            controls_};
        return true;
    }
} // namespace OpenBlood2
