#include "engine/collision.h"
#include "engine/objects.h"
#include <cmath>
namespace OpenBlood2
{
    namespace
    {
        bool normalize(DVector v, DVector& out)
        {
            const auto length = std::hypot(v.x, v.y, v.z);
            if (!physics_vector(v) || !std::isfinite(length) || length < 1e-6f)
                return false;
            out = vector_scale(v, 1 / length);
            return true;
        }
    } // namespace
    static DVector cross(DVector a, DVector b)
    {
        return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
    }
    // Blood II's AlignRotation contract supplies another up vector for parallel
    // axes. The later gr_GetPerpendicularVector corroborates this perturbation
    // order/threshold. Keep this engine's existing +X right convention.
    bool alignment_axes(DVector direction, DVector up, DVector& forward, DVector& right, bool& fallback)
    {
        fallback = false;
        if (!normalize(direction, forward) || !normalize(up, up))
            return false;
        auto parallel = [&]() { return std::abs(vector_dot(forward, up)) > .99f; };
        if (parallel())
        {
            fallback = true;
            up.z += 5;
            if (!normalize(up, up))
                return false;
            if (parallel())
            {
                up.x += 5;
                up.y += 5;
                if (!normalize(up, up))
                    return false;
                if (parallel())
                {
                    up.x += 5;
                    up.y -= 2;
                    up.z -= 5;
                    if (!normalize(up, up))
                        return false;
                }
            }
        }
        return normalize(cross(up, forward), right);
    }
    bool align_rotation(DVector forward, DVector up, DRotation& rotation)
    {
        DVector f, right;
        bool fallback = false;
        DRotation base;
        if (!alignment_axes(forward, up, f, right, fallback) || !align_rotation(f, base))
            return false;
        const auto base_right = rotate_vector(base, {1, 0, 0});
        const auto base_up = rotate_vector(base, {0, 1, 0});
        const float roll = std::atan2(vector_dot(right, base_up), vector_dot(right, base_right));
        rotation = compose_rotation(base, rotation_from_euler({0, 0, roll}));
        return true;
    }
    bool align_rotation(DVector forward, DRotation& rotation)
    {
        if (!physics_vector(forward) || vector_dot(forward, forward) < 1e-12f)
            return false;
        const float horizontal = std::hypot(forward.x, forward.z);
        rotation = rotation_from_euler(
            {std::atan2(-forward.y, horizontal), horizontal > 1e-6f ? std::atan2(forward.x, forward.z) : 0, 0});
        return true;
    }
    bool object_node_transform(const ObjectRecord& input, const char* name, DVector* position, DRotation* rotation)
    {
        const auto* record = &input;
        // InitialUpdate may query the bound model; pending removals may not.
        if (!record || record->life != Life::Alive || !record->model || record->state.m_ObjectType != OT_MODEL ||
            !name || !*name || !position || !rotation)
            return DFALSE;
        const auto& model = *record->model;
        auto equal = [](std::string_view a, std::string_view b)
        {
            if (a.size() != b.size())
                return false;
            for (std::size_t i = 0; i < a.size(); ++i)
            {
                auto fold = [](unsigned char c) { return c >= 'A' && c <= 'Z' ? c + ('a' - 'A') : c; };
                if (fold(a[i]) != fold(b[i]))
                    return false;
            }
            return true;
        };
        // LithTech Model::FindNode: duplicate names resolve to the first node in model order.
        std::size_t found = 0;
        while (found < model.nodes.size() && !equal(model.nodes[found].name, name))
            ++found;
        NodePose pose;
        const auto scale = record->state.m_Scale;
        if (scale.x <= 0 || scale.y <= 0 || scale.z <= 0 || !physics_vector(scale) ||
            !sample_node_pose(model, record->animation.animation, record->animation.time, found, pose))
            return DFALSE;
        auto transformed =
            rotate_vector(record->state.m_Rotation,
                          {pose.position.x * scale.x, pose.position.y * scale.y, pose.position.z * scale.z});
        transformed.x += record->state.m_Pos.x;
        transformed.y += record->state.m_Pos.y;
        transformed.z += record->state.m_Pos.z;
        if (!physics_vector(transformed))
            return DFALSE;
        *position = transformed;
        *rotation = compose_rotation(record->state.m_Rotation, pose.rotation);
        return DTRUE;
    }
} // namespace OpenBlood2
