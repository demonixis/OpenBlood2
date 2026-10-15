#include "game/server_adapter.h"
#include <algorithm>
#include <cmath>
#include <map>

namespace OpenBlood2
{
    CollisionQuery ServerAdapter::sweep_scene(HOBJECT ignore, DVector from, DVector to, DVector dims,
                                              CollisionHit& nearest, const std::vector<ObjectId>& ids)
    {
        auto result = collision->sweep(from, to, dims, nearest);
        if (result == CollisionQuery::Invalid)
            return result;
        for (auto id : ids)
        {
            const auto* other = objects_.get(id);
            if (!other || other->life != Life::Alive || other->handle == ignore || !(other->state.m_Flags & FLAG_SOLID))
                continue;
            CollisionHit hit;
            auto query = sweep_box(from, to, dims, other->state.m_Pos, other->dimensions, hit);
            if (query == CollisionQuery::Invalid)
                return query;
            if (query == CollisionQuery::Hit && (result != CollisionQuery::Hit || hit.fraction < nearest.fraction))
            {
                nearest = hit;
                nearest.object = other->handle;
                result = query;
            }
        }
        return result;
    }
    ResizeResult ServerAdapter::ResizeObject(HOBJECT h, DVector dimensions)
    {
        trace_service("ResizeObject");
        auto* r = objects_.get(h);
        if (!r || r->life != Life::Alive || r->container || !physics_vector(dimensions) || dimensions.x <= 0 ||
            dimensions.y <= 0 || dimensions.z <= 0 || !physics_vector(r->state.m_Pos) || moving_)
            return ResizeResult::Invalid;
        if (!collision || !collision_ready)
            return ResizeResult::Unavailable;
        // Local SetAnimation/ResetDims replacement. Preserve the feet and test
        // the entire requested shape before committing. No partial resize or
        // pushing other objects; a blocked clip keeps its position and clock.
        auto position = r->state.m_Pos;
        position.y += dimensions.y - r->dimensions.y;
        CollisionHit hit;
        const auto query = sweep_scene(h, position, position, dimensions, hit, objects_.snapshot());
        if (query == CollisionQuery::Invalid)
            return ResizeResult::Invalid;
        if (query == CollisionQuery::Hit)
            return ResizeResult::Blocked;
        r->dimensions = dimensions;
        r->state.m_Pos = position;
        return ResizeResult::Resized;
    }
    bool ServerAdapter::GetMovementState(HOBJECT h, MovementState& state)
    {
        trace_service("GetMovementState");
        state = {};
        const auto* r = objects_.get(h);
        if (!r || r->life != Life::Alive || !collision || !collision_ready)
            return false;
        state.position = r->state.m_Pos;
        state.velocity = r->velocity;
        state.acceleration = r->acceleration;
        state.dimensions = r->dimensions;
        state.stair_attempts = r->stair_attempts;
        state.stair_steps = r->stair_steps;
        state.last_step_height = r->last_step_height;
        if (!physics_vector(state.position) || !physics_vector(state.velocity) || !physics_vector(state.acceleration) ||
            !physics_vector(state.dimensions) || state.dimensions.x < 0 || state.dimensions.y < 0 ||
            state.dimensions.z < 0)
            return false;
        // A fresh short support sweep avoids a stale last-contact normal after
        // sliding along a wall or walking off a ledge. World support has a polygon
        // identity, not a fabricated HOBJECT. Motion away from the support plane
        // is airborne; positive Y alone can also mean walking uphill.
        auto to = state.position;
        to.y -= 0.02f;
        CollisionHit nearest;
        const auto query = sweep_scene(h, state.position, to, state.dimensions, nearest, objects_.snapshot());
        if (query == CollisionQuery::Invalid)
            return false;
        const bool found = query == CollisionQuery::Hit;
        if (found && !nearest.start_solid && nearest.normal.y > 0 &&
            vector_dot(state.velocity, nearest.normal) <= 0.01f)
        {
            state.standing = true;
            state.ground_normal = nearest.normal;
            state.ground_object = nearest.object;
            state.ground_polygon = nearest.polygon;
        }
        return true;
    }
    void ServerAdapter::initialize_physics_services()
    {
        TeleportObject = [](HOBJECT h, DVector* position) -> DRESULT
        {
            auto& a = active();
            a.trace_service("TeleportObject");
            auto* r = a.objects_.get(h);
            if (!r || r->life != Life::Alive || r->container || !position || !physics_vector(*position))
                return DE_INVALIDPARAMS;
            r->state.m_Pos = *position;
            r->grounded = false;
            r->contact_normal = {};
            r->contact_object = nullptr;
            r->contact_polygon = UINT32_MAX;
            return DE_OK;
        };
    }
    DRESULT ServerAdapter::GetVelocity(HOBJECT h, DVector* v)
    {
        trace_service("GetVelocity");
        if (v)
            *v = {};
        auto* r = objects_.get(h);
        if (!r || !v)
            return DE_INVALIDPARAMS;
        *v = r->velocity;
        return DE_OK;
    }
    DRESULT ServerAdapter::SetVelocity(HOBJECT h, DVector* v)
    {
        trace_service("SetVelocity");
        auto* r = objects_.get(h);
        if (!r || r->container || !v || !physics_vector(*v))
            return DE_INVALIDPARAMS;
        r->velocity = *v;
        return DE_OK;
    }
    DRESULT ServerAdapter::GetAcceleration(HOBJECT h, DVector* v)
    {
        trace_service("GetAcceleration");
        if (v)
            *v = {};
        auto* r = objects_.get(h);
        if (!r || !v)
            return DE_INVALIDPARAMS;
        *v = r->acceleration;
        return DE_OK;
    }
    DRESULT ServerAdapter::SetAcceleration(HOBJECT h, DVector* v)
    {
        trace_service("SetAcceleration");
        auto* r = objects_.get(h);
        if (!r || r->container || !v || !physics_vector(*v))
            return DE_INVALIDPARAMS;
        r->acceleration = *v;
        return DE_OK;
    }
    DRESULT ServerAdapter::MoveObject(HOBJECT h, DVector* destination)
    {
        trace_service("MoveObject");
        auto* r = objects_.get(h);
        if (!r || r->life != Life::Alive || !destination || !physics_vector(*destination) ||
            !physics_vector(r->state.m_Pos) || !physics_vector(r->dimensions) || r->dimensions.x < 0 ||
            r->dimensions.y < 0 || r->dimensions.z < 0)
            return DE_INVALIDPARAMS;
        if (!collision || !collision_ready)
            return unsupported("MoveObject without loaded collision world");
        if (moving_)
        {
            objects_.fail(ObjectError::Busy, "Recursive MoveObject during contact dispatch");
            return DE_ERROR;
        }
        if (r->state.m_ObjectType != OT_NORMAL && r->state.m_ObjectType != OT_MODEL)
            return unsupported("Moving world-model collision");
        MovementState support;
        const bool can_step = (r->state.m_Flags & FLAG_STAIRSTEP) && (r->state.m_Flags & FLAG_SOLID) &&
                              GetMovementState(h, support) && support.standing && support.ground_normal.y >= 0.5f &&
                              r->velocity.y <= 0.01f && destination->y <= r->state.m_Pos.y + 0.001f;
        moving_ = true;
        const auto ids = objects_.snapshot();
        std::map<std::uint32_t, HOBJECT> touched;
        auto touch = [&](HOBJECT target)
        {
            if (const auto* o = objects_.get(target))
                touched.emplace(o->id.slot, target);
        };
        auto position = r->state.m_Pos, remaining = vector_subtract(*destination, position);
        r->grounded = false;
        r->contact_normal = {};
        r->contact_object = nullptr;
        r->contact_polygon = UINT32_MAX;
        bool blocked_start = false;
        for (unsigned iteration = 0; iteration < 4; ++iteration)
        {
            auto end = vector_add(position, remaining);
            CollisionHit nearest;
            auto query = (r->state.m_Flags & FLAG_SOLID) ? sweep_scene(h, position, end, r->dimensions, nearest, ids)
                                                         : CollisionQuery::Miss;
            if (query == CollisionQuery::Invalid)
            {
                moving_ = false;
                return DE_ERROR;
            }
            const bool found = query == CollisionQuery::Hit;
            // FLAG_STAIRSTEP is original. This bounded three-sweep solver is new;
            // the default half-height rule is evidenced by later LithTech code.
            // Probe paths never dispatch speculative contacts.
            if (can_step && found && !nearest.start_solid && std::abs(nearest.normal.y) < 0.5f &&
                std::hypot(remaining.x, remaining.z) > 0.0001f)
            {
                ++r->stair_attempts;
                const float height = r->dimensions.y * 0.5f;
                auto up = position;
                up.y += height + 0.002f;
                auto across = vector_add(up, {remaining.x, 0, remaining.z});
                auto down = across;
                down.y = position.y - 0.02f;
                CollisionHit probe, landing;
                if (sweep_scene(h, position, up, r->dimensions, probe, ids) == CollisionQuery::Miss &&
                    sweep_scene(h, up, across, r->dimensions, probe, ids) == CollisionQuery::Miss &&
                    sweep_scene(h, across, down, r->dimensions, landing, ids) == CollisionQuery::Hit &&
                    !landing.start_solid && landing.normal.y >= 0.5f && landing.point.y > position.y + 0.001f &&
                    landing.point.y - position.y <= height + 0.001f)
                {
                    for (auto id : ids)
                    {
                        const auto* other = objects_.get(id);
                        if (!other || other->handle == h || other->life != Life::Alive ||
                            !((other->state.m_Flags | r->state.m_Flags) & FLAG_TOUCH_NOTIFY))
                            continue;
                        if (sweep_box(position, up, r->dimensions, other->state.m_Pos, other->dimensions, probe) ==
                                CollisionQuery::Hit ||
                            sweep_box(up, across, r->dimensions, other->state.m_Pos, other->dimensions, probe) ==
                                CollisionQuery::Hit ||
                            sweep_box(across, landing.point, r->dimensions, other->state.m_Pos, other->dimensions,
                                      probe) == CollisionQuery::Hit)
                            touch(other->handle);
                    }
                    r->last_step_height = landing.point.y - position.y;
                    ++r->stair_steps;
                    position = vector_add(landing.point, vector_scale(landing.normal, 0.001f));
                    r->contact_normal = landing.normal;
                    r->contact_polygon = landing.polygon;
                    r->contact_object = landing.object;
                    r->grounded = true;
                    const auto into = vector_dot(r->velocity, landing.normal);
                    if (into < 0)
                        r->velocity = vector_subtract(r->velocity, vector_scale(landing.normal, into));
                    if (landing.object)
                        touch(landing.object);
                    break;
                }
            }
            auto reached = found ? nearest.point : end;
            for (auto id : ids)
            {
                const auto* other = objects_.get(id);
                if (!other || other->handle == h || other->life != Life::Alive || other->container ||
                    !((other->state.m_Flags | r->state.m_Flags) & FLAG_TOUCH_NOTIFY))
                    continue;
                CollisionHit hit;
                if (sweep_box(position, reached, r->dimensions, other->state.m_Pos, other->dimensions, hit) ==
                    CollisionQuery::Hit)
                    touch(other->handle);
            }
            if (!found)
            {
                position = end;
                break;
            }
            if (nearest.object)
                touch(nearest.object);
            r->contact_normal = nearest.normal;
            r->contact_polygon = nearest.polygon;
            r->contact_object = nearest.object;
            r->grounded |= nearest.normal.y > 0.7f;
            if (nearest.start_solid)
            {
                blocked_start = true;
                break;
            }
            // A small separation avoids repeatedly reporting the same touching face.
            position = vector_add(reached, vector_scale(nearest.normal, 0.001f));
            remaining = vector_scale(remaining, static_cast<float>(1 - nearest.fraction));
            auto into = vector_dot(remaining, nearest.normal);
            if (into < 0)
                remaining = vector_subtract(remaining, vector_scale(nearest.normal, into));
            auto velocity_into = vector_dot(r->velocity, nearest.normal);
            if (velocity_into < 0)
                r->velocity = vector_subtract(r->velocity, vector_scale(nearest.normal, velocity_into));
            if (vector_dot(remaining, remaining) < 1e-10f)
                break;
        }
        r->state.m_Pos = position;
        for (const auto& contact : touched)
        {
            auto other = contact.second;
            auto* target = objects_.get(other);
            if (r->life != Life::Alive)
                break;
            if (!target || target->life != Life::Alive)
                continue;
            // Force is not invented: a mass/impulse solver has not been connected.
            if (r->state.m_Flags & FLAG_TOUCH_NOTIFY)
                objects_.engine_message(h, MID_TOUCHNOTIFY, other);
            if (r->life == Life::Alive && target->life == Life::Alive && (target->state.m_Flags & FLAG_TOUCH_NOTIFY))
                objects_.engine_message(other, MID_TOUCHNOTIFY, h);
        }
        moving_ = false;
        if (blocked_start)
        {
            objects_.fail(ObjectError::Unsupported,
                          "MoveObject starts penetrating geometry; depenetration is not implemented");
            return DE_ERROR;
        }
        return DE_OK;
    }
    bool ServerAdapter::advance_physics(double seconds)
    {
        if (physics_advancing_ || objects_.busy())
            return objects_.fail(ObjectError::Busy, "Reentrant physics step rejected");
        if (!std::isfinite(seconds) || seconds < 0 || seconds > 60)
            return objects_.fail(ObjectError::InvalidTime, "Invalid physics delta");
        if (seconds == 0)
            return true;
        physics_advancing_ = true;
        struct Reset
        {
            bool& flag;
            ~Reset()
            {
                flag = false;
            }
        } reset{physics_advancing_};
        const auto ids = objects_.snapshot();
        for (auto id : ids)
        {
            auto* r = objects_.get(id);
            if (!r || r->life != Life::Alive || r->activity != OBJSTATE_ACTIVE || r->client_movement || r->container ||
                (r->state.m_ObjectType != OT_NORMAL && r->state.m_ObjectType != OT_MODEL))
                continue;
            ContainerState containers;
            if (!GetContainers(r->handle, r->state.m_Pos, containers))
                return false;
            if (!(r->state.m_Flags & FLAG_GRAVITY) && vector_dot(r->velocity, r->velocity) == 0 &&
                vector_dot(r->acceleration, r->acceleration) == 0 && !containers.total)
                continue;
            if (seconds > 0.5)
                return objects_.fail(ObjectError::InvalidTime, "Physics catch-up exceeds 0.5 seconds");
            const auto steps = static_cast<unsigned>(std::max(1.0, std::ceil(seconds * 120)));
            const auto dt = static_cast<float>(seconds / steps);
            for (unsigned i = 0; i < steps && r->life == Life::Alive; ++i)
            {
                ContainerPhysics physics{
                    r->handle, r->velocity, r->acceleration, dt, (r->state.m_Flags & FLAG_GRAVITY) != 0, false, false};
                if (!affect_containers(r->handle, physics))
                    return false;
                if (r->life != Life::Alive)
                    break;
                r->velocity = physics.velocity;
                auto acceleration = physics.acceleration;
                if (physics.gravity)
                    acceleration = vector_add(acceleration, gravity);
                r->velocity = vector_add(r->velocity, vector_scale(acceleration, dt));
                auto destination = vector_add(r->state.m_Pos, vector_scale(r->velocity, dt));
                if (MoveObject(r->handle, &destination) != DE_OK)
                    return false;
            }
        }
        return objects_.finish_frame();
    }
} // namespace OpenBlood2
