#include "game/local_movement.h"
#include "game/original/SeeingEye.h"
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
        shapes_.clear();
        primary_.reset();
        primary_source_ = nullptr;
        collision_world_ = 0;
        collision_error.clear();
        primary_copies = shape_copies = 0;
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
    bool LocalMovement::environment(CBloodServerShell& shell, ClientContainerState& state, int& character)
    {
        auto* player = valid() ? shell.Player(connection_) : nullptr;
        if (!player || !publish_collision())
            return false;
        auto head = client_.movement.body.position;
        head.y += player->eye_level;
        character = player->m_nCharacter;
        return client_.collision.environment(client_.movement.owner, client_.movement.body, head, state);
    }
    bool LocalMovement::finish(CBloodServerShell& shell, float seconds, ClientContainerState& state)
    {
        // Capture membership before the authoritative air/damage update, which
        // can change the player's life. Client queries never run gameplay callbacks.
        int character = 0;
        if (!environment(shell, state, character))
            return false;
        auto* player = shell.Player(connection_);
        return player && player->UpdateAirLevel(seconds);
    }
    bool LocalMovement::jump_voice(CVoiceMgr& voices, int character, bool external_camera)
    {
        return valid() && voices.PlayEventSound(object_, character, 11, external_camera);
    }
    bool LocalMovement::publish_collision()
    {
        collision_error.clear();
        if (objects_.busy() || !server_.collision || !server_.collision_ready)
        {
            collision_error = "Collision publication requires a stable loaded world";
            return false;
        }
        const auto world = client_.world.frame().world;
        if (collision_world_ != world)
        {
            shapes_.clear();
            primary_.reset();
            collision_world_ = world;
        }
        if (!primary_ || primary_source_ != server_.collision || primary_->revision() != server_.collision->revision())
        {
            primary_ = std::make_shared<const CollisionWorld>(*server_.collision);
            primary_source_ = server_.collision;
            ++primary_copies;
        }
        ClientCollisionFrame frame;
        frame.world = world;
        frame.sequence = client_.collision.frame().sequence + 1;
        frame.primary = primary_;
        const auto ids = objects_.snapshot();
        frame.objects.reserve(ids.size());
        for (auto id : ids)
        {
            const auto* object = objects_.get(id);
            if (!object || object->life != Life::Alive)
                continue;
            ClientCollider value;
            value.id = id;
            value.position = object->state.m_Pos;
            value.dimensions = object->dimensions;
            value.velocity = object->velocity;
            value.acceleration = object->acceleration;
            value.flags = object->state.m_Flags;
            value.user_flags = object->user_flags;
            value.type = object->state.m_ObjectType;
            value.attached = object->attachment != 0;
            value.world_model = bool(object->world_model);
            value.container = bool(object->container());
            value.dynamic = object->world_body || object->model_body;
            value.pushable = object->body_properties.pushable;
            value.priority = object->blocking_priority;
            if (value.container)
            {
                ContainerInfo info;
                if (container_info(*object, info))
                    value.environment = {info.code, info.current, info.gravity, info.viscosity, info.hidden};
            }
            if (const auto* eye = dynamic_cast<const SeeingEye*>(object->game.get()); eye && !eye->dropped)
                if (const auto* owner = objects_.get(eye->m_hFiredFrom); owner && owner->life == Life::Alive)
                    value.ignored_by = owner->id;
            const auto& shape = object->world_model ? object->world_model : object->container();
            if (shape)
            {
                auto& cached = shapes_[id.slot];
                if (cached.id != id || cached.source.lock() != shape)
                {
                    cached = {id, shape, std::make_shared<const ContainerShape>(*shape)};
                    ++shape_copies;
                }
                value.shape = cached.copy;
            }
            frame.objects.push_back(std::move(value));
        }
        // Cache lifetime follows current membership, not all objects ever observed.
        for (auto it = shapes_.begin(); it != shapes_.end();)
        {
            const auto* object = objects_.get(it->second.id);
            if (!object || object->life != Life::Alive || (!object->world_model && !object->container()))
                it = shapes_.erase(it);
            else
                ++it;
        }
        return client_.collision.apply(std::move(frame), collision_error);
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
        if (publish_collision() && client_.collision.predict(client_.movement.owner, body(), move.position,
                                                             move.velocity, move.acceleration, predicted))
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
