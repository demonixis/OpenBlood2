#include "client/network_movement.h"
#include <algorithm>
#include <cmath>

namespace OpenBlood2
{
    namespace
    {
        bool same(DVector a, DVector b)
        {
            return a.x == b.x && a.y == b.y && a.z == b.z;
        }
        bool same_constraints(const Net::MovementSetup& a, const Net::MovementSetup& b)
        {
            return a.character == b.character && a.crouched == b.crouched && a.can_move == b.can_move &&
                   a.eye_level == b.eye_level && same(a.dimensions, b.dimensions) && same(a.gravity, b.gravity) &&
                   a.move_speed == b.move_speed && a.jump_speed == b.jump_speed;
        }
        PlayerPhysics physics(const Net::MovementSetup& v)
        {
            return {{}, {}, v.dimensions, v.gravity, v.move_speed, v.jump_speed};
        }
    } // namespace
    bool NetworkMovement::belongs(const Net::MovementIdentity& id) const
    {
        return joined_.world && id.world == joined_.world && id.player == joined_.player && id.object == joined_.object;
    }
    void NetworkMovement::clear_history()
    {
        retired_ = std::max(retired_, sent_);
        frames_.clear();
        reports_.clear();
        future_ack_.reset();
    }
    bool NetworkMovement::begin(const Net::Joined& joined)
    {
        if (!joined.world || !joined.player || !joined.object || !joined.object.generation ||
            joined.object.slot >= 65536)
            return false;
        manager_.Term();
        joined_ = joined;
        setup_ = {};
        next_frame_ = sent_ = acknowledged_ = retired_ = 0;
        corrections = replays = ignored = deferred = 0;
        clear_history();
        return true;
    }
    void NetworkMovement::trim()
    {
        while (frames_.size() > frame_limit)
            frames_.pop_front();
        while (!reports_.empty() && (reports_.size() > report_limit ||
                                     (!frames_.empty() && reports_.front().frame + 1 < frames_.front().id)))
        {
            retired_ = std::max(retired_, reports_.front().report);
            reports_.pop_front();
        }
    }
    NetworkMovement::Reception NetworkMovement::receive(const Net::MovementSetup& value, std::string& error)
    {
        error.clear();
        if (!Net::valid_movement(value))
        {
            error = "Invalid movement setup";
            return Reception::Refused;
        }
        if (!belongs(value.owner) || value.sequence <= setup_.sequence ||
            value.owner.force_revision < setup_.owner.force_revision)
        {
            ++ignored;
            return Reception::Ignored;
        }
        const bool force = !setup_.sequence || value.owner.force_revision != setup_.owner.force_revision;
        if (!force && value.code != setup_.code)
        {
            error = "Movement code changed without a force generation";
            return Reception::Refused;
        }
        const ClientObjectId owner{joined_.world, joined_.object};
        if (collision_.frame().world != owner.world || !collision_.get(owner.object))
        {
            ++deferred;
            return Reception::Deferred;
        }
        if (force)
        {
            const auto previous_manager = manager_.checkpoint();
            const auto previous_prediction = movement_.prediction();
            const bool preserve = setup_.sequence != 0;
            const auto apply = [&](ReplicaMovement& movement, CMoveMgr& manager)
            {
                manager.Term();
                if (!movement.bind(owner, value.owner.force_revision, value.code, value.character, value.eye_level,
                                   value.crouched))
                    return false;
                if (preserve)
                {
                    manager.restore(previous_manager);
                    if (!movement.restore_prediction(previous_prediction))
                        return false;
                }
                movement.body().dimensions = value.dimensions;
                return manager.OnPhysicsUpdate(physics(value)) &&
                       manager.OnServerForcePos({value.position, value.code});
            };
            // Pure queries, no callbacks: preflight before replacing active state.
            ReplicaMovement candidate(collision_);
            CMoveMgr candidate_manager(candidate);
            if (!apply(candidate, candidate_manager))
            {
                ++deferred;
                return Reception::Deferred;
            }
            if (!apply(movement_, manager_))
            {
                error = "Movement force preflight changed unexpectedly";
                return Reception::Refused;
            }
        }
        else
        {
            auto body = movement_.body();
            if (!same(body.dimensions, value.dimensions))
            {
                const auto resize = collision_.resize(owner, body, value.dimensions, body);
                if (resize == CollisionQuery::Invalid)
                {
                    ++deferred;
                    return Reception::Deferred;
                }
                // A delayed server stance can grow beneath a ceiling the client
                // has already reached. Keep the checked smaller body until an
                // ACK supplies a corresponding authoritative position/shape.
            }
            if (!manager_.OnPhysicsUpdate(physics(value)) ||
                !movement_.stance(value.character, value.eye_level, value.crouched))
            {
                error = "Invalid movement physics/stance";
                return Reception::Refused;
            }
            movement_.body() = body;
        }
        const bool changed = force || !same_constraints(setup_, value);
        setup_ = value;
        // A changed physics/stance boundary cannot replay older input using new
        // constraints. New reports immediately establish checkpoints in this setup.
        if (changed)
            clear_history();
        return Reception::Applied;
    }
    bool NetworkMovement::update(const MovementInput& input, float yaw, float pitch, double seconds, bool commands,
                                 bool firing, bool external_camera, std::optional<Net::MovementReport>& out,
                                 std::string& error)
    {
        error.clear();
        out.reset();
        if (!ready() || next_frame_ == UINT64_MAX || !std::isfinite(yaw) || std::abs(yaw) > 1e6f ||
            !std::isfinite(pitch) || std::abs(pitch) > 1e6f || !std::isfinite(seconds) || seconds < 0 || seconds > 60 ||
            !std::isfinite(input.mouse_forward) || std::abs(input.mouse_forward) > 1e7f ||
            !std::isfinite(input.mouse_strafe) || std::abs(input.mouse_strafe) > 1e7f)
        {
            error = "Movement prediction requires a ready client and bounded aim";
            return false;
        }
        if (future_ack_ && future_ack_->setup_sequence == setup_.sequence)
        {
            const auto pending = *future_ack_;
            const auto result = receive(pending, error);
            if (result == Reception::Refused)
                return false;
            if (result != Reception::Deferred)
                future_ack_.reset();
        }
        const auto original_manager = manager_.checkpoint();
        const auto original_prediction = movement_.prediction();
        if (!manager_.Update(external_camera, input, yaw, seconds, commands && setup_.can_move, firing, pitch))
        {
            manager_.restore(original_manager);
            movement_.restore_prediction(original_prediction);
            error = "Independent CMoveMgr prediction failed";
            return false;
        }
        frames_.push_back({++next_frame_, input, yaw, pitch, seconds, commands, firing, external_camera});
        std::optional<ReplicaMoveReport> report;
        if (!movement_.report(seconds, report))
        {
            error = "Movement report clock failed";
            return false;
        }
        out.reset();
        if (report)
        {
            Net::MovementReport next{setup_.owner, report->sequence, report->move, report->controls, pitch};
            if (!Net::valid_movement(next))
            {
                error = "Invalid predicted movement report";
                return false;
            }
            sent_ = next.sequence;
            reports_.push_back({sent_, next_frame_, manager_.checkpoint(), movement_.prediction()});
            out = next;
        }
        trim();
        return true;
    }
    NetworkMovement::Reception NetworkMovement::receive(const Net::MovementAck& ack, std::string& error)
    {
        error.clear();
        if (!Net::valid_movement(ack))
        {
            error = "Invalid movement acknowledgement";
            return Reception::Refused;
        }
        if (!belongs(ack.owner) || ack.owner.force_revision != setup_.owner.force_revision ||
            ack.sequence <= acknowledged_ || ack.sequence <= retired_ || ack.setup_sequence < setup_.sequence)
        {
            ++ignored;
            return Reception::Ignored;
        }
        if (ack.sequence > sent_ || ack.accepted.move_code != setup_.code)
        {
            error = "Movement acknowledgement names an unsent report or wrong code";
            return Reception::Refused;
        }
        if (ack.setup_sequence > setup_.sequence)
        {
            if (!future_ack_ || future_ack_->sequence < ack.sequence)
                future_ack_ = ack;
            ++deferred;
            return Reception::Deferred;
        }
        const auto found =
            std::find_if(reports_.begin(), reports_.end(), [&](const auto& p) { return p.report == ack.sequence; });
        if (found == reports_.end())
        {
            ++ignored;
            return Reception::Ignored;
        }
        auto corrected = found->prediction;
        auto& body = corrected.state.body;
        body.position = ack.accepted.position;
        body.velocity = ack.accepted.velocity;
        body.acceleration = ack.accepted.acceleration;
        body.dimensions = ack.dimensions;
        // A newer replica may already contain a closing mover at the older ACK
        // pose. Defer instead of committing an embedded body, even with no later
        // frames to replay. Retry remains possible when collision catches up.
        ClientMotionState checked;
        if (collision_.resize(corrected.state.owner, body, body.dimensions, checked) != CollisionQuery::Miss)
        {
            ++deferred;
            return Reception::Deferred;
        }
        body = checked;
        auto checkpoint = found->manager;
        checkpoint.values.m_bOnGround = body.standing && body.ground_normal.y >= .5f;
        checkpoint.values.m_bFalling = !checkpoint.values.m_bOnGround;
        checkpoint.values.m_fVelMagnitude = std::hypot(body.velocity.x, body.velocity.z);
        const auto original_manager = manager_.checkpoint();
        const auto original_prediction = movement_.prediction();
        auto updated = reports_;
        manager_.restore(checkpoint);
        if (!movement_.restore_prediction(corrected))
        {
            manager_.restore(original_manager);
            ++deferred;
            return Reception::Deferred;
        }
        std::uint64_t replayed = 0;
        for (const auto& frame : frames_)
        {
            if (frame.id <= found->frame)
                continue;
            if (!manager_.Update(frame.external_camera, frame.input, frame.yaw, frame.seconds,
                                 frame.commands && setup_.can_move, frame.firing, frame.pitch))
            {
                manager_.restore(original_manager);
                movement_.restore_prediction(original_prediction);
                ++deferred;
                return Reception::Deferred;
            }
            ++replayed;
            for (auto& later : updated)
                if (later.frame == frame.id)
                {
                    later.manager = manager_.checkpoint();
                    later.prediction = movement_.prediction();
                }
        }
        const auto accepted_frame = found->frame;
        acknowledged_ = ack.sequence;
        reports_ = std::move(updated);
        while (!reports_.empty() && reports_.front().report <= acknowledged_)
            reports_.pop_front();
        while (!frames_.empty() && frames_.front().id <= accepted_frame)
            frames_.pop_front();
        movement_.state.acknowledgements = original_prediction.state.acknowledgements + 1;
        ++corrections;
        replays += replayed;
        return Reception::Applied;
    }
} // namespace OpenBlood2
