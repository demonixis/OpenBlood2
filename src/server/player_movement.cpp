#include "server/session.h"
#include <algorithm>
#include <cmath>

namespace OpenBlood2
{
    namespace
    {
        float magnitude(DVector v)
        {
            return std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
        }
        DVector add(DVector a, DVector b)
        {
            return {a.x + b.x, a.y + b.y, a.z + b.z};
        }
        bool same(DVector a, DVector b)
        {
            return a.x == b.x && a.y == b.y && a.z == b.z;
        }
        bool same_setup(const Net::MovementSetup& a, const Net::MovementSetup& b)
        {
            return a.owner == b.owner && a.code == b.code && a.character == b.character && a.crouched == b.crouched &&
                   a.can_move == b.can_move && a.eye_level == b.eye_level && same(a.dimensions, b.dimensions) &&
                   same(a.gravity, b.gravity) && a.move_speed == b.move_speed && a.jump_speed == b.jump_speed;
        }
    } // namespace
    bool ServerSession::refresh_movement(Player& p, std::string& error)
    {
        if (!p.failure.empty())
        {
            error = p.failure;
            return false;
        }
        auto* player = game_.server_shell.Player(p.client);
        const auto* object = game_.objects.get(p.object);
        if (!player || !object || object->life != Life::Alive || player->m_hObject != object->handle)
        {
            error = "Remote movement owner is unavailable";
            return false;
        }
        Net::CameraState camera;
        if (!refresh_camera(p, camera, error))
            return false;
        Net::MovementSetup next;
        const bool force = !p.setup.sequence || p.source_revision != player->movement_revision ||
                           p.pose_revision != object->pose_revision;
        if (force && p.setup.owner.force_revision == UINT64_MAX)
        {
            error = "Network movement force identity exhausted";
            return false;
        }
        next.owner = {world_id_, p.id, p.object, p.setup.owner.force_revision + std::uint64_t(force)};
        next.sequence = p.setup.sequence;
        next.code = player->m_byClientMoveCode;
        next.character = player->m_nCharacter;
        next.crouched = player->crouched;
        next.can_move = !player->m_damage.IsDead() && !player->MovementBlocked() && !player->camera_movement_blocked;
        next.eye_level = player->eye_level;
        next.position = object->state.m_Pos;
        next.dimensions = object->dimensions;
        next.gravity = game_.server.gravity;
        next.move_speed = player->m_fMoveVel;
        next.jump_speed = player->m_fJumpVel;
        if (!same_setup(p.setup, next))
        {
            if (next.sequence == UINT64_MAX)
            {
                error = "Movement setup sequence exhausted";
                return false;
            }
            ++next.sequence;
            if (!Net::valid_movement(next))
            {
                error = "Invalid original player physics for remote movement";
                return false;
            }
            if (next.owner.force_revision != p.setup.owner.force_revision)
            {
                // Original client force-position response resets motion once.
                // Remote reports of a prior life/teleport cannot restore it.
                DVector zero;
                if (game_.server.SetVelocity(object->handle, &zero) != DE_OK ||
                    game_.server.SetAcceleration(object->handle, &zero) != DE_OK)
                    return false;
                movement_ignored += p.pending.size();
                p.pending.clear();
                p.ack.reset();
                // Impulses belong to one force/life: the zeroed body drops them.
                p.impulses.clear();
                p.impulse_sequence = p.impulse_received = 0;
                player->m_damage.added_velocity = {};
                player->m_damage.added_velocity_pending = false;
                // The client snaps to this pose: its next report starts here.
                p.applied_sequence = 0;
            }
            p.setup = next;
            p.source_revision = player->movement_revision;
            p.pose_revision = object->pose_revision;
        }
        camera.force_revision = p.setup.owner.force_revision;
        if (!p.camera || *p.camera != camera)
        {
            // Append after original private events (including PlayerDead). A
            // changed force publishes even when death/respawn coalesced to alive.
            if (queue_private(p, p.object, camera) != DE_OK)
            {
                error = "Private camera state queue refused";
                return false;
            }
            p.camera = camera;
        }
        return true;
    }
    bool ServerSession::movement_setup(std::uint64_t id, Net::MovementSetup& out, std::string& error)
    {
        auto it = std::find_if(players_.begin(), players_.end(), [&](const auto& p) { return p.id == id; });
        if (it == players_.end() || !refresh_movement(*it, error))
            return false;
        out = it->setup;
        return true;
    }
    bool ServerSession::collect_impulses(Player& p, std::string& error)
    {
        auto* player = game_.server_shell.Player(p.client);
        if (!player)
        {
            error = "Remote movement owner is unavailable";
            return false;
        }
        auto& damage = player->m_damage;
        if (!damage.added_velocity_pending && !player->door_push_pending)
            return true;
        // Damage impulses and door pushes change the server body directly (the
        // original PSTATE_ADDVELOCITY); the client adds the same delta to its
        // prediction, and older reports' steps receive it on replay.
        auto velocity = damage.added_velocity;
        if (player->door_push_pending)
        {
            velocity = add(velocity, player->door_push_velocity);
            if (!player->ApplyDoorPush())
            {
                error = "Remote player door push failed";
                return false;
            }
        }
        damage.added_velocity = {};
        damage.added_velocity_pending = false;
        Net::MovementImpulse value{p.setup.owner, p.impulse_sequence + 1, velocity};
        if (p.impulse_sequence == UINT64_MAX || p.impulses.size() >= Net::movement_impulse_limit ||
            !Net::valid_movement(value))
        {
            error = "Remote movement impulse capacity or velocity refused";
            return false;
        }
        p.impulses.push_back(value);
        p.impulse_sequence = value.sequence;
        return true;
    }
    bool ServerSession::movement_impulses(std::uint64_t id, std::uint64_t after, std::vector<Net::MovementImpulse>& out,
                                          std::string& error)
    {
        out.clear();
        const auto it = std::find_if(players_.begin(), players_.end(), [&](const auto& p) { return p.id == id; });
        if (it == players_.end() || !refresh_movement(*it, error) || !collect_impulses(*it, error))
            return false;
        if (after > it->impulse_sequence)
        {
            error = "Movement impulse cursor is ahead of the server";
            return false;
        }
        for (const auto& value : it->impulses)
            if (value.sequence > after)
                out.push_back(value);
        return true;
    }
    bool ServerSession::movement_ack(std::uint64_t id, std::optional<Net::MovementAck>& out) const
    {
        auto it = std::find_if(players_.begin(), players_.end(), [&](const auto& p) { return p.id == id; });
        if (it == players_.end())
            return false;
        out = it->ack;
        return true;
    }
    ServerSession::MoveResult ServerSession::queue_movement(std::uint64_t id, const Net::MovementReport& value)
    {
        auto it = std::find_if(players_.begin(), players_.end(), [&](const auto& p) { return p.id == id; });
        if (it == players_.end() || !Net::valid_movement(value) || value.owner.player != id)
            return MoveResult::Invalid;
        auto* player = game_.server_shell.Player(it->client);
        const auto* object = game_.objects.get(it->object);
        if (!it->failure.empty() || !player || !object || value.owner.world != world_id_ ||
            value.owner.object != it->object || value.owner.force_revision != it->setup.owner.force_revision ||
            it->source_revision != player->movement_revision || it->pose_revision != object->pose_revision ||
            value.move.move_code != player->m_byClientMoveCode || value.sequence <= it->received_sequence)
        {
            ++movement_ignored;
            return MoveResult::Ignored;
        }
        // A report acknowledges impulses the client applied: never one not yet
        // created, never fewer than an earlier report.
        if (value.impulse_sequence > it->impulse_sequence || value.impulse_sequence < it->impulse_received)
            return MoveResult::Invalid;
        it->impulse_received = value.impulse_sequence;
        // Every report carries distinct substeps: keep them in order. Dropping
        // the oldest leaves a sequence gap, which the next replay catches up.
        if (it->pending.size() >= movement_queue_limit)
        {
            it->pending.pop_front();
            ++movement_dropped;
        }
        it->pending.push_back(value);
        it->received_sequence = value.sequence;
        it->received_tick = ticks_;
        return MoveResult::Queued;
    }
    bool ServerSession::replay_movement(Player& p, const Net::MovementReport& value, std::string& error)
    {
        auto* player = game_.server_shell.Player(p.client);
        const auto* object = game_.objects.get(p.object);
        if (!player || !object)
        {
            error = "Remote movement owner is unavailable";
            return false;
        }
        const bool gap = p.applied_sequence && value.sequence != p.applied_sequence + 1;
        p.applied_sequence = value.sequence;
        // Movement gates neutralize controls, while gravity and existing impulses
        // remain represented by the client-integrated steps. A dead body keeps
        // the server's own motion.
        if (player->m_damage.IsDead() || value.steps.empty())
            return true;
        // Velocity the client had not integrated yet: impulses created after
        // this report's acknowledged one (including impulses from this replay).
        auto missing = [&]
        {
            DVector delta{};
            for (const auto& impulse : p.impulses)
                if (impulse.sequence > value.impulse_sequence)
                    delta = add(delta, impulse.velocity);
            return delta;
        };
        auto submit = [&](DVector destination, Net::MovementStep step)
        {
            // The solo LocalMovement submission: an integrated destination and
            // pre-collision velocity; only the swept solver and contacts run here,
            // so stairs, impacts and falling damage follow the solo rules.
            if (game_.server_shell.OnMessage(p.client, PlayerMove{value.move.move_code, destination, step.velocity,
                                                                  step.acceleration, step.on_ground, step.surface}))
                return true;
            error = "Remote player position collision failed";
            return false;
        };
        MovementState state;
        if (!game_.server.GetMovementState(object->handle, state))
        {
            error = "Remote movement state is unavailable";
            return false;
        }
        if (gap)
        {
            // A lost or reordered report: one collision-checked sweep to where
            // the client says these steps start, charged at the speed limit.
            const auto distance = magnitude({value.origin.x - state.position.x, value.origin.y - state.position.y,
                                             value.origin.z - state.position.z});
            const double cost = distance / Net::movement_speed_limit;
            if (distance > 0 && cost <= p.movement_budget)
            {
                p.movement_budget -= cost;
                if (!submit(value.origin, value.steps.front()))
                    return false;
                ++movement_catchups;
            }
            else if (distance > 0)
                ++movement_leashed;
        }
        for (auto step : value.steps)
        {
            if (!collect_impulses(p, error))
                return false;
            const auto delta = missing();
            step.velocity = add(step.velocity, delta);
            step.offset = add(step.offset, {delta.x * step.seconds, delta.y * step.seconds, delta.z * step.seconds});
            // Never faster than real time nor than the speed limit; the
            // acknowledgement corrects the client to the server's result.
            if (step.seconds > p.movement_budget ||
                magnitude(step.offset) > Net::movement_speed_limit * step.seconds + 1e-3f)
            {
                ++movement_leashed;
                break;
            }
            p.movement_budget -= step.seconds;
            if (!game_.server.GetMovementState(object->handle, state))
            {
                error = "Remote movement state is unavailable";
                return false;
            }
            if (!submit({state.position.x + step.offset.x, state.position.y + step.offset.y,
                         state.position.z + step.offset.z},
                        step) ||
                !refresh_movement(p, error))
                return false;
            ++movement_steps;
            // A contact can teleport or kill the player: later steps belong to
            // the previous pose or life.
            if (p.setup.owner != value.owner || player->m_damage.IsDead())
                break;
        }
        return true;
    }
    bool ServerSession::advance_player(Player& p, double seconds, std::string& error)
    {
        if (!refresh_movement(p, error))
            return false;
        auto* player = game_.server_shell.Player(p.client);
        auto* object = game_.objects.get(p.object);
        if (!player || !object)
        {
            error = "Remote movement owner is unavailable";
            return false;
        }
        p.movement_budget = std::min(movement_budget_limit, p.movement_budget + seconds * movement_budget_rate);
        if (!collect_impulses(p, error))
            return false;
        if (!p.pending.empty())
        {
            p.controls = p.pending.back().controls;
            p.aim = {p.pending.back().pitch, p.pending.back().controls.yaw};
        }
        // Original server animation/clearance runs every simulation tick,
        // retaining the last controls between reports. It never integrates
        // movement commands; only the client runs CMoveMgr.
        if ((!p.pending.empty() || p.ack) && !player->m_damage.IsDead())
        {
            const bool active = p.setup.can_move && ticks_ - p.received_tick <= 30;
            const bool look = p.camera && (!p.camera->scripted || p.camera->eye || p.camera->orb);
            const auto aim = look ? p.aim : PlayerRotation{player->m_fPitch, player->m_fYaw};
            auto controls = active ? p.controls : PlayerControls{{}, false, aim.yaw};
            controls.yaw = aim.yaw;
            if (!game_.server_shell.OnMessage(p.client, controls) || !game_.server_shell.OnMessage(p.client, aim) ||
                !refresh_movement(p, error))
            {
                if (error.empty())
                    error = "Remote player controls failed";
                return false;
            }
        }
        std::optional<Net::MovementReport> applied;
        while (!p.pending.empty())
        {
            auto value = std::move(p.pending.front());
            p.pending.pop_front();
            if (value.owner != p.setup.owner)
            {
                ++movement_ignored;
                continue;
            }
            if (!replay_movement(p, value, error) || !refresh_movement(p, error))
                return false;
            ++movement_reports;
            applied = std::move(value);
        }
        // A contact that forced a new pose reset the acknowledgement already.
        if (applied && applied->owner != p.setup.owner)
            applied.reset();
        if (!collect_impulses(p, error))
            return false;
        if (applied)
        {
            // The client integrates every impulse up to the one this report
            // acknowledges: later reports never need those deltas.
            while (!p.impulses.empty() && p.impulses.front().sequence <= applied->impulse_sequence)
                p.impulses.pop_front();
            MovementState actual;
            if (!game_.server.GetMovementState(object->handle, actual))
            {
                error = "Remote movement state is unavailable";
                return false;
            }
            p.ack = Net::MovementAck{p.setup.owner,
                                     applied->sequence,
                                     p.setup.sequence,
                                     {p.setup.code, actual.position, actual.velocity, actual.acceleration,
                                      actual.standing && actual.ground_normal.y >= .5f, applied->move.surface},
                                     actual.dimensions,
                                     p.impulse_sequence};
        }
        else if (p.ack && ticks_ - p.received_tick > 30)
        {
            // Reports stopped: the server never extrapolates a remote body (that
            // would dispatch contacts along a path the player may not take).
            DVector zero;
            if (game_.server.SetVelocity(object->handle, &zero) != DE_OK)
            {
                error = "Remote movement hold failed";
                return false;
            }
            ++movement_holds;
        }
        if (!player->UpdateAirLevel(static_cast<float>(seconds)) ||
            (p.ack && !player->m_InventoryMgr.UpdateHandModels()))
        {
            error = "Remote player air or hand-model update failed";
            return false;
        }
        return true;
    }
    bool ServerSession::advance_players(double seconds, std::string& error)
    {
        for (auto& p : players_)
        {
            if (!p.failure.empty())
                continue;
            std::string failure;
            if (advance_player(p, seconds, failure))
                continue;
            // Isolate the failing player, as solo removes only its own player:
            // the endpoint disconnects this peer and the match keeps running.
            p.failure = failure.empty() ? "Remote player movement failed" : failure;
            p.pending.clear();
            p.ack.reset();
            ++movement_failures;
            warnings_.push_back("Remote player " + std::to_string(p.id) + " removed: " + p.failure);
        }
        (void)error;
        return true;
    }
} // namespace OpenBlood2
