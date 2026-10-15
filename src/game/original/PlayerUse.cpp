#include "Door.h"
#include "PlayerObj.h"
#include "RotatingDoor.h"
#include "engine/collision.h"
#include <cmath>
using namespace OpenBlood2;

// PlayerObj.cpp OnCommandOn(COMMAND_USE), ordinary single-player branch.
// Dead multiplayer respawn / slow-death rescue are outside the accepted session.
bool CPlayerObj::OnCommandUse()
{
    if (!g_pServerDE->IsObjectAlive(m_hObject) || movement_blocked || camera_movement_blocked)
        return false;
    ++use_commands;
    use_hit = {};
    use_door_refused = false;
    g_pServerDE->GetObjectPos(m_hObject, &use_from);
    use_from.y += eye_level;
    const float cp = std::cos(m_fPitch);
    const DVector forward{std::sin(m_fYaw) * cp, -std::sin(m_fPitch), std::cos(m_fYaw) * cp};
    use_to = vector_add(use_from, vector_scale(forward, 82));
    // Self is excluded explicitly: the source ray normally starts inside its
    // player box and the old engine reports entry faces, not interior starts.
    use_result = g_pServerDE->IntersectSegment(use_from, use_to, m_hObject, use_hit, false, SegmentObjects::All);
    if (use_result == SegmentResult::Invalid || use_result == SegmentResult::Unavailable)
        return false;
    if (use_result != SegmentResult::Hit || !use_hit.object)
        return true;
    use_door_refused =
        g_pServerDE->IsKindOf(g_pServerDE->GetObjectClass(use_hit.object), g_pServerDE->GetClass("Door"));
    if (use_door_refused)
        return true;
    auto message = g_pServerDE->StartMessageToObject(this, use_hit.object, 1000);
    if (!message)
        return false;
    const auto written = g_pServerDE->WriteToMessageHString(message, m_hstrTrigger);
    const auto sent = g_pServerDE->EndMessage(message);
    if (written != DE_OK || sent != DE_OK)
        return false;
    ++use_messages;
    return true;
}

// PlayerObj.cpp ObjectTouch / UpdateClientPhysics rotating-door push branch.
void CPlayerObj::ObjectTouchDoor(HOBJECT object)
{
    if (door_push_pending || !g_pServerDE->IsObjectAlive(object))
        return;
    const auto* door = dynamic_cast<RotatingDoor*>(g_pServerDE->HandleToObject(object));
    if (!door || door->state != Door::Opening || !door->push_player_back)
        return;
    DRotation rotation;
    g_pServerDE->GetObjectRotation(m_hObject, &rotation);
    door_push_velocity = rotate_vector({0, 0, -500}, rotation);
    door_push_pending = true;
    ++door_pushes;
}
bool CPlayerObj::ApplyDoorPush()
{
    if (!door_push_pending)
        return true;
    DVector velocity;
    g_pServerDE->GetVelocity(m_hObject, &velocity);
    velocity = vector_add(velocity, door_push_velocity);
    if (g_pServerDE->SetVelocity(m_hObject, &velocity) != DE_OK)
        return false;
    door_push_pending = false;
    return true;
}
