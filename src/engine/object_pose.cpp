#include "engine/collision.h"
#include "engine/objects.h"
namespace OpenBlood2
{
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
        std::size_t found = model.nodes.size();
        for (std::size_t i = 0; i < model.nodes.size(); ++i)
            if (equal(model.nodes[i].name, name))
            {
                if (found != model.nodes.size())
                    return DFALSE; // Ambiguous names never choose an arbitrary node.
                found = i;
            }
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
