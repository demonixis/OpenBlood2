#include "engine/world_model.h"
#include "game/original/PickupObject.h"
#include "game/original/PolyGrid.h"
#include "game/original/Projectile.h"
#include "game/original/SeeingEye.h"
#include "game/server_adapter.h"
#include <algorithm>
#include <cmath>
#include <map>
#include <sstream>

namespace OpenBlood2
{
    namespace
    {
        // Head skin (contact_overlap_contracts.md): a solid stair-stepping body leaves
        // the top 0.01 units of its box out of its own motion, support and resize
        // queries; the feet stay exact. Returns how far the swept box's center and
        // half-height drop for half-height `half`.
        float head_skin(const ObjectRecord& r, float half)
        {
            constexpr float skin = 0.01f;
            return (r.state.m_Flags & FLAG_STAIRSTEP) && (r.state.m_Flags & FLAG_SOLID) && half > skin ? skin * .5f
                                                                                                       : 0.f;
        }
    } // namespace
    bool ServerAdapter::CollectObjectsTouchingSphere(DVector center, float radius, std::vector<HOBJECT>& output)
    {
        trace_service("CollectObjectsTouchingSphere");
        output.clear();
        if (!physics_vector(center) || !std::isfinite(radius) || radius < 0 || radius > 1000000)
            return false;
        // Later LithTech reference: box broad phase, then strict expanded bounding sphere.
        // Stable slot order replaces the unavailable historical spatial-tree traversal order.
        for (auto id : objects_.snapshot())
        {
            const auto* record = objects_.get(id);
            if (!record || record->life != Life::Alive)
                continue;
            const auto delta = vector_subtract(record->state.m_Pos, center);
            const auto dims = record->dimensions;
            if (std::abs(delta.x) > radius + dims.x || std::abs(delta.y) > radius + dims.y ||
                std::abs(delta.z) > radius + dims.z)
                continue;
            const double full = radius + std::sqrt(double(vector_dot(dims, dims))) + .1;
            if (double(vector_dot(delta, delta)) < full * full)
                output.push_back(record->handle);
        }
        return true;
    }
    CollisionQuery ServerAdapter::sweep_scene(HOBJECT ignore, DVector from, DVector to, DVector dims,
                                              CollisionHit& nearest, const std::vector<ObjectId>& ids,
                                              HOBJECT ignore_extra, double allowed_penetration, StartOverlap start)
    {
        auto result = collision->sweep(from, to, dims, nearest, allowed_penetration, start);
        if (result == CollisionQuery::Invalid)
            return result;
        auto departed = nearest.departed;
        for (auto id : ids)
        {
            const auto* other = objects_.get(id);
            if (!other || other->life != Life::Alive || other->attachment || other->handle == ignore ||
                other->handle == ignore_extra || !(other->state.m_Flags & FLAG_SOLID))
                continue;
            if (const auto* eye = dynamic_cast<const SeeingEye*>(other->game.get());
                eye && !eye->dropped && eye->m_hFiredFrom == ignore)
                continue; // Let the owner move while its freshly thrown eye exits the body.
            CollisionHit hit;
            auto query = sweep_object(*other, from, to, dims, hit, allowed_penetration, start);
            if (query == CollisionQuery::Invalid)
                return query;
            departed += hit.departed;
            if (query == CollisionQuery::Hit &&
                (result != CollisionQuery::Hit || hit.fraction < nearest.fraction ||
                 (hit.fraction == nearest.fraction && nearest.allowed_overlap && !hit.allowed_overlap)))
            {
                nearest = hit;
                nearest.object = other->handle;
                result = query;
            }
        }
        nearest.departed = departed;
        return result;
    }
    ResizeResult ServerAdapter::ResizeObject(HOBJECT h, DVector dimensions)
    {
        trace_service("ResizeObject");
        auto* r = objects_.get(h);
        if (!r || r->life != Life::Alive || r->attachment || (r->container() || r->world_model) ||
            !physics_vector(dimensions) || dimensions.x <= 0 || dimensions.y <= 0 || dimensions.z <= 0 ||
            !physics_vector(r->state.m_Pos))
            return ResizeResult::Invalid;
        if (!collision || !collision_ready)
            return ResizeResult::Unavailable;
        // Original scripts can select an animation synchronously from a touch
        // callback. Equal extents need no collision transaction (also the first
        // branch in later LithTech ChangeObjectDimensions). During a move, the
        // body already belongs to the checked collision transaction. Outside it,
        // keep the placement check: restore uses equal extents to validate a
        // newly staged saved position, which has not been checked by a move.
        if (moving_ && dimensions.x == r->dimensions.x && dimensions.y == r->dimensions.y &&
            dimensions.z == r->dimensions.z)
            return ResizeResult::Resized;
        if (moving_)
            return ResizeResult::InTransaction;
        // Local SetAnimation/ResetDims replacement. Preserve the feet and test
        // the entire requested shape before committing. No partial resize or
        // pushing other objects; a blocked clip keeps its position and clock.
        // The original SetObjectDims2 keeps the centre (later LithTech
        // ChangeObjectDimensions); gravity bodies and the player settle back onto
        // the same feet, so the foot anchor is equivalent for them. Bodies without
        // FLAG_GRAVITY (Gideon, Zealot, Thief...) never resettle and keep the centre,
        // otherwise every clip change moved a hovering boss up or down.
        auto position = r->state.m_Pos;
        if ((r->state.m_Flags & FLAG_GRAVITY) || r->client_movement)
            position.y += dimensions.y - r->dimensions.y;
        const float skin = head_skin(*r, dimensions.y);
        const DVector box{dimensions.x, dimensions.y - skin, dimensions.z},
            center{position.x, position.y - skin, position.z};
        CollisionHit hit;
        const auto query = sweep_scene(h, center, center, box, hit, objects_.snapshot());
        if (query == CollisionQuery::Invalid)
            return ResizeResult::Invalid;
        if (query == CollisionQuery::Hit)
            return ResizeResult::Blocked;
        r->dimensions = dimensions;
        r->state.m_Pos = position;
        return ResizeResult::Resized;
    }
    bool ServerAdapter::PlaceNonSolidModel(HOBJECT h, DVector requested, DVector anchor, DVector anchor_half,
                                           BodyPlacement& report)
    {
        trace_service("PlaceNonSolidModel");
        report = {};
        report.anchor = anchor;
        report.requested = report.candidate = requested;
        report.result = BodyPlacementResult::Invalid;
        auto* r = objects_.get(h);
        if (!r || r->life != Life::Alive || moving_ || r->attachment || r->container() || r->world_model ||
            r->state.m_ObjectType != OT_MODEL || !r->character_body || (r->state.m_Flags & FLAG_SOLID))
            return false;
        if (!collision || !collision_ready)
        {
            report.result = BodyPlacementResult::Unavailable;
            return false;
        }
        report = collision->place_body(anchor, anchor_half, requested, r->dimensions);
        if (!placement_succeeded(report))
            return false;
        // No callbacks or partial motion. Nonsolid corpses use primary-world
        // collision only, like their subsequent ordinary gravity integration.
        r->state.m_Pos = report.candidate;
        r->grounded = false;
        r->contact_normal = {};
        r->contact_object = nullptr;
        r->contact_polygon = UINT32_MAX;
        return true;
    }
    bool ServerAdapter::GetMovementState(HOBJECT h, MovementState& state)
    {
        trace_service("GetMovementState");
        state = {};
        const auto* r = objects_.get(h);
        if (!r || r->life != Life::Alive || r->attachment || !collision || !collision_ready)
            return false;
        state.position = r->state.m_Pos;
        state.velocity = r->velocity;
        state.acceleration = r->acceleration;
        state.dimensions = r->dimensions;
        state.stair_attempts = r->stair_attempts;
        state.start_departures = r->start_departures;
        state.stair_steps = r->stair_steps;
        state.last_step_height = r->last_step_height;
        state.overlap_contacts = r->overlap_contacts;
        state.last_overlap_depth = r->last_overlap_depth;
        if (!physics_vector(state.position) || !physics_vector(state.velocity) || !physics_vector(state.acceleration) ||
            !physics_vector(state.dimensions) || state.dimensions.x < 0 || state.dimensions.y < 0 ||
            state.dimensions.z < 0)
            return false;
        // A fresh short support sweep avoids a stale last-contact normal after
        // sliding along a wall or walking off a ledge. World support has a polygon
        // identity, not a fabricated HOBJECT. Motion away from the support plane
        // is airborne; positive Y alone can also mean walking uphill.
        const float skin = head_skin(*r, state.dimensions.y);
        auto center = object_bounds_center(*r);
        center.y -= skin;
        auto to = center;
        to.y -= 0.02f;
        CollisionHit nearest;
        const DVector box{state.dimensions.x, state.dimensions.y - skin, state.dimensions.z};
        const auto query = sweep_scene(h, center, to, box, nearest, objects_.snapshot());
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
            state.ground_surface = nearest.texture_flags;
            if (!state.ground_surface && nearest.object)
                state.ground_surface = GetObjectUserFlags(nearest.object) >> 24;
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
            if (!r || r->life != Life::Alive || r->attachment || (r->container() || r->world_model) || !position ||
                !physics_vector(*position))
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
        if (!r || r->life != Life::Alive || r->attachment || r->container() || (r->world_model && !r->world_body) ||
            !v || !physics_vector(*v))
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
        if (!r || r->life != Life::Alive || r->attachment || r->container() || (r->world_model && !r->world_body) ||
            !v || !physics_vector(*v))
            return DE_INVALIDPARAMS;
        r->acceleration = *v;
        return DE_OK;
    }
    DRESULT ServerAdapter::MoveObject(HOBJECT h, DVector* destination)
    {
        trace_service("MoveObject");
        auto* r = objects_.get(h);
        if (!r || r->life != Life::Alive || r->attachment || !destination || !physics_vector(*destination) ||
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
        if (r->world_body || r->model_body)
            return move_world_body(*r, *destination);
        if (r->world_model)
            return MoveWorldModel(h, *destination) == MoverResult::Moved ? DE_OK : DE_ERROR;
        if (r->state.m_ObjectType != OT_NORMAL && r->state.m_ObjectType != OT_MODEL)
            return unsupported("Moving this object type");
        // Later LithTech's StairStep_Segment has no support test: it steps onto any
        // walkable face the body is not moving away from. Airborne bodies at or past
        // their jump apex therefore step up too; rising bodies never do.
        const bool can_step = !(r->state.m_Flags & FLAG_NOSLIDING) && (r->state.m_Flags & FLAG_STAIRSTEP) &&
                              (r->state.m_Flags & FLAG_SOLID) && r->velocity.y <= 0.01f &&
                              destination->y <= r->state.m_Pos.y + 0.001f;
        moving_ = true;
        const auto ids = objects_.snapshot();
        // The body's own sweeps use the head-skinned box; points stay real centers.
        const float skin = head_skin(*r, r->dimensions.y);
        const DVector box{r->dimensions.x, r->dimensions.y - skin, r->dimensions.z}, lower{0, skin, 0};
        auto sweep_body = [&](DVector from, DVector to, CollisionHit& hit, HOBJECT ignore_extra = nullptr,
                              double allowed = 0, StartOverlap overlap = StartOverlap::Block)
        {
            const auto query = sweep_scene(h, vector_subtract(from, lower), vector_subtract(to, lower), box, hit, ids,
                                           ignore_extra, allowed, overlap);
            hit.point = vector_add(hit.point, lower);
            return query;
        };
        struct Contact
        {
            HOBJECT other = nullptr;
            DVector force{};
            bool world = false;
        };
        std::map<std::uint32_t, Contact> touched;
        const auto incoming_velocity = r->velocity;
        auto velocity = incoming_velocity;
        auto mass = [](const ObjectRecord& body)
        { return body.character_body || body.world_body || body.model_body ? body.body_properties.mass : 1.f; };
        auto touch = [&](HOBJECT target)
        {
            if (const auto* o = objects_.get(target))
                touched.try_emplace(o->id.slot, Contact{target});
        };
        auto impact = [&](const CollisionHit& hit)
        {
            const float into = vector_dot(velocity, hit.normal);
            if (into >= 0 || hit.start_solid)
                return;
            auto* other = objects_.get(hit.object);
            const bool world = !other || bool(other->world_model);
            // SDK stopping-force convention, corroborated by later LithTech:
            // world: normal delta-v * .01 * mass; boxes: relative normal speed
            // * .01 * combined mass. The swept solver remains replacement physics.
            const float scale = world ? -into * .01f * mass(*r)
                                      : std::abs(vector_dot(vector_subtract(velocity, other->velocity), hit.normal)) *
                                            .01f * (mass(*r) + mass(*other));
            auto& contact = touched[other ? other->id.slot : UINT32_MAX];
            contact.other = hit.object;
            contact.world = world;
            contact.force = vector_add(contact.force, vector_scale(hit.normal, scale));
        };
        BodyPlan pushed;
        auto position = r->state.m_Pos, remaining = vector_subtract(*destination, position);
        r->grounded = false;
        r->contact_normal = {};
        r->contact_object = nullptr;
        r->contact_polygon = UINT32_MAX;
        bool blocked_start = false;
        CollisionHit start_contact;
        // Contact points are float-rounded, so separating from one face of a
        // crease or wedge can leave the body a few ulps (1.7e-5 units at a
        // CityHub floor/wall corner) inside the adjacent face. Later LithTech
        // classifies planes with a 0.001 tolerance and never stops on this.
        // Replacement policy: tolerate such overlaps but never move deeper.
        constexpr double allowed_penetration = 0.01;
        // Deeper overlaps (later LithTech's one-sided world polygons ignore contacts
        // facing along the motion): a solid body may only leave them, which lets the
        // original MoveObjectToGround lower a player whose start crosses a ceiling;
        // sliding or going deeper still fails below. A non-solid passive object
        // (pickup, debris) may also slide along an authored overlap while settling.
        // See contact_overlap_contracts.md and collision_traversal_contracts.md.
        const auto start = (r->state.m_Flags & FLAG_SOLID) ? StartOverlap::Leave : StartOverlap::LeaveOrSlide;
        // Another object's world model the body starts inside (see below).
        HOBJECT passed = nullptr;
        for (unsigned iteration = 0; iteration < 4; ++iteration)
        {
            auto end = vector_add(position, remaining);
            CollisionHit nearest;
            auto query =
                (r->state.m_Flags & FLAG_SOLID)
                    ? sweep_body(position, end, nearest, passed, allowed_penetration, start)
                    : ((r->state.m_Flags & FLAG_GRAVITY) && !(r->state.m_Flags & FLAG_GOTHRUWORLD)
                           ? collision->sweep(position, end, r->dimensions, nearest, allowed_penetration, start)
                           : CollisionQuery::Miss);
            r->start_departures += nearest.departed;
            if (query == CollisionQuery::Invalid)
            {
                moving_ = false;
                return DE_ERROR;
            }
            const bool found = query == CollisionQuery::Hit;
            if (found && nearest.allowed_overlap)
            {
                ++r->overlap_contacts;
                r->last_overlap_depth = static_cast<float>(nearest.penetration);
            }
            auto* obstacle = found ? objects_.get(nearest.object) : nullptr;
            // Replacement policy: a body created or left inside another object's world model
            // (03_Museum's Trigger3 spawns Fanatics into the skylight glass it destroys at the
            // same instant) moves through that model instead of failing the simulation. The
            // primary world, characters and a second overlapping model still refuse.
            if (found && nearest.start_solid && !passed && obstacle && obstacle->world_model && obstacle != r)
            {
                passed = obstacle->handle;
                ++r->brush_start_passes;
                continue;
            }
            if (found && !nearest.start_solid && std::abs(nearest.normal.y) < .01f && obstacle &&
                (obstacle->world_body || obstacle->model_body) && obstacle->body_properties.pushable &&
                r->blocking_priority > obstacle->blocking_priority)
            {
                const auto into = vector_dot(remaining, nearest.normal);
                const auto push = vector_scale(nearest.normal, into * float(1 - nearest.fraction) - .002f);
                CollisionHit clearance;
                auto actor_end = end;
                auto clear = sweep_body(position, actor_end, clearance, obstacle->handle, allowed_penetration);
                MovementState flat_support;
                const bool flatten = clear == CollisionQuery::Hit && !clearance.start_solid &&
                                     clearance.normal.y > .99999f && actor_end.y < position.y &&
                                     GetMovementState(h, flat_support) && flat_support.standing &&
                                     flat_support.ground_normal.y > .99999f;
                if (flatten)
                {
                    // Gravity may reach the floor just after the side contact.
                    // Revalidate the complete horizontal path before pushing;
                    // otherwise successive grounded commands stick at the box.
                    actor_end.y = position.y;
                    clear = sweep_body(position, actor_end, clearance, obstacle->handle, allowed_penetration);
                }
                const auto planned =
                    clear == CollisionQuery::Miss
                        ? plan_world_body(*obstacle, vector_add(obstacle->state.m_Pos, push), ids, h, pushed)
                        : MoverResult::Unavailable;
                if (into < 0 && clear == CollisionQuery::Miss && planned == MoverResult::Moved &&
                    vector_dot(vector_subtract(pushed.destination, vector_add(obstacle->state.m_Pos, push)),
                               vector_subtract(pushed.destination, vector_add(obstacle->state.m_Pos, push))) < 1e-8f)
                {
                    touch(obstacle->handle);
                    for (auto id : ids)
                    {
                        const auto* other = objects_.get(id);
                        CollisionHit hit;
                        if (other && other->life == Life::Alive && other->handle != h && !other->container() &&
                            ((other->state.m_Flags | r->state.m_Flags) & FLAG_TOUCH_NOTIFY) &&
                            sweep_object(*other, position, actor_end, r->dimensions, hit) == CollisionQuery::Hit)
                            touch(other->handle);
                    }
                    if (flatten)
                        velocity.y = 0;
                    position = actor_end;
                    break;
                }
                pushed = {};
            }
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
                // Lift only as far as the headroom allows. Later LithTech lifts by the
                // actual step and collides the rest of the body normally; requiring the
                // whole limit to clear refused every step under a low ceiling (a
                // crouched body in a 64-unit duct has 8 units to spare, not 14).
                CollisionHit probe, landing;
                const auto lift = sweep_body(position, up, probe, nullptr, allowed_penetration);
                if (lift == CollisionQuery::Hit && !probe.start_solid)
                    up.y = std::max(position.y, probe.point.y - 0.001f);
                auto across = vector_add(up, {remaining.x, 0, remaining.z});
                auto down = across;
                down.y = position.y - 0.02f;
                if (lift != CollisionQuery::Invalid && !(lift == CollisionQuery::Hit && probe.start_solid) &&
                    up.y > position.y + 0.001f && sweep_body(up, across, probe) == CollisionQuery::Miss &&
                    sweep_body(across, down, landing) == CollisionQuery::Hit && !landing.start_solid &&
                    landing.normal.y >= 0.5f && landing.point.y > position.y + 0.001f &&
                    landing.point.y - position.y <= height + 0.001f)
                {
                    for (auto id : ids)
                    {
                        const auto* other = objects_.get(id);
                        if (!other || other->handle == h || other->life != Life::Alive || other->attachment ||
                            other->container() || !((other->state.m_Flags | r->state.m_Flags) & FLAG_TOUCH_NOTIFY))
                            continue;
                        if (sweep_object(*other, position, up, r->dimensions, probe) == CollisionQuery::Hit ||
                            sweep_object(*other, up, across, r->dimensions, probe) == CollisionQuery::Hit ||
                            sweep_object(*other, across, landing.point, r->dimensions, probe) == CollisionQuery::Hit)
                            touch(other->handle);
                    }
                    r->last_step_height = landing.point.y - position.y;
                    ++r->stair_steps;
                    position = vector_add(landing.point, vector_scale(landing.normal, 0.001f));
                    r->contact_normal = landing.normal;
                    r->contact_polygon = landing.polygon;
                    r->contact_object = landing.object;
                    r->grounded = true;
                    impact(landing);
                    const auto into = vector_dot(velocity, landing.normal);
                    if (into < 0)
                        velocity = vector_subtract(velocity, vector_scale(landing.normal, into));
                    if (landing.object)
                        touch(landing.object);
                    break;
                }
            }
            auto reached = found ? nearest.point : end;
            for (auto id : ids)
            {
                const auto* other = objects_.get(id);
                if (!other || other->handle == h || other->life != Life::Alive || other->attachment ||
                    other->container() || !((other->state.m_Flags | r->state.m_Flags) & FLAG_TOUCH_NOTIFY))
                    continue;
                CollisionHit hit;
                if (sweep_object(*other, position, reached, r->dimensions, hit) == CollisionQuery::Hit)
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
                start_contact = nearest;
                break;
            }
            // A small separation avoids repeatedly reporting the same touching face.
            // A tolerated overlap is lifted out completely; otherwise alternating
            // wedge separations can ratchet it deeper over many frames.
            const auto separation = 0.001f + (nearest.allowed_overlap ? float(nearest.penetration) : 0.f);
            position = vector_add(reached, vector_scale(nearest.normal, separation));
            remaining = vector_scale(remaining, static_cast<float>(1 - nearest.fraction));
            auto into = vector_dot(remaining, nearest.normal);
            if (into < 0)
                remaining = vector_subtract(remaining, vector_scale(nearest.normal, into));
            impact(nearest);
            auto velocity_into = vector_dot(velocity, nearest.normal);
            if (velocity_into < 0)
                velocity = vector_subtract(velocity, vector_scale(nearest.normal, velocity_into));
            // Source flag requests first-contact placement without a tangential slide.
            // Velocity keeps the ordinary normal projection; this is replacement physics.
            if (r->state.m_Flags & FLAG_NOSLIDING)
                break;
            if (vector_dot(remaining, remaining) < 1e-10f)
                break;
        }
        r->state.m_Pos = position;
        if (pushed.body)
        {
            commit_world_body(pushed);
            ++pushed.body->body_pushes;
            dispatch_world_body(pushed, ids);
        }
        for (const auto& [slot, contact] : touched)
        {
            const auto other = contact.other;
            auto* target = objects_.get(other);
            if (r->life != Life::Alive)
                break;
            if (other && (!target || target->life != Life::Alive))
                continue;
            const float force = std::sqrt(vector_dot(contact.force, contact.force));
            // Final positions are committed, but source callbacks see velocity
            // before the stopping impulse. Apply it afterwards, preserving any
            // callback velocity delta and keeping all removal deferred.
            if (r->state.m_Flags & FLAG_TOUCH_NOTIFY)
            {
                const auto before_callback = r->velocity;
                if (objects_.engine_message(h, MID_TOUCHNOTIFY, other, force) && force > 0)
                {
                    r->last_impact_force = force;
                    r->last_impact_velocity = before_callback;
                    ++r->impact_notifications;
                }
            }
            if (target && r->life == Life::Alive && target->life == Life::Alive &&
                (target->state.m_Flags & FLAG_TOUCH_NOTIFY))
            {
                const float reciprocal = contact.world ? force : -force;
                const auto before_callback = target->velocity;
                if (objects_.engine_message(other, MID_TOUCHNOTIFY, h, reciprocal) && force > 0)
                {
                    target->last_impact_force = reciprocal;
                    target->last_impact_velocity = before_callback;
                    ++target->impact_notifications;
                }
            }
        }
        if (r->life == Life::Alive)
            r->velocity = vector_add(velocity, vector_subtract(r->velocity, incoming_velocity));
        moving_ = false;
        if (blocked_start)
        {
            const auto* other = objects_.get(start_contact.object);
            std::ostringstream detail;
            detail << "MoveObject starts penetrating geometry; depenetration is not implemented: " << r->type->name
                   << " / " << r->state.m_Name << " (";
            if (other)
                detail << other->type->name << " / " << other->state.m_Name;
            else
                detail << "world polygon " << start_contact.polygon;
            detail << ", depth " << start_contact.penetration << ", normal " << start_contact.normal.x << ' '
                   << start_contact.normal.y << ' ' << start_contact.normal.z << ", at " << r->state.m_Pos.x << ' '
                   << r->state.m_Pos.y << ' ' << r->state.m_Pos.z << ')';
            objects_.fail(ObjectError::Unsupported, detail.str());
            return DE_ERROR;
        }
        return DE_OK;
    }
    // Later LithTech CWorldServerBSP::IsOutsideWorld: outside the DAT world extents padded by 100.
    bool ServerAdapter::outside_world(DVector p) const
    {
        const auto* world = objects_.world;
        if (!world || world->models.empty() || !collision_ready)
            return false;
        return p.x < world->min.x - 100 || p.y < world->min.y - 100 || p.z < world->min.z - 100 ||
               p.x > world->max.x + 100 || p.y > world->max.y + 100 || p.z > world->max.z + 100;
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
            if (!r || r->life != Life::Alive || r->attachment || r->activity != OBJSTATE_ACTIVE || r->client_movement ||
                r->container() || dynamic_cast<PolyGrid*>(r->game.get()) || dynamic_cast<CProjectile*>(r->game.get()) ||
                (r->world_model && !r->world_body) ||
                (!r->world_body && r->state.m_ObjectType != OT_NORMAL && r->state.m_ObjectType != OT_MODEL))
                continue;
            ContainerState containers;
            if (!GetContainers(r->handle, object_bounds_center(*r), containers))
                return false;
            if (!(r->state.m_Flags & FLAG_GRAVITY) && vector_dot(r->velocity, r->velocity) == 0 &&
                vector_dot(r->acceleration, r->acceleration) == 0 && !containers.total)
                continue;
            if (seconds > 0.5)
                return objects_.fail(ObjectError::InvalidTime, "Physics catch-up exceeds 0.5 seconds");
            // Replacement policy: the original keeps integrating a body lost outside the world
            // (02_CityHubB's startcrate is authored over the void) until its coordinates leave
            // the engine's range. Hold it while nothing in its motion points back toward the
            // world; it has nothing to touch out there. A script can still move it back.
            if (outside_world(r->state.m_Pos) && !(r->state.m_Flags & FLAG_REMOVEIFOUTSIDE))
            {
                const auto* world = objects_.world;
                const auto pull = vector_add(r->acceleration, r->state.m_Flags & FLAG_GRAVITY ? gravity : DVector{});
                const float p[3]{r->state.m_Pos.x, r->state.m_Pos.y, r->state.m_Pos.z},
                    v[3]{r->velocity.x, r->velocity.y, r->velocity.z}, a[3]{pull.x, pull.y, pull.z},
                    lo[3]{world->min.x - 100, world->min.y - 100, world->min.z - 100},
                    hi[3]{world->max.x + 100, world->max.y + 100, world->max.z + 100};
                bool returning = false;
                for (int axis = 0; axis < 3; ++axis)
                    returning |= (p[axis] < lo[axis] && (v[axis] > 0 || a[axis] > 0)) ||
                                 (p[axis] > hi[axis] && (v[axis] < 0 || a[axis] < 0));
                if (!returning && !containers.total)
                {
                    ++outside_world_holds;
                    continue;
                }
            }
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
                DVector displacement;
                if (r->world_body || r->character_body || r->model_body)
                {
                    ++r->body_steps;
                    MovementState support;
                    const bool resting = physics.gravity && GetMovementState(r->handle, support) && support.standing &&
                                         support.ground_normal.y >= .5f;
                    if (resting && vector_dot(acceleration, support.ground_normal) < 0)
                        acceleration = vector_subtract(
                            acceleration,
                            vector_scale(support.ground_normal, vector_dot(acceleration, support.ground_normal)));
                    if (resting && r->body_properties.friction > 0)
                    {
                        const auto k = r->body_properties.friction;
                        const auto decay = std::exp(-k * dt);
                        const auto terminal = vector_scale(acceleration, 1 / k);
                        displacement =
                            vector_add(vector_scale(terminal, dt),
                                       vector_scale(vector_subtract(r->velocity, terminal), (1 - decay) / k));
                        r->velocity = vector_add(terminal, vector_scale(vector_subtract(r->velocity, terminal), decay));
                        if (vector_dot(r->velocity, r->velocity) < .1f && vector_dot(acceleration, acceleration) < .1f)
                            r->velocity = {};
                    }
                    else
                    {
                        displacement =
                            vector_add(vector_scale(r->velocity, dt), vector_scale(acceleration, .5f * dt * dt));
                        r->velocity = vector_add(r->velocity, vector_scale(acceleration, dt));
                    }
                }
                else
                {
                    r->velocity = vector_add(r->velocity, vector_scale(acceleration, dt));
                    displacement = vector_scale(r->velocity, dt);
                }
                auto destination = vector_add(r->state.m_Pos, displacement);
                // A very short real-clock frame can round an integrated body's
                // destination back to its current float position. Keep its
                // integrated velocity, but do not turn that lack of motion into
                // a placement query (authored bodies may still be leaving a face).
                // Explicit MoveObject queries and every representable move stay strict.
                if (destination.x == r->state.m_Pos.x && destination.y == r->state.m_Pos.y &&
                    destination.z == r->state.m_Pos.z)
                    continue;
                if (const auto moved = MoveObject(r->handle, &destination); moved != DE_OK)
                {
                    // Replacement policy: a touch-only pickup that cannot move (it starts inside
                    // geometry) never fails the session step. It gets one bounded separation; if
                    // that changes nothing it stops falling where it is, still collectible, and
                    // the failure stays recorded. Every other body keeps the strict failure.
                    auto* pickup = dynamic_cast<PickupObject*>(r->game.get());
                    // Same policy for a gravity CDestructableModel authored overlapping geometry
                    // (MoveObjectToGround keeps its pose): it stays where it was placed, as the
                    // original's downward-only helper left it. No separation is attempted.
                    if (!pickup && r->life == Life::Alive &&
                        Objects::is_kind_of(r->type, objects_.find_class("CDestructableModel")))
                    {
                        r->state.m_Flags &= ~FLAG_GRAVITY;
                        r->velocity = r->acceleration = {};
                        ++model_motion_stops;
                        break;
                    }
                    if (!pickup || r->life != Life::Alive)
                        return objects_.fail(ObjectError::Unsupported,
                                             "Physics MoveObject failed: " + r->type->name + " / " + r->state.m_Name +
                                                 " at " + std::to_string(r->state.m_Pos.x) + "," +
                                                 std::to_string(r->state.m_Pos.y) + "," +
                                                 std::to_string(r->state.m_Pos.z) + " result=" + std::to_string(moved));
                    const auto nudges = r->placement_nudges;
                    if (ResolveInitialWorldOverlap(r->handle) != DE_OK || r->placement_nudges == nudges)
                    {
                        r->state.m_Flags &= ~FLAG_GRAVITY;
                        r->velocity = r->acceleration = {};
                        pickup->error = "Cannot move out of geometry; gravity stopped";
                    }
                    ++pickup_motion_stops;
                    break;
                }
                // Original UpdateObjectPhysics: removed once a move leaves it outside the world.
                if (r->life == Life::Alive && (r->state.m_Flags & FLAG_REMOVEIFOUTSIDE) &&
                    outside_world(r->state.m_Pos))
                {
                    if (!objects_.remove(r->handle))
                        return false;
                    ++outside_world_removals;
                    break;
                }
            }
        }
        return objects_.finish_frame();
    }
} // namespace OpenBlood2
