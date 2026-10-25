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
        if (mover->world_body)
            return MoverResult::Unavailable; // Dynamic bodies use collision-clipped MoveObject.
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
            DVector position, origin;
            bool rider;
        };
        std::vector<Push> pushes;
        auto blocked = [&](HOBJECT blocker)
        {
            ++mover->mover_blocks;
            mover->mover_blocker = blocker;
            return MoverResult::Blocked;
        };
        // Doors retain their source world bypass for wall pockets. Authored
        // BoxPhysics path movers instead sweep their actual bounds against the
        // primary world. Polygon-vs-world translation remains unavailable.
        if (!(mover->state.m_Flags & FLAG_GOTHRUWORLD))
        {
            if (!(mover->state.m_Flags & FLAG_BOXPHYSICS))
                return MoverResult::Unavailable;
            CollisionHit hit;
            const auto center = object_bounds_center(*mover);
            const auto query = collision->sweep(center, vector_add(center, delta), mover->dimensions, hit);
            if (query == CollisionQuery::Invalid)
                return MoverResult::Invalid;
            if (query == CollisionQuery::Hit && (hit.start_solid || hit.fraction < 1 - 1e-6))
                return blocked(nullptr); // Primary world has no object handle.
        }
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
                    const auto query =
                        sweep_object(*body, center, vector_add(center, delta), mover->dimensions, contact);
                    if (query == CollisionQuery::Invalid)
                        return MoverResult::Invalid;
                    // Adjacent leaves must be able to meet at their closed endpoints.
                    // Reject initial penetration and every contact before the endpoint;
                    // exact end contact is not a move through the other brush.
                    if (query == CollisionQuery::Hit && (contact.start_solid || contact.fraction < 1))
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
                pushes.push_back({body, end, body->state.m_Pos, rider});
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
        // Sensors crossed by transported bodies are queried on the accepted
        // path, after all positions commit. No callbacks on a blocked plan.
        for (const auto& push : pushes)
            for (auto id : ids)
            {
                auto* sensor = objects_.get(id);
                auto& body = *push.body;
                if (body.life != Life::Alive)
                    break;
                if (!sensor || sensor->life != Life::Alive || sensor->handle == body.handle || sensor->container() ||
                    sensor->world_model || (sensor->state.m_Flags & FLAG_SOLID) ||
                    !(sensor->state.m_Flags & FLAG_TOUCH_NOTIFY))
                    continue;
                CollisionHit hit;
                if (sweep_object(*sensor, push.origin, push.position, body.dimensions, hit) == CollisionQuery::Hit)
                {
                    if (body.state.m_Flags & FLAG_TOUCH_NOTIFY)
                        objects_.engine_message(body.handle, MID_TOUCHNOTIFY, sensor->handle);
                    if (body.life == Life::Alive && sensor->life == Life::Alive)
                        objects_.engine_message(sensor->handle, MID_TOUCHNOTIFY, body.handle);
                }
            }
        moving_ = false;
        return MoverResult::Moved;
    }
} // namespace OpenBlood2
