#include "game/local_movement.h"
#include "game/server_adapter.h"

namespace OpenBlood2
{
    namespace
    {
        ClientMotionState client_state(const MovementState& state, const Objects& objects)
        {
            ClientMotionState result;
            static_cast<MovementValues&>(result) = state;
            if (const auto* ground = objects.get(state.ground_object); ground && ground->life == Life::Alive)
                result.ground_object = ground->id;
            return result;
        }
        bool same(DVector a, DVector b)
        {
            return a.x == b.x && a.y == b.y && a.z == b.z;
        }
        bool same(const PlayerMove& a, const PlayerMove& b)
        {
            return a.move_code == b.move_code && same(a.position, b.position) && same(a.velocity, b.velocity) &&
                   same(a.acceleration, b.acceleration) && a.on_ground == b.on_ground && a.surface == b.surface;
        }
    } // namespace
    void LocalMovement::clear()
    {
        client_.movement = {};
        connection_ = nullptr;
        object_ = nullptr;
        force_revision_ = 0;
        // Keep sequence monotonic across rebinds in this world, even to the same player.
        pending_ = {};
        pending = false;
    }
    bool LocalMovement::bind(HCLIENT connection)
    {
        clear();
        auto object = server_.GetClientUserData(connection);
        const auto* record = objects_.get(object);
        if (!record || record->life != Life::Alive || !dynamic_cast<const CPlayerObj*>(record->game.get()))
            return false;
        connection_ = connection;
        object_ = object;
        client_.movement.owner = client_.world.reference(record->id);
        if (synchronize())
            return true;
        clear();
        return false;
    }
    bool LocalMovement::valid() const
    {
        const auto* record = objects_.get(object_);
        return record && record->life == Life::Alive && server_.GetClientUserData(connection_) == object_ &&
               client_.movement.owner == client_.world.reference(record->id);
    }
    bool LocalMovement::synchronize()
    {
        MovementState state;
        if (!valid() || !server_.GetMovementState(object_, state))
            return false;
        client_.movement.body = client_state(state, objects_);
        return true;
    }
    bool LocalMovement::force_position(const PlayerPosition& value)
    {
        if (!valid() || !physics_vector(value.position))
            return false;
        const auto* player = dynamic_cast<const CPlayerObj*>(objects_.get(object_)->game.get());
        if (!player)
            return false;
        // Preserve the solo source ordering: the server already teleported. Reset
        // motion once here and correct the local body, without replaying that move.
        DVector zero;
        if (server_.SetVelocity(object_, &zero) != DE_OK || server_.SetAcceleration(object_, &zero) != DE_OK ||
            !synchronize())
            return false;
        force_revision_ = player->movement_revision;
        pending = false;
        return true;
    }
    bool LocalMovement::controls(CBloodServerShell& shell, const PlayerControls& value, bool send, bool& crouched)
    {
        if (!valid() || (send && !shell.OnMessage(connection_, value)))
            return false;
        const auto* player = shell.Player(connection_);
        if (!player)
            return false;
        crouched = player->crouched;
        return true;
    }
    bool LocalMovement::environment(CBloodServerShell& shell, ContainerState& state, int& character)
    {
        auto* player = valid() ? shell.Player(connection_) : nullptr;
        if (!player || !player->UpdateContainerState())
            return false;
        state = player->containers;
        character = player->m_nCharacter;
        return true;
    }
    bool LocalMovement::finish(CBloodServerShell& shell, float seconds, ContainerState& state)
    {
        auto* player = valid() ? shell.Player(connection_) : nullptr;
        if (!player || !player->UpdateAirLevel(seconds))
            return false;
        state = player->containers;
        return true;
    }
    bool LocalMovement::jump_voice(CVoiceMgr& voices, int character, bool external_camera)
    {
        return valid() && voices.PlayEventSound(object_, character, 11, external_camera);
    }
    bool LocalMovement::predict(const PlayerMove& move, LocalMoveReport& report)
    {
        report = {};
        if (!valid() || pending || next_sequence_ == UINT64_MAX)
        {
            ++client_.movement.rejected;
            return false;
        }
        const auto* player = dynamic_cast<const CPlayerObj*>(objects_.get(object_)->game.get());
        ClientMotionState predicted;
        if (!player || player->movement_revision != force_revision_ || move.move_code != player->m_byClientMoveCode ||
            !physics_vector(move.position) || !physics_vector(move.velocity) || !physics_vector(move.acceleration))
        {
            ++client_.movement.rejected;
            return false;
        }
        pending_ = {client_.movement.owner, force_revision_, ++next_sequence_, move};
        pending = true;
        report = pending_;
        if (server_.PredictPlayerMovement(object_, body(), move, predicted))
            body() = predicted;
        else
            // The authoritative resolver still receives a bounded valid command:
            // preserve its original failure diagnostics and deferred contact semantics.
            ++client_.movement.prediction_failures;
        ++client_.movement.predictions;
        return true;
    }
    bool LocalMovement::submit(CBloodServerShell& shell, const LocalMoveReport& report)
    {
        const auto* player = valid() ? shell.Player(connection_) : nullptr;
        const bool accept = player && pending && report.owner == client_.movement.owner &&
                            report.owner == pending_.owner && report.force_revision == player->movement_revision &&
                            report.force_revision == pending_.force_revision && report.sequence == pending_.sequence &&
                            same(report.move, pending_.move);
        pending = false;
        if (!accept)
        {
            ++client_.movement.rejected;
            return false;
        }
        // Collision validation + gameplay contacts only. PlayerMove already contains
        // CMoveMgr's integrated destination; the server must never integrate dt again.
        if (!shell.OnMessage(connection_, report.move) || !synchronize())
            return false;
        ++client_.movement.acknowledgements;
        return true;
    }
} // namespace OpenBlood2
