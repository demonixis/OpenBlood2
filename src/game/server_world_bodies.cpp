#include "engine/world_model.h"
#include "game/server_adapter.h"
#include <algorithm>
#include <cmath>
namespace OpenBlood2
{
    bool ServerAdapter::ConfigureWorldBody(HOBJECT handle, const WorldBodyProperties& properties)
    {
        trace_service("ConfigureWorldBody");
        auto* r = objects_.get(handle);
        if (!r || r->life != Life::Alive || !r->world_model || r->container || r->world_body ||
            !(r->state.m_Flags & FLAG_BOXPHYSICS) || (r->state.m_Flags & FLAG_GOTHRUWORLD) ||
            !std::isfinite(properties.mass) || properties.mass < 0 || properties.mass > 1e6f ||
            !std::isfinite(properties.friction) || properties.friction < 0 || properties.friction > 20005)
            return objects_.fail(ObjectError::Unsupported, "Invalid dynamic world-body configuration");
        r->world_body = true;
        r->body_properties = properties;
        return true;
    }
    bool ServerAdapter::SetBlockingPriority(HOBJECT handle, DBYTE priority)
    {
        trace_service("SetBlockingPriority");
        auto* r = objects_.get(handle);
        if (!r || r->life != Life::Alive)
            return false;
        r->blocking_priority = priority;
        return true;
    }
    MoverResult ServerAdapter::plan_world_body(ObjectRecord& body, DVector destination,
                                               const std::vector<ObjectId>& ids, HOBJECT ignore, BodyPlan& plan)
    {
        plan = {};
        plan.body = &body;
        plan.initiator = ignore;
        plan.origin = plan.destination = body.state.m_Pos;
        plan.velocity = body.velocity;
        if (!body.world_body || !physics_vector(destination) || !physics_vector(plan.velocity))
            return MoverResult::Invalid;
        const auto offset = vector_subtract(object_bounds_center(body), body.state.m_Pos);
        auto position = object_bounds_center(body);
        auto remaining = vector_subtract(destination, plan.origin);
        if (!physics_vector(remaining))
            return MoverResult::Invalid;
        auto blocked = [&](const CollisionHit& contact)
        {
            plan.destination = plan.origin;
            plan.paths.clear();
            plan.velocity = {};
            plan.contact = contact;
            plan.blocked = true;
            return MoverResult::Blocked;
        };
        if (vector_dot(remaining, remaining) == 0)
            return MoverResult::Moved;
        if (body.state.m_Flags & FLAG_SOLID)
            for (auto id : ids)
            {
                auto* rider = objects_.get(id);
                if (!rider || rider->life != Life::Alive || rider == &body || rider->handle == ignore ||
                    !(rider->state.m_Flags & FLAG_SOLID) || (rider->state.m_Flags & FLAG_DONTFOLLOWSTANDING))
                    continue;
                MovementState support;
                if (!GetMovementState(rider->handle, support) || !support.standing ||
                    support.ground_object != body.handle)
                    continue;
                if (rider->world_model || plan.riders.size() == 64)
                {
                    CollisionHit hit;
                    hit.object = rider->handle;
                    return blocked(hit); // No stacks or recursive chain pushing.
                }
                plan.riders.push_back({rider, rider->state.m_Pos});
            }
        auto sweep = [&](DVector from, DVector to, CollisionHit& nearest)
        {
            auto result = collision->sweep(from, to, body.dimensions, nearest);
            if (result == CollisionQuery::Invalid || !(body.state.m_Flags & FLAG_SOLID))
                return result;
            for (auto id : ids)
            {
                const auto* other = objects_.get(id);
                if (!other || other->life != Life::Alive || other == &body || other->handle == ignore ||
                    !(other->state.m_Flags & FLAG_SOLID) ||
                    std::any_of(plan.riders.begin(), plan.riders.end(),
                                [&](const auto& rider) { return rider.body == other; }))
                    continue;
                CollisionHit hit;
                const auto query = sweep_object(*other, from, to, body.dimensions, hit);
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
        };
        for (unsigned i = 0; i < 4; ++i)
        {
            const auto end = vector_add(position, remaining);
            CollisionHit hit;
            const auto query = sweep(position, end, hit);
            if (query == CollisionQuery::Invalid)
                return MoverResult::Invalid;
            if (query == CollisionQuery::Hit && hit.start_solid)
                return blocked(hit);
            const auto reached =
                query == CollisionQuery::Hit ? vector_add(hit.point, vector_scale(hit.normal, .001f)) : end;
            if (!physics_vector(reached))
                return MoverResult::Invalid;
            // Each rider follows exactly the accepted piecewise path. Validate
            // every leg against the old scene before any body position changes.
            for (const auto& rider : plan.riders)
            {
                const auto from =
                    vector_add(rider.origin, vector_subtract(vector_subtract(position, offset), plan.origin));
                const auto to =
                    vector_add(rider.origin, vector_subtract(vector_subtract(reached, offset), plan.origin));
                CollisionHit obstruction;
                const auto result =
                    sweep_scene(rider.body->handle, from, to, rider.body->dimensions, obstruction, ids, body.handle);
                if (result == CollisionQuery::Invalid)
                    return MoverResult::Invalid;
                if (result == CollisionQuery::Hit && (obstruction.start_solid || obstruction.fraction < 1 - 1e-6))
                    return blocked(obstruction);
            }
            plan.paths.push_back({vector_subtract(position, offset), vector_subtract(reached, offset)});
            position = reached;
            if (query != CollisionQuery::Hit)
                break;
            plan.contact = hit;
            plan.blocked = true;
            remaining = vector_scale(remaining, float(1 - hit.fraction));
            const auto inward = vector_dot(remaining, hit.normal);
            if (inward < 0)
                remaining = vector_subtract(remaining, vector_scale(hit.normal, inward));
            const auto velocity_inward = vector_dot(plan.velocity, hit.normal);
            if (velocity_inward < 0)
                plan.velocity = vector_subtract(plan.velocity, vector_scale(hit.normal, velocity_inward));
            if (vector_dot(remaining, remaining) < 1e-10f)
                break;
        }
        plan.destination = vector_subtract(position, offset);
        return MoverResult::Moved;
    }
    void ServerAdapter::commit_world_body(const BodyPlan& plan)
    {
        auto& body = *plan.body;
        body.state.m_Pos = plan.destination;
        body.velocity = plan.velocity;
        body.contact_object = plan.contact.object;
        body.contact_polygon = plan.contact.polygon;
        body.contact_normal = plan.contact.normal;
        body.grounded = plan.blocked && plan.contact.normal.y > .7f;
        body.body_result = plan.blocked ? MoverResult::Blocked : MoverResult::Moved;
        body.mover_blocker = plan.contact.object;
        body.mover_blocks += plan.blocked;
        if (vector_dot(vector_subtract(plan.destination, plan.origin),
                       vector_subtract(plan.destination, plan.origin)) == 0)
            return;
        ++body.mover_moves;
        for (const auto& rider : plan.riders)
        {
            rider.body->state.m_Pos = vector_add(rider.origin, vector_subtract(plan.destination, plan.origin));
            rider.body->grounded = false;
            rider.body->contact_object = nullptr;
            rider.body->contact_normal = {};
            rider.body->contact_polygon = UINT32_MAX;
            ++body.mover_riders;
        }
    }
    void ServerAdapter::dispatch_world_body(const BodyPlan& plan, const std::vector<ObjectId>& ids)
    {
        if (plan.paths.empty())
            return;
        auto contact = [&](ObjectRecord& a, ObjectRecord& b)
        {
            if (a.life == Life::Alive && b.life == Life::Alive && (a.state.m_Flags & FLAG_TOUCH_NOTIFY))
                objects_.engine_message(a.handle, MID_TOUCHNOTIFY, b.handle);
            if (a.life == Life::Alive && b.life == Life::Alive && (b.state.m_Flags & FLAG_TOUCH_NOTIFY))
                objects_.engine_message(b.handle, MID_TOUCHNOTIFY, a.handle);
        };
        auto sensors = [&](ObjectRecord& body, DVector origin, bool mover)
        {
            for (auto id : ids)
            {
                auto* other = objects_.get(id);
                if (body.life != Life::Alive)
                    break;
                if (!other || other == &body || other->life != Life::Alive || other->container || other == plan.body ||
                    other->handle == plan.initiator ||
                    !((other->state.m_Flags | body.state.m_Flags) & FLAG_TOUCH_NOTIFY))
                    continue;
                if (mover &&
                    std::any_of(plan.riders.begin(), plan.riders.end(), [&](const auto& r) { return r.body == other; }))
                    continue;
                for (const auto& path : plan.paths)
                {
                    const auto from = vector_add(origin, vector_subtract(path.from, plan.origin));
                    const auto to = vector_add(origin, vector_subtract(path.to, plan.origin));
                    CollisionHit hit;
                    if (sweep_object(*other, from, to, body.dimensions, hit) == CollisionQuery::Hit ||
                        (mover && other->handle == plan.contact.object))
                    {
                        contact(body, *other);
                        break;
                    }
                }
            }
        };
        const auto center =
            vector_add(object_bounds_center(*plan.body), vector_subtract(plan.origin, plan.destination));
        sensors(*plan.body, center, true);
        for (const auto& rider : plan.riders)
        {
            contact(*plan.body, *rider.body);
            sensors(*rider.body, rider.origin, false);
        }
    }
    DRESULT ServerAdapter::move_world_body(ObjectRecord& body, DVector destination)
    {
        const auto ids = objects_.snapshot();
        BodyPlan plan;
        auto result = plan_world_body(body, destination, ids, nullptr, plan);
        body.body_result = result;
        if (result == MoverResult::Invalid || result == MoverResult::Unavailable)
            return DE_ERROR;
        moving_ = true;
        commit_world_body(plan);
        dispatch_world_body(plan, ids);
        moving_ = false;
        return DE_OK; // Blocked/clipped motion is a valid collision response.
    }
} // namespace OpenBlood2
