#include "game/server_adapter.h"
#include <algorithm>
#include <cmath>
namespace OpenBlood2
{
    namespace
    {
        bool normalize(DRotation& q)
        {
            if (!physics_vector(q.m_Vec) || !std::isfinite(q.m_Spin))
                return false;
            const float norm = std::sqrt(vector_dot(q.m_Vec, q.m_Vec) + q.m_Spin * q.m_Spin);
            if (!std::isfinite(norm) || norm < 1e-6f)
                return false;
            q = {vector_scale(q.m_Vec, 1 / norm), q.m_Spin / norm};
            return true;
        }
    } // namespace
    DRESULT ServerAdapter::InterpolateRotation(DRotation& out, const DRotation& from, const DRotation& to, float t)
    {
        trace_service("InterpolateRotation");
        auto a = from, b = to;
        if (!std::isfinite(t) || t < 0 || t > 1 || !normalize(a) || !normalize(b))
            return DE_INVALIDPARAMS;
        float dot = vector_dot(a.m_Vec, b.m_Vec) + a.m_Spin * b.m_Spin;
        if (dot < 0)
        {
            b = {vector_scale(b.m_Vec, -1), -b.m_Spin};
            dot = -dot;
        }
        dot = std::clamp(dot, 0.0f, 1.0f);
        float wa = 1 - t, wb = t;
        if (dot < .9995f)
        {
            const float theta = std::acos(dot), sine = std::sin(theta);
            wa = std::sin((1 - t) * theta) / sine;
            wb = std::sin(t * theta) / sine;
        }
        DRotation q{vector_add(vector_scale(a.m_Vec, wa), vector_scale(b.m_Vec, wb)), a.m_Spin * wa + b.m_Spin * wb};
        if (!normalize(q))
            return DE_INVALIDPARAMS;
        out = q;
        return DE_OK;
    }
    MoverResult ServerAdapter::MoveKeyframedObject(HOBJECT h, DVector position, const DRotation* rotation)
    {
        trace_service("MoveKeyframedObject");
        const auto* record = objects_.get(h);
        DRotation q = rotation ? *rotation : DRotation{};
        if (!record || record->life != Life::Alive || !physics_vector(position) || (rotation && !normalize(q)))
            return MoverResult::Invalid;
        if (record->attachment || record->container || record->client_movement)
            return MoverResult::Unavailable;
        if (record->world_model)
        {
            // A translation can reuse the atomic pusher/rider transaction. Never
            // translate then discover an unsupported angular request afterwards.
            auto current = record->state.m_Rotation;
            if (!record->world_model_base || record->world_body || !normalize(current))
                return MoverResult::Unavailable;
            const auto difference =
                rotation ? std::min(vector_dot(vector_subtract(current.m_Vec, q.m_Vec),
                                               vector_subtract(current.m_Vec, q.m_Vec)) +
                                        (current.m_Spin - q.m_Spin) * (current.m_Spin - q.m_Spin),
                                    vector_dot(vector_add(current.m_Vec, q.m_Vec), vector_add(current.m_Vec, q.m_Vec)) +
                                        (current.m_Spin + q.m_Spin) * (current.m_Spin + q.m_Spin))
                         : 0;
            if (difference > 1e-12f)
                return MoverResult::Unavailable;
            const auto result = MoveWorldModel(h, position);
            return IsObjectAlive(h) ? result : MoverResult::Invalid;
        }
        if (record->state.m_ObjectType != OT_NORMAL && record->state.m_ObjectType != OT_MODEL)
            return MoverResult::Unavailable;
        if (!collision_ready || !collision)
            return MoverResult::Unavailable;
        if (MoveObject(h, &position) != DE_OK)
            return MoverResult::Blocked;
        if (!IsObjectAlive(h))
            return MoverResult::Invalid; // A contact may remove the body.
        if (rotation && SetObjectRotation(h, &q) != DE_OK)
            return MoverResult::Invalid;
        const auto delta = vector_subtract(objects_.get(h)->state.m_Pos, position);
        return vector_dot(delta, delta) < 1e-6f ? MoverResult::Moved : MoverResult::Blocked;
    }
} // namespace OpenBlood2
