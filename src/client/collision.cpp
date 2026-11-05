#include "client/collision.h"
#include "engine/shape_collision.h"
#include <algorithm>
#include <array>
#include <bit>
#include <cmath>

namespace OpenBlood2
{
    namespace
    {
        bool same_bits(float a, float b)
        {
            return std::bit_cast<std::uint32_t>(a) == std::bit_cast<std::uint32_t>(b);
        }
        bool same_bits(DVector a, DVector b)
        {
            return same_bits(a.x, b.x) && same_bits(a.y, b.y) && same_bits(a.z, b.z);
        }
        // Bit-exact: an identical collider already passed validation, NaN never compares equal.
        bool same_collider(const ClientCollider& a, const ClientCollider& b)
        {
            if (a.id != b.id || a.ignored_by != b.ignored_by || a.shape != b.shape || a.flags != b.flags ||
                a.user_flags != b.user_flags || a.type != b.type || a.priority != b.priority ||
                a.volume_brush != b.volume_brush || a.attached != b.attached || a.world_model != b.world_model ||
                a.container != b.container || a.dynamic != b.dynamic || a.pushable != b.pushable ||
                !same_bits(a.position, b.position) || !same_bits(a.dimensions, b.dimensions) ||
                !same_bits(a.velocity, b.velocity) || !same_bits(a.acceleration, b.acceleration) ||
                a.environment.has_value() != b.environment.has_value())
                return false;
            if (!a.environment)
                return true;
            const auto &x = *a.environment, &y = *b.environment;
            return x.code == y.code && x.hidden == y.hidden && same_bits(x.current, y.current) &&
                   same_bits(x.gravity, y.gravity) && same_bits(x.viscosity, y.viscosity);
        }
        bool dimensions(DVector v)
        {
            return physics_vector(v) && v.x >= 0 && v.y >= 0 && v.z >= 0;
        }
        float head_skin(std::uint32_t flags, float half)
        {
            return (flags & FLAG_STAIRSTEP) && (flags & FLAG_SOLID) && half > .01f ? .005f : 0.f;
        }
        ClientCollisionHit value_hit(const CollisionHit& hit, ObjectId object = {})
        {
            return {object,      hit.fraction, hit.penetration,   hit.normal,      hit.point,
                    hit.polygon, hit.departed, hit.texture_flags, hit.start_solid, hit.allowed_overlap};
        }
        CollisionQuery sweep_collider(const ClientCollider& object, DVector from, DVector to, DVector half,
                                      ClientCollisionHit& hit, double allowed = 0,
                                      StartOverlap start = StartOverlap::Block)
        {
            CollisionHit contact;
            const auto query = sweep_shape(object.shape.get(), object.flags & FLAG_BOXPHYSICS, object.position,
                                           object.dimensions, from, to, half, contact, allowed, start);
            hit = value_hit(contact, object.id);
            return query;
        }
    } // namespace
    bool ClientCollisionWorld::begin(std::uint64_t world)
    {
        if (!world || world <= frame_.world)
            return false;
        frame_ = {};
        frame_.world = world;
        return true;
    }
    bool ClientCollisionWorld::apply(ClientCollisionFrame&& next, std::string& error)
    {
        error.clear();
        if (!next.world || next.world != frame_.world || !next.sequence || next.sequence <= frame_.sequence)
            error = "Stale client collision frame";
        else if (!next.primary || next.objects.size() > ClientWorld::object_limit)
            error = "Missing or excessive client collision scene";
        else if (next.primary != frame_.primary || next.objects.size() != frame_.objects.size() ||
                 !std::equal(next.objects.begin(), next.objects.end(), frame_.objects.begin(), same_collider))
        {
            ObjectId previous;
            for (const auto& object : next.objects)
            {
                if (!object.id || object.id.slot >= ClientWorld::object_limit || !object.id.generation ||
                    (previous && previous.slot >= object.id.slot) || !physics_vector(object.position) ||
                    !physics_vector(object.velocity) || !physics_vector(object.acceleration) ||
                    !dimensions(object.dimensions) || ((object.world_model || object.container) && !object.shape) ||
                    (object.ignored_by &&
                     (!object.ignored_by.generation || object.ignored_by.slot >= ClientWorld::object_limit)))
                {
                    error = "Invalid client collider identity or state";
                    break;
                }
                if (object.shape && (!physics_vector(object.shape->origin) || !physics_vector(object.shape->min) ||
                                     !physics_vector(object.shape->max)))
                {
                    error = "Invalid client collider geometry";
                    break;
                }
                if (object.environment)
                {
                    const auto& environment = *object.environment;
                    if (!object.container || !object.shape || !physics_vector(environment.current) ||
                        !std::isfinite(environment.gravity) || !std::isfinite(environment.viscosity) ||
                        environment.viscosity < 0 || environment.viscosity > 1)
                    {
                        error = "Invalid client container environment";
                        break;
                    }
                }
                previous = object.id;
            }
        }
        if (!error.empty())
            return false;
        // The caller receives the replaced scene, so a local publisher can reuse its storage.
        std::swap(frame_, next);
        return true;
    }
    const ClientCollider* ClientCollisionWorld::get(ObjectId id) const
    {
        if (!id)
            return nullptr;
        auto it = std::lower_bound(frame_.objects.begin(), frame_.objects.end(), id.slot,
                                   [](const ClientCollider& object, auto slot) { return object.id.slot < slot; });
        return it != frame_.objects.end() && it->id == id ? &*it : nullptr;
    }
    CollisionQuery ClientCollisionWorld::sweep(ClientObjectId ignore, DVector from, DVector to, DVector half,
                                               ClientCollisionHit& nearest, ObjectId extra, double allowed,
                                               StartOverlap start) const
    {
        return sweep(ignore, from, to, half, nearest, std::span<const ObjectId>(&extra, 1), allowed, start);
    }
    CollisionQuery ClientCollisionWorld::sweep(ClientObjectId ignore, DVector from, DVector to, DVector half,
                                               ClientCollisionHit& nearest, std::span<const ObjectId> extras,
                                               double allowed, StartOverlap start) const
    {
        nearest = {};
        if (ignore.world != frame_.world || !frame_.primary || (ignore.object && !get(ignore.object)))
            return CollisionQuery::Invalid;
        CollisionHit world_hit;
        auto result = frame_.primary->sweep(from, to, half, world_hit, allowed, start);
        nearest = value_hit(world_hit);
        if (result == CollisionQuery::Invalid)
            return result;
        auto departed = nearest.departed;
        for (const auto& object : frame_.objects)
        {
            if (object.attached || object.id == ignore.object ||
                std::find(extras.begin(), extras.end(), object.id) != extras.end() || !(object.flags & FLAG_SOLID) ||
                (ignore.object && object.ignored_by == ignore.object))
                continue;
            ClientCollisionHit hit;
            const auto query = sweep_collider(object, from, to, half, hit, allowed, start);
            if (query == CollisionQuery::Invalid)
                return query;
            departed += hit.departed;
            if (query == CollisionQuery::Hit &&
                (result != CollisionQuery::Hit || hit.fraction < nearest.fraction ||
                 (hit.fraction == nearest.fraction && nearest.allowed_overlap && !hit.allowed_overlap)))
            {
                nearest = hit;
                result = query;
            }
        }
        nearest.departed = departed;
        return result;
    }
    CollisionQuery ClientCollisionWorld::resize(ClientObjectId owner, const ClientMotionState& input, DVector size,
                                                ClientMotionState& out) const
    {
        const auto* object = get(owner.object);
        if (owner.world != frame_.world || !object || object->attached || object->shape || object->dynamic ||
            (object->type != OT_MODEL && object->type != OT_NORMAL) || !dimensions(size) ||
            !dimensions(input.dimensions) || !physics_vector(input.position) || !physics_vector(input.velocity) ||
            !physics_vector(input.acceleration))
            return CollisionQuery::Invalid;
        auto next = input;
        next.position.y += size.y - input.dimensions.y;
        next.dimensions = size;
        const auto skin = head_skin(object->flags, size.y);
        auto center = next.position;
        center.y -= skin;
        ClientCollisionHit hit;
        const auto query = sweep(owner, center, center, {size.x, size.y - skin, size.z}, hit);
        if (query != CollisionQuery::Miss)
            return query;
        if (!support(owner, next))
            return CollisionQuery::Invalid;
        out = next;
        return CollisionQuery::Miss;
    }
    bool ClientCollisionWorld::support(ClientObjectId owner, ClientMotionState& state) const
    {
        const auto* object = get(owner.object);
        if (owner.world != frame_.world || !object || object->attached || !frame_.primary ||
            !physics_vector(state.position) || !physics_vector(state.velocity) || !physics_vector(state.acceleration) ||
            !dimensions(state.dimensions))
            return false;
        state.standing = false;
        state.ground_normal = {};
        state.ground_object = {};
        state.ground_surface = 0;
        state.ground_polygon = UINT32_MAX;
        const auto skin = head_skin(object->flags, state.dimensions.y);
        auto center = shape_bounds_center(state.position, object->shape.get());
        center.y -= skin;
        auto to = center;
        to.y -= .02f;
        ClientCollisionHit hit;
        const auto query =
            sweep(owner, center, to, {state.dimensions.x, state.dimensions.y - skin, state.dimensions.z}, hit);
        if (query == CollisionQuery::Invalid)
            return false;
        if (query == CollisionQuery::Hit && !hit.start_solid && hit.normal.y > 0 &&
            vector_dot(state.velocity, hit.normal) <= .01f)
        {
            state.standing = true;
            state.ground_normal = hit.normal;
            state.ground_object = hit.object;
            state.ground_polygon = hit.polygon;
            state.ground_surface = hit.texture_flags;
            if (!state.ground_surface)
                if (const auto* ground = get(hit.object))
                    state.ground_surface = ground->user_flags >> 24;
        }
        return true;
    }
    // Read-only counterpart of plan_world_body: validate the same four sliding
    // legs and up to 64 riders. The client never commits body/rider motion here.
    bool ClientCollisionWorld::push(const ClientCollider& body, DVector destination, ObjectId ignore,
                                    DVector& reached) const
    {
        const auto offset = vector_subtract(shape_bounds_center(body.position, body.shape.get()), body.position);
        auto position = shape_bounds_center(body.position, body.shape.get());
        auto remaining = vector_subtract(destination, body.position);
        if (!body.dynamic || !physics_vector(destination) || !physics_vector(remaining))
            return false;
        reached = body.position;
        if (vector_dot(remaining, remaining) == 0)
            return true;
        std::vector<const ClientCollider*> riders;
        if (body.flags & FLAG_SOLID)
            for (const auto& rider : frame_.objects)
            {
                if (rider.id == body.id || rider.id == ignore || !(rider.flags & FLAG_SOLID) ||
                    (rider.flags & FLAG_DONTFOLLOWSTANDING))
                    continue;
                ClientMotionState state;
                state.position = rider.position;
                state.velocity = rider.velocity;
                state.acceleration = rider.acceleration;
                state.dimensions = rider.dimensions;
                if (!support({frame_.world, rider.id}, state) || !state.standing || state.ground_object != body.id)
                    continue;
                if (rider.world_model || riders.size() == 64)
                    return false;
                riders.push_back(&rider);
            }
        auto sweep_push = [&](DVector from, DVector to, ClientCollisionHit& nearest)
        {
            CollisionHit hit;
            auto result = frame_.primary->sweep(from, to, body.dimensions, hit);
            nearest = value_hit(hit);
            if (result == CollisionQuery::Invalid || !(body.flags & FLAG_SOLID))
                return result;
            for (const auto& other : frame_.objects)
            {
                if (other.id == body.id || other.id == ignore || !(other.flags & FLAG_SOLID) ||
                    std::find(riders.begin(), riders.end(), &other) != riders.end())
                    continue;
                ClientCollisionHit candidate;
                const auto query = sweep_collider(other, from, to, body.dimensions, candidate);
                if (query == CollisionQuery::Invalid)
                    return query;
                if (query == CollisionQuery::Hit &&
                    (result != CollisionQuery::Hit || candidate.fraction < nearest.fraction))
                {
                    nearest = candidate;
                    result = query;
                }
            }
            return result;
        };
        for (unsigned i = 0; i < 4; ++i)
        {
            const auto end = vector_add(position, remaining);
            ClientCollisionHit hit;
            const auto query = sweep_push(position, end, hit);
            if (query == CollisionQuery::Invalid || (query == CollisionQuery::Hit && hit.start_solid))
                return false;
            const auto next =
                query == CollisionQuery::Hit ? vector_add(hit.point, vector_scale(hit.normal, .001f)) : end;
            if (!physics_vector(next))
                return false;
            for (const auto* rider : riders)
            {
                const auto from =
                    vector_add(rider->position, vector_subtract(vector_subtract(position, offset), body.position));
                const auto to =
                    vector_add(rider->position, vector_subtract(vector_subtract(next, offset), body.position));
                ClientCollisionHit obstruction;
                const auto result = sweep({frame_.world, rider->id}, from, to, rider->dimensions, obstruction, body.id);
                if (result == CollisionQuery::Invalid ||
                    (result == CollisionQuery::Hit && (obstruction.start_solid || obstruction.fraction < 1 - 1e-6)))
                    return false;
            }
            position = next;
            if (query != CollisionQuery::Hit)
                break;
            remaining = vector_scale(remaining, float(1 - hit.fraction));
            const auto inward = vector_dot(remaining, hit.normal);
            if (inward < 0)
                remaining = vector_subtract(remaining, vector_scale(hit.normal, inward));
            if (vector_dot(remaining, remaining) < 1e-10f)
                break;
        }
        reached = vector_subtract(position, offset);
        return true;
    }
    bool ClientCollisionWorld::predict(ClientObjectId owner, const ClientMotionState& input, DVector destination,
                                       DVector velocity, DVector acceleration, ClientMotionState& result,
                                       bool pass_overlapping_models) const
    {
        auto state = input;
        result = {};
        const auto* source = get(owner.object);
        if (owner.world != frame_.world || !source || source->attached || source->shape || source->dynamic ||
            (source->type != OT_NORMAL && source->type != OT_MODEL) || !physics_vector(destination) ||
            !physics_vector(velocity) || !physics_vector(acceleration))
            return false;
        state.velocity = velocity;
        state.acceleration = acceleration;
        if (!support(owner, state))
            return false;
        const unsigned priority = state.standing ? 50 : 40;
        const auto flags = source->flags;
        const bool can_step = !(flags & FLAG_NOSLIDING) && (flags & FLAG_STAIRSTEP) && (flags & FLAG_SOLID) &&
                              velocity.y <= .01f && destination.y <= state.position.y + .001f;
        const auto skin = head_skin(flags, state.dimensions.y);
        const DVector box{state.dimensions.x, state.dimensions.y - skin, state.dimensions.z}, lower{0, skin, 0};
        // Slot 0: a world model the body started inside; then overlapped models.
        std::array<ObjectId, 5> passes{};
        std::size_t models = 0;
        auto sweep_body = [&](DVector from, DVector to, ClientCollisionHit& hit, ObjectId extra = {},
                              double allowed = 0, StartOverlap start = StartOverlap::Block)
        {
            passes[0] = extra;
            const auto query = sweep(owner, vector_subtract(from, lower), vector_subtract(to, lower), box, hit,
                                     std::span<const ObjectId>(passes.data(), models + 1), allowed, start);
            hit.point = vector_add(hit.point, lower);
            return query;
        };
        auto position = state.position, remaining = vector_subtract(destination, position);
        constexpr double allowed = .01;
        const auto start = (flags & FLAG_SOLID) ? StartOverlap::Leave : StartOverlap::LeaveOrSlide;
        ObjectId passed;
        for (unsigned iteration = 0; iteration < 4 + models; ++iteration)
        {
            auto end = vector_add(position, remaining);
            ClientCollisionHit nearest;
            auto query = CollisionQuery::Miss;
            if (flags & FLAG_SOLID)
                query = sweep_body(position, end, nearest, passed, allowed, start);
            else if ((flags & FLAG_GRAVITY) && !(flags & FLAG_GOTHRUWORLD))
            {
                CollisionHit hit;
                query = frame_.primary->sweep(position, end, state.dimensions, hit, allowed, start);
                nearest = value_hit(hit);
            }
            state.start_departures += nearest.departed;
            if (query == CollisionQuery::Invalid)
                return false;
            const bool found = query == CollisionQuery::Hit;
            if (found && nearest.allowed_overlap)
            {
                ++state.overlap_contacts;
                state.last_overlap_depth = float(nearest.penetration);
            }
            const auto* obstacle = found ? get(nearest.object) : nullptr;
            if (found && nearest.start_solid && !passed && obstacle && obstacle->world_model && obstacle != source)
            {
                passed = obstacle->id;
                continue;
            }
            if (found && nearest.start_solid && pass_overlapping_models && obstacle && obstacle != source &&
                obstacle->type == OT_MODEL && !obstacle->world_model && !obstacle->container && !obstacle->shape &&
                models + 1 < passes.size())
            {
                passes[++models] = obstacle->id;
                continue;
            }
            if (found && !nearest.start_solid && std::abs(nearest.normal.y) < .01f && obstacle && obstacle->dynamic &&
                obstacle->pushable && priority > obstacle->priority)
            {
                const auto into = vector_dot(remaining, nearest.normal);
                const auto offset = vector_scale(nearest.normal, into * float(1 - nearest.fraction) - .002f);
                ClientCollisionHit clearance;
                auto actor_end = end;
                auto clear = sweep_body(position, actor_end, clearance, obstacle->id, allowed);
                // Like the server, support is queried at the original body position.
                auto flat = state;
                const bool flatten = clear == CollisionQuery::Hit && !clearance.start_solid &&
                                     clearance.normal.y > .99999f && actor_end.y < position.y && support(owner, flat) &&
                                     flat.standing && flat.ground_normal.y > .99999f;
                if (flatten)
                {
                    actor_end.y = position.y;
                    clear = sweep_body(position, actor_end, clearance, obstacle->id, allowed);
                }
                const auto requested = vector_add(obstacle->position, offset);
                DVector pushed;
                const bool planned = clear == CollisionQuery::Miss && push(*obstacle, requested, owner.object, pushed);
                if (into < 0 && planned &&
                    vector_dot(vector_subtract(pushed, requested), vector_subtract(pushed, requested)) < 1e-8f)
                {
                    if (flatten)
                        velocity.y = 0;
                    position = actor_end;
                    break;
                }
            }
            if (can_step && found && !nearest.start_solid && std::abs(nearest.normal.y) < .5f &&
                std::hypot(remaining.x, remaining.z) > .0001f)
            {
                ++state.stair_attempts;
                const float height = state.dimensions.y * .5f;
                auto up = position;
                up.y += height + .002f;
                ClientCollisionHit probe, landing;
                const auto lift = sweep_body(position, up, probe, {}, allowed);
                if (lift == CollisionQuery::Hit && !probe.start_solid)
                    up.y = std::max(position.y, probe.point.y - .001f);
                auto across = vector_add(up, {remaining.x, 0, remaining.z});
                auto down = across;
                down.y = position.y - .02f;
                if (lift != CollisionQuery::Invalid && !(lift == CollisionQuery::Hit && probe.start_solid) &&
                    up.y > position.y + .001f && sweep_body(up, across, probe) == CollisionQuery::Miss &&
                    sweep_body(across, down, landing) == CollisionQuery::Hit && !landing.start_solid &&
                    landing.normal.y >= .5f && landing.point.y > position.y + .001f &&
                    landing.point.y - position.y <= height + .001f)
                {
                    state.last_step_height = landing.point.y - position.y;
                    ++state.stair_steps;
                    position = vector_add(landing.point, vector_scale(landing.normal, .001f));
                    const auto into = vector_dot(velocity, landing.normal);
                    if (into < 0)
                        velocity = vector_subtract(velocity, vector_scale(landing.normal, into));
                    break;
                }
            }
            const auto reached = found ? nearest.point : end;
            if (!found)
            {
                position = end;
                break;
            }
            if (nearest.start_solid)
                return false;
            const auto separation = .001f + (nearest.allowed_overlap ? float(nearest.penetration) : 0.f);
            position = vector_add(reached, vector_scale(nearest.normal, separation));
            remaining = vector_scale(remaining, float(1 - nearest.fraction));
            const auto into = vector_dot(remaining, nearest.normal);
            if (into < 0)
                remaining = vector_subtract(remaining, vector_scale(nearest.normal, into));
            const auto velocity_into = vector_dot(velocity, nearest.normal);
            if (velocity_into < 0)
                velocity = vector_subtract(velocity, vector_scale(nearest.normal, velocity_into));
            if (flags & FLAG_NOSLIDING)
                break;
            if (vector_dot(remaining, remaining) < 1e-10f)
                break;
        }
        state.position = position;
        state.velocity = vector_add(velocity, {});
        if (!support(owner, state))
            return false;
        result = state;
        return true;
    }
} // namespace OpenBlood2
