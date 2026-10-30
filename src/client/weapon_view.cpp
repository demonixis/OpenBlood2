#include "client/weapon_view.h"
#include "engine/collision.h"
#include "game/original/WeaponDefs.h"
#include <cmath>

using namespace OpenBlood2;
bool ClientWeaponView::AdjustAngleAndSetPos(float pitch, float yaw, DVector eye, NodePose& out) const
{
    out = {};
    const auto* data = weapon_definition(type);
    if (!model || !data || !std::isfinite(pitch) || !std::isfinite(yaw) || !physics_vector(eye))
        return false;
    out.rotation = rotation_from_euler({pitch, yaw, 0});
    const DVector offset{left ? -data->m_vViewModelOffset.x : data->m_vViewModelOffset.x, data->m_vViewModelOffset.y,
                         data->m_vViewModelOffset.z};
    out.position = vector_add(eye, rotate_vector(out.rotation, offset));
    return physics_vector(out.position);
}
DVector ClientWeaponView::FlashOffset(bool original_projection) const
{
    const auto* data = weapon_definition(type);
    if (!model || !data)
        return {};
    DVector fallback{data->m_vFlash.x, data->m_vFlash.y, data->m_vFlash.z};
    if (left)
        fallback.x = -fallback.x;
    std::size_t index = 0;
    while (index < model->nodes.size() && model->nodes[index].name != "muzzle")
        ++index;
    NodePose node;
    if (index < model->nodes.size() && sample_node_pose(*model, animation.animation, animation.time, index, node))
    {
        if (!original_projection)
            return node.position;
        const auto offset = DVector{left ? -data->m_vViewModelOffset.x : data->m_vViewModelOffset.x,
                                    data->m_vViewModelOffset.y, data->m_vViewModelOffset.z};
        return vector_scale(vector_add(node.position, offset), 10);
    }
    return original_projection ? fallback : vector_scale(fallback, .1f);
}
