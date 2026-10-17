#include "engine/world_model.h"
#include "game/server_adapter.h"
#include <algorithm>
#include <cmath>

namespace OpenBlood2
{
    MoverResult ServerAdapter::RotateWorldModel(HOBJECT handle, const RotationMove& move)
    {
        trace_service("RotateWorldModel");
        auto* mover = objects_.get(handle);
        if (!mover || mover->life != Life::Alive || !mover->world_model_base ||
            (mover->world_body && !mover->body_properties.rotating_debris) || moving_ ||
            !physics_vector(move.from_angles) || !physics_vector(move.to_angles) ||
            !physics_vector(move.original_position) || !physics_vector(move.pivot))
            return MoverResult::Invalid;
        if (!collision_ready || !collision)
            return MoverResult::Unavailable;
        const auto from_rotation = rotation_from_euler(move.from_angles);
        const auto alignment = std::abs(vector_dot(from_rotation.m_Vec, mover->state.m_Rotation.m_Vec) +
                                        from_rotation.m_Spin * mover->state.m_Rotation.m_Spin);
        if (alignment < .99999f)
            return MoverResult::Invalid;
        const auto angles = vector_subtract(move.to_angles, move.from_angles);
        const auto arm = vector_subtract(move.original_position, move.pivot);
        const auto residual =
            vector_subtract(mover->state.m_Pos, vector_add(move.pivot, rotate_vector(arm, from_rotation)));
        // Source RotateObject rotates a world model about its own position;
        // it does not require Door's translation-only GOTHRUWORLD flag.
        if (!(mover->state.m_Flags & FLAG_GOTHRUWORLD) &&
            (vector_dot(arm, arm) != 0 || vector_dot(residual, residual) != 0))
            return MoverResult::Unavailable;
        auto length = [](DVector v) { return std::sqrt(vector_dot(v, v)); };
        float radius = 0;
        const auto& base = *mover->world_model_base;
        for (const auto& triangle : base.triangles)
            for (auto point : triangle)
                radius = std::max(radius, length(vector_subtract(point, base.origin)));
        // Euler path length bounds every vertex's arc, including multi-turn moves.
        const float travel =
            (radius + length(arm)) * (std::abs(angles.x) + std::abs(angles.y) + std::abs(angles.z)) + length(residual);
        if (!std::isfinite(travel))
            return MoverResult::Invalid;
        if (travel == 0)
            return MoverResult::Moved;
        if (travel > 256 || base.triangles.size() * std::ceil(travel / .5f) > 262144)
        {
            objects_.fail(ObjectError::Limit, "Rotation exceeds 512 steps / 262144 triangle steps; subdivide time");
            return MoverResult::Unavailable;
        }
        const auto count = static_cast<unsigned>(std::max(1.0f, std::ceil(travel / .5f)));
        mover->rotation_steps = count;
        const float pad = travel / static_cast<float>(count) + .002f;
        // Pure yaw cannot move any vertex vertically. Avoid falsely dragging
        // a body standing above the top face with DONTFOLLOWSTANDING / jumping.
        const DVector padding{
            pad, angles.x == 0 && angles.z == 0 ? std::abs(residual.y) / static_cast<float>(count) + .00001f : pad,
            pad};
        const auto ids = objects_.snapshot();
        struct Body
        {
            ObjectRecord* object;
            DVector position;
            bool rider;
            bool moved = false;
            std::vector<DVector> path;
        };
        std::vector<Body> bodies;
        auto blocked = [&](HOBJECT other)
        {
            ++mover->mover_blocks;
            mover->mover_blocker = other;
            return MoverResult::Blocked;
        };
        if (mover->state.m_Flags & FLAG_SOLID)
            for (auto id : ids)
            {
                auto* body = objects_.get(id);
                if (!body || body->life != Life::Alive || body == mover || !(body->state.m_Flags & FLAG_SOLID))
                    continue;
                MovementState support;
                const bool rider = !body->world_model && !(body->state.m_Flags & FLAG_DONTFOLLOWSTANDING) &&
                                   GetMovementState(body->handle, support) && support.standing &&
                                   support.ground_object == handle;
                bodies.push_back({body, body->state.m_Pos, rider, false, {}});
            }
        auto previous = mover->world_model;
        auto old_position = mover->state.m_Pos;
        auto old_rotation = mover->state.m_Rotation;
        std::shared_ptr<ContainerShape> shape;
        DRotation rotation;
        DVector position;
        std::size_t affected = 0;
        for (unsigned step = 1; step <= count; ++step)
        {
            const auto t = static_cast<float>(step) / static_cast<float>(count);
            rotation = rotation_from_euler(vector_add(move.from_angles, vector_scale(angles, t)));
            position = vector_add(vector_add(move.pivot, rotate_vector(arm, rotation)), vector_scale(residual, 1 - t));
            shape = std::make_shared<ContainerShape>(base);
            if (!shape->transform(multiply_rotation(rotation, inverse_rotation(mover->world_model_basis)), position))
                return MoverResult::Invalid;
            if (mover->world_body)
            {
                // Unlike a kinematic brush, free debris must not rotate into the
                // primary world. Sweep a conservative bound over this angular step.
                const auto center = vector_scale(vector_add(shape->min, shape->max), .5f);
                const auto previous_center = vector_add(vector_scale(vector_add(previous->min, previous->max), .5f),
                                                        vector_subtract(old_position, previous->origin));
                const auto old_half = vector_scale(vector_subtract(previous->max, previous->min), .5f);
                const auto half = vector_scale(vector_subtract(shape->max, shape->min), .5f);
                const DVector extent{std::max(old_half.x, half.x) + padding.x, std::max(old_half.y, half.y) + padding.y,
                                     std::max(old_half.z, half.z) + padding.z};
                CollisionHit hit;
                const auto result = collision->sweep(previous_center, center, extent, hit);
                if (result == CollisionQuery::Invalid)
                    return MoverResult::Unavailable;
                if (result == CollisionQuery::Hit)
                    return blocked(nullptr);
            }
            const auto delta_rotation = multiply_rotation(rotation, inverse_rotation(old_rotation));
            for (auto& body : bodies)
            {
                auto& record = *body.object;
                CollisionHit contact;
                const auto old_offset = vector_subtract(old_position, previous->origin);
                const auto local = vector_subtract(body.position, old_offset);
                const auto expanded = vector_add(record.dimensions, padding);
                const bool box = (mover->state.m_Flags & FLAG_BOXPHYSICS) != 0;
                const auto old_center = vector_scale(vector_add(previous->min, previous->max), .5f);
                const auto old_half = vector_scale(vector_subtract(previous->max, previous->min), .5f);
                const auto query = box ? sweep_box(local, local, expanded, old_center, old_half, contact)
                                       : previous->sweep(local, local, expanded, contact);
                if (record.world_model)
                {
                    const auto center = vector_add(old_center, old_offset);
                    if (sweep_object(record, center, center, vector_add(old_half, padding), contact) !=
                        CollisionQuery::Miss)
                        return blocked(record.handle);
                    continue;
                }
                if (!body.rider && query == CollisionQuery::Miss)
                    continue;
                if (query == CollisionQuery::Invalid ||
                    (record.state.m_ObjectType != OT_NORMAL && record.state.m_ObjectType != OT_MODEL))
                    return blocked(record.handle);
                if (!body.moved && ++affected > 64)
                    return blocked(record.handle);
                auto end =
                    vector_add(position, rotate_vector(vector_subtract(body.position, old_position), delta_rotation));
                const auto displacement = vector_subtract(end, body.position);
                const auto distance = length(displacement);
                auto overlaps = [&](DVector point)
                {
                    CollisionHit hit;
                    if (box)
                        return sweep_box(point, point, record.dimensions,
                                         vector_scale(vector_add(shape->min, shape->max), .5f),
                                         vector_scale(vector_subtract(shape->max, shape->min), .5f),
                                         hit) != CollisionQuery::Miss;
                    return shape->sweep(point, point, record.dimensions, hit) != CollisionQuery::Miss;
                };
                // AABBs do not rotate with their centers. A bounded forward
                // separation handles that corner growth; otherwise reject atomically.
                for (unsigned attempt = 0; overlaps(end) && distance > 1e-6f && attempt < 8; ++attempt)
                    end = vector_add(end, vector_scale(displacement, pad / distance));
                if (overlaps(end))
                    return blocked(record.handle);
                CollisionHit obstruction;
                const auto result =
                    sweep_scene(record.handle, body.position, end, record.dimensions, obstruction, ids, handle);
                if (result == CollisionQuery::Invalid ||
                    (result == CollisionQuery::Hit && (obstruction.start_solid || obstruction.fraction < 1 - 1e-6)))
                    return blocked(result == CollisionQuery::Hit ? obstruction.object : record.handle);
                if (!body.moved)
                    body.path.push_back(body.position);
                body.path.push_back(end);
                body.moved = true;
                body.position = end;
            }
            // Validate staged bodies against each other, as well as the old
            // scene above. No partial commit and no speculative callbacks.
            for (std::size_t i = 0; i < bodies.size(); ++i)
                if (bodies[i].moved)
                    for (std::size_t j = i + 1; j < bodies.size(); ++j)
                        if (!bodies[j].object->world_model)
                        {
                            CollisionHit hit;
                            if (sweep_box(bodies[i].position, bodies[i].position, bodies[i].object->dimensions,
                                          bodies[j].position, bodies[j].object->dimensions, hit) == CollisionQuery::Hit)
                                return blocked(bodies[j].object->handle);
                        }
            previous = shape;
            old_rotation = rotation;
            old_position = position;
        }
        moving_ = true;
        mover->world_model = std::move(shape);
        mover->dimensions = vector_scale(vector_subtract(mover->world_model->max, mover->world_model->min), .5f);
        mover->state.m_Pos = position;
        mover->state.m_Rotation = rotation;
        ++mover->mover_moves;
        for (auto& body : bodies)
            if (body.moved)
            {
                body.object->state.m_Pos = body.position;
                body.object->grounded = false;
                body.object->contact_object = nullptr;
                body.object->contact_polygon = UINT32_MAX;
                body.object->contact_normal = {};
                ++mover->mover_pushes;
                mover->mover_riders += body.rider;
            }
        for (auto& body : bodies)
            if (body.moved)
            {
                auto& record = *body.object;
                if (mover->life == Life::Alive && record.life == Life::Alive)
                    objects_.engine_message(handle, MID_TOUCHNOTIFY, record.handle);
                if (mover->life == Life::Alive && record.life == Life::Alive &&
                    (record.state.m_Flags & FLAG_TOUCH_NOTIFY))
                    objects_.engine_message(record.handle, MID_TOUCHNOTIFY, handle);
                for (auto id : ids)
                {
                    auto* sensor = objects_.get(id);
                    if (record.life != Life::Alive)
                        break;
                    if (!sensor || sensor->life != Life::Alive || sensor == &record || sensor->container() ||
                        sensor->world_model || (sensor->state.m_Flags & FLAG_SOLID) ||
                        !(sensor->state.m_Flags & FLAG_TOUCH_NOTIFY))
                        continue;
                    bool touched = false;
                    for (std::size_t i = 1; i < body.path.size() && !touched; ++i)
                    {
                        CollisionHit hit;
                        touched = sweep_object(*sensor, body.path[i - 1], body.path[i], record.dimensions, hit) ==
                                  CollisionQuery::Hit;
                    }
                    if (touched)
                    {
                        if (record.state.m_Flags & FLAG_TOUCH_NOTIFY)
                            objects_.engine_message(record.handle, MID_TOUCHNOTIFY, sensor->handle);
                        if (record.life == Life::Alive && sensor->life == Life::Alive)
                            objects_.engine_message(sensor->handle, MID_TOUCHNOTIFY, record.handle);
                    }
                }
            }
        moving_ = false;
        return MoverResult::Moved;
    }
} // namespace OpenBlood2
