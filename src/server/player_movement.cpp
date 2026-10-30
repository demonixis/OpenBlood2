#include "server/session.h"
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
        bool same_setup(const Net::MovementSetup& a, const Net::MovementSetup& b)
        {
            return a.owner == b.owner && a.code == b.code && a.character == b.character && a.crouched == b.crouched &&
                   a.can_move == b.can_move && a.eye_level == b.eye_level && same(a.dimensions, b.dimensions) &&
                   same(a.gravity, b.gravity) && a.move_speed == b.move_speed && a.jump_speed == b.jump_speed;
        }
    } // namespace
    bool ServerSession::refresh_movement(Player& p, std::string& error)
    {
        auto* player = game_.server_shell.Player(p.client);
        const auto* object = game_.objects.get(p.object);
        if (!player || !object || object->life != Life::Alive || player->m_hObject != object->handle)
        {
            error = "Remote movement owner is unavailable";
            return false;
        }
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
                movement_ignored += p.pending.has_value();
                p.pending.reset();
                p.ack.reset();
            }
            p.setup = next;
            p.source_revision = player->movement_revision;
            p.pose_revision = object->pose_revision;
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
        if (!player || !object || value.owner.world != world_id_ || value.owner.object != it->object ||
            value.owner.force_revision != it->setup.owner.force_revision ||
            it->source_revision != player->movement_revision || it->pose_revision != object->pose_revision ||
            value.move.move_code != player->m_byClientMoveCode || value.sequence <= it->received_sequence)
        {
            ++movement_ignored;
            return MoveResult::Ignored;
        }
        movement_coalesced += it->pending.has_value();
        it->pending = value;
        it->received_sequence = value.sequence;
        it->received_tick = ticks_;
        return MoveResult::Queued;
    }
    bool ServerSession::advance_players(double seconds, std::string& error)
    {
        for (auto& p : players_)
        {
            if (!refresh_movement(p, error))
                return false;
            auto* player = game_.server_shell.Player(p.client);
            auto* object = game_.objects.get(p.object);
            if (!player || !object)
                return false;
            if (p.pending)
            {
                p.controls = p.pending->controls;
                p.aim = {p.pending->pitch, p.pending->controls.yaw};
            }
            // Original server animation/clearance runs every simulation tick,
            // retaining the last controls between reports. It never integrates
            // movement commands; only the client runs CMoveMgr.
            if ((p.pending || p.ack) && !player->m_damage.IsDead())
            {
                const bool active = p.setup.can_move && ticks_ - p.received_tick <= 30;
                const auto controls = active ? p.controls : PlayerControls{{}, false, p.aim.yaw};
                if (!game_.server_shell.OnMessage(p.client, controls) ||
                    !game_.server_shell.OnMessage(p.client, p.aim) || !refresh_movement(p, error))
                {
                    if (error.empty())
                        error = "Remote player controls failed";
                    return false;
                }
            }
            if (p.pending)
            {
                const auto value = *p.pending;
                p.pending.reset();
                // Movement gates neutralize controls, while gravity and existing
                // impulses remain represented by the client-computed destination.
                if (!player->m_damage.IsDead() && !game_.server_shell.OnMessage(p.client, value.move))
                {
                    error = "Remote player position collision failed";
                    return false;
                }
                if (!refresh_movement(p, error))
                    return false;
                if (p.setup.owner == value.owner)
                {
                    if (!player->ApplyDoorPush())
                        return false;
                    MovementState actual;
                    if (!game_.server.GetMovementState(object->handle, actual))
                        return false;
                    p.ack = Net::MovementAck{p.setup.owner,
                                             value.sequence,
                                             p.setup.sequence,
                                             {p.setup.code, actual.position, actual.velocity, actual.acceleration,
                                              actual.standing && actual.ground_normal.y >= .5f, value.move.surface},
                                             actual.dimensions};
                }
                ++movement_reports;
                // This report already represents the client's integrated frame.
                // Only intervening ticks advance received velocity.
            }
            else if (p.ack && ticks_ - p.received_tick <= 30 && !player->m_damage.IsDead())
            {
                if (!player->ApplyDoorPush())
                    return false;
                const auto count = static_cast<unsigned>(std::max(1.0, std::ceil(seconds * 120)));
                const float dt = static_cast<float>(seconds / count);
                for (unsigned i = 0; i < count; ++i)
                {
                    MovementState state;
                    if (!game_.server.GetMovementState(object->handle, state) ||
                        !game_.server.SetBlockingPriority(object->handle, state.standing ? 50 : 40))
                        return false;
                    auto destination = vector_add(state.position, vector_scale(state.velocity, dt));
                    if (game_.server.MoveObject(object->handle, &destination) != DE_OK)
                        return false;
                    ++movement_steps;
                }
            }
            else if (p.ack)
            {
                // Bound extrapolation after lost reports; heartbeats alone do
                // not keep an old movement command running until timeout.
                DVector zero;
                if (game_.server.SetVelocity(object->handle, &zero) != DE_OK)
                    return false;
                ++movement_holds;
            }
            if (!player->UpdateAirLevel(static_cast<float>(seconds)))
                return false;
            if (p.ack && !player->m_InventoryMgr.UpdateHandModels())
                return false;
        }
        return true;
    }
} // namespace OpenBlood2
