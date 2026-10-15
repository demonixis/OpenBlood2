#include "engine/world_model.h"
#include "game/server_adapter.h"
#include <cmath>

namespace OpenBlood2
{
    MoverResult ServerAdapter::MoveWorldModel(HOBJECT handle, DVector destination)
    {
        trace_service("MoveWorldModel");
        auto* mover = objects_.get(handle);
        if (!mover || mover->life != Life::Alive || !mover->world_model || !physics_vector(destination) || moving_)
            return MoverResult::Invalid;
        if (!collision_ready || !collision)
            return MoverResult::Unavailable;
        const auto delta = vector_subtract(destination, mover->state.m_Pos);
        if (!physics_vector(delta))
            return MoverResult::Invalid;
        if (vector_dot(delta, delta) == 0)
            return MoverResult::Moved;
        const auto ids = objects_.snapshot();
        struct Push
        {
            ObjectRecord* body;
            DVector position;
            bool rider;
        };
        std::vector<Push> pushes;
        auto blocked = [&](HOBJECT blocker)
        {
            ++mover->mover_blocks;
            mover->mover_blocker = blocker;
            return MoverResult::Blocked;
        };
        // Door uses GOTHRUWORLD to enter its wall pocket. Other callers must
        // explicitly opt in; no guessed world-model/world depenetration.
        if (!(mover->state.m_Flags & FLAG_GOTHRUWORLD))
            return MoverResult::Unavailable;
        if (mover->state.m_Flags & FLAG_SOLID)
        {
            for (auto id : ids)
            {
                auto* body = objects_.get(id);
                if (!body || body->handle == handle || body->life != Life::Alive || !(body->state.m_Flags & FLAG_SOLID))
                    continue;
                CollisionHit contact;
                if (body->world_model)
                {
                    // Other movers are immovable in this bounded solver. A
                    // conservative swept bound avoids tunnelling mesh pairs.
                    const auto center = object_bounds_center(*mover);
                    if (sweep_object(*body, center, vector_add(center, delta), mover->dimensions, contact) !=
                        CollisionQuery::Miss)
                        return blocked(body->handle);
                    continue;
                }
                MovementState support;
                const bool rider = !(body->state.m_Flags & FLAG_DONTFOLLOWSTANDING) &&
                                   GetMovementState(body->handle, support) && support.standing &&
                                   support.ground_object == handle;
                const auto query = sweep_object(*mover, body->state.m_Pos, vector_subtract(body->state.m_Pos, delta),
                                                body->dimensions, contact);
                if (query == CollisionQuery::Invalid || (query == CollisionQuery::Hit && contact.start_solid))
                    return blocked(body->handle);
                if (!rider && query != CollisionQuery::Hit)
                    continue;
                if (body->state.m_ObjectType != OT_NORMAL && body->state.m_ObjectType != OT_MODEL)
                    return blocked(body->handle);
                if (pushes.size() == 64)
                {
                    objects_.fail(ObjectError::Limit, "World-model transaction exceeds 64 pushed bodies");
                    return blocked(body->handle);
                }
                auto displacement = rider ? delta : vector_scale(delta, float(1 - contact.fraction));
                if (!rider)
                    displacement = vector_add(displacement, vector_scale(contact.normal, .002f));
                const auto end = vector_add(body->state.m_Pos, displacement);
                if (!physics_vector(end))
                    return MoverResult::Invalid;
                // Plan every move before mutating any position. No recursive
                // chain pushing, crushing damage or speculative touch callbacks.
                CollisionHit obstruction;
                const auto result =
                    sweep_scene(body->handle, body->state.m_Pos, end, body->dimensions, obstruction, ids, handle);
                if (result == CollisionQuery::Invalid ||
                    (result == CollisionQuery::Hit && (obstruction.start_solid || obstruction.fraction < 1 - 1e-6)))
                    return blocked(result == CollisionQuery::Hit ? obstruction.object : body->handle);
                pushes.push_back({body, end, rider});
            }
        }
        moving_ = true;
        mover->state.m_Pos = destination;
        ++mover->mover_moves;
        for (const auto& push : pushes)
        {
            auto& body = *push.body;
            body.state.m_Pos = push.position;
            body.grounded = false;
            body.contact_object = nullptr;
            body.contact_polygon = UINT32_MAX;
            body.contact_normal = {};
            ++mover->mover_pushes;
            mover->mover_riders += push.rider;
        }
        // A single atomic translation precedes original callbacks. Destruction
        // stays deferred, including when a callback removes this very mover.
        for (const auto& push : pushes)
        {
            auto& body = *push.body;
            if (mover->life != Life::Alive)
                break;
            if (body.life != Life::Alive)
                continue;
            if (mover->state.m_Flags & FLAG_TOUCH_NOTIFY)
                objects_.engine_message(handle, MID_TOUCHNOTIFY, body.handle);
            if (mover->life == Life::Alive && body.life == Life::Alive && (body.state.m_Flags & FLAG_TOUCH_NOTIFY))
                objects_.engine_message(body.handle, MID_TOUCHNOTIFY, handle);
        }
        moving_ = false;
        return MoverResult::Moved;
    }
} // namespace OpenBlood2
