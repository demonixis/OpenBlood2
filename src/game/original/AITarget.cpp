// Original AI_Mgr.cpp:716-722,1269-1314,2200,5556-5608.
// See AI_TARGET_CONTRACTS.md for checked failures and current-transform adaptation.
#include "AI_Mgr.h"
#include "engine/collision.h"
#include <cmath>
#include <limits>

bool AI_Mgr::TargetFailure(const char* error)
{
    ++target_failures;
    target_error = error;
    return false;
}
bool AI_Mgr::SetNewTarget(HOBJECT target)
{
    auto* server = GetServerDE();
    target_error.clear();
    if (!server || !server->IsObjectAlive(m_hObject) ||
        (target && (!server->IsObjectAlive(target) || target == m_hObject)))
        return TargetFailure("Target assignment requires distinct live objects");
    if (target == m_hTarget)
        return true; // Source same-target no-op: no link or position refresh.

    // Validate/acquire the new link before releasing the old one. A rejected
    // service must not lose the original target. Link creation has no callbacks.
    if (target && server->CreateInterObjectLink(m_hObject, target) != DE_OK)
        return TargetFailure("Target link creation failed; previous target retained");
    if (m_hTarget)
        server->BreakInterObjectLink(m_hObject, m_hTarget);
    m_hTarget = target;
    target_position_valid = false;
    ++target_changes;
    return true;
}
bool AI_Mgr::RefreshTargetPosition()
{
    auto* server = GetServerDE();
    target_error.clear();
    DVector position;
    if (!server || !server->IsObjectAlive(m_hObject) || !server->IsObjectAlive(m_hTarget) ||
        server->GetObjectPos(m_hTarget, &position) != DE_OK)
    {
        target_position_valid = false;
        return TargetFailure("Target position requires live observer and target with a resolved transform");
    }
    m_vTargetPos = position;
    target_position_valid = true;
    return true;
}
bool AI_Mgr::SetTarget(std::string_view name)
{
    auto* server = GetServerDE();
    target_error.clear();
    target_name.assign(name.substr(0, 100));
    if (!server || !server->IsObjectAlive(m_hObject) || name.empty() || name.size() > 100 ||
        target_name.find('\0') != target_name.npos)
        return TargetFailure("Invalid named target request");
    auto* list = server->FindNamedObjects(target_name.c_str());
    if (!list)
        return TargetFailure("Named target list unavailable");
    const auto target = list->m_pFirstLink ? list->m_pFirstLink->m_hObject : nullptr;
    // Copy the checked identity; never keep a list or raw game-object pointer.
    server->RelinquishList(list);
    if (!target)
        return TargetFailure("No matching named target; previous target retained");
    if (!SetNewTarget(target) || !RefreshTargetPosition() || !MC_FaceTarget())
        return false;
    --Metacmd; // SetTarget cancels MC_FaceTarget's original command increment.
    return true;
}
bool AI_Mgr::MC_FaceTarget()
{
    if (m_hTarget)
    {
        auto* server = GetServerDE();
        if (!server || !server->IsObjectAlive(m_hTarget) || !target_position_valid)
            return TargetFailure("Face target requires a live target and a sampled position");
        // Original method uses the stored position, not a new target query.
        return MC_FacePos(m_vTargetPos);
    }
    target_error.clear();
    auto* server = GetServerDE();
    if (!server || !server->IsObjectAlive(m_hObject) || Metacmd == std::numeric_limits<int>::max())
        return TargetFailure("Face command owner or command counter unavailable");
    ++Metacmd;
    return true;
}
bool AI_Mgr::MC_FacePos(DVector position)
{
    auto* server = GetServerDE();
    target_error.clear();
    DVector origin;
    DRotation rotation;
    if (!server || !server->IsObjectAlive(m_hObject) || Metacmd == std::numeric_limits<int>::max() ||
        server->GetObjectPos(m_hObject, &origin) != DE_OK || server->GetObjectRotation(m_hObject, &rotation) != DE_OK)
        return TargetFailure("Face command owner, transform or command counter unavailable");
    auto finite = [](DVector v) { return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z); };
    const auto delta = OpenBlood2::vector_subtract(position, origin);
    if (!finite(position) || !finite(delta) || (delta.x == 0 && delta.y == 0 && delta.z == 0))
        return TargetFailure("Face position is nonfinite or coincides with the observer");
    // Normalizing vDir in the source does not change atan2(x,z). Avoid its
    // zero/overflow hazards. Preserve the source's raw (unwrapped) yaw delta
    // and rotation about current local up, including pitched/rolled owners.
    const auto forward = OpenBlood2::rotate_vector({0, 0, 1}, rotation);
    const float angle = std::atan2(delta.x, delta.z) - std::atan2(forward.x, forward.z);
    const auto result = OpenBlood2::compose_rotation(rotation, {{0, std::sin(angle * .5f), 0}, std::cos(angle * .5f)});
    if (server->SetObjectRotation(m_hObject, &result) != DE_OK)
        return TargetFailure("Face rotation service rejected the request");
    face_position = position;
    face_yaw_delta = angle;
    ++face_commands;
    ++Metacmd;
    return true;
}
DDWORD AI_Mgr::EngineMessageFn(DDWORD message, void* data, DFLOAT value)
{
    if (message == MID_LINKBROKEN && data == m_hTarget && m_hTarget)
    {
        m_hTarget = nullptr;
        target_position_valid = false;
        ++target_link_breaks;
    }
    if (message == MID_LINKBROKEN && data == m_hTrackObject && m_hTrackObject)
    {
        m_hTrackObject = nullptr;
        tracked_position_valid = false;
        ++tracked_invalidations;
    }
    return CBaseCharacter::EngineMessageFn(message, data, value);
}

// Source Escape states assign a non-owning track identity without creating a link.
// Stable checked tokens also handle removal when no MID_LINKBROKEN can be emitted.
bool AI_Mgr::SetTrackedObject(HOBJECT object)
{
    auto* server = GetServerDE();
    if (!server || !server->IsObjectAlive(m_hObject) ||
        (object && (object == m_hObject || !server->IsObjectAlive(object))))
        return TargetFailure("Tracked assignment requires distinct live objects");
    m_hTrackObject = object;
    tracked_position_valid = false;
    ++tracked_changes;
    return true;
}
HOBJECT AI_Mgr::GetTrackedObject() const
{
    auto* server = GetServerDE();
    return server && server->IsObjectAlive(m_hObject) && server->IsObjectAlive(m_hTrackObject) ? m_hTrackObject
                                                                                               : nullptr;
}
bool AI_Mgr::RefreshTrackedPosition()
{
    auto* server = GetServerDE();
    const auto object = GetTrackedObject();
    DVector position;
    if (!object || server->GetObjectPos(object, &position) != DE_OK)
    {
        tracked_position_valid = false;
        if (!object && m_hTrackObject)
        {
            m_hTrackObject = nullptr;
            ++tracked_invalidations;
        }
        return TargetFailure("Tracked pose unavailable; historical coordinates retained");
    }
    m_vTrackObjPos = position;
    tracked_position_valid = true;
    return true;
}
