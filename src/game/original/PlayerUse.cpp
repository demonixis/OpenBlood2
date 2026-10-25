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
    if (!g_pServerDE->IsObjectAlive(m_hObject) || MovementBlocked() || camera_movement_blocked ||
        (m_InventoryMgr.eye && m_InventoryMgr.eye->active))
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
bool CPlayerObj::ObjectTouchDoor(HOBJECT object)
{
    if (door_push_pending || !g_pServerDE->IsObjectAlive(object))
        return false;
    const auto* door = dynamic_cast<RotatingDoor*>(g_pServerDE->HandleToObject(object));
    if (!door || door->state != Door::Opening || !door->push_player_back)
        return false;
    DRotation rotation;
    g_pServerDE->GetObjectRotation(m_hObject, &rotation);
    door_push_velocity = rotate_vector({0, 0, -500}, rotation);
    door_push_pending = true;
    ++door_pushes;
    return true;
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

bool CPlayerObj::OnCommandItem(DBYTE type)
{
    // Original OnCommandOn COMMAND_INVITEM_1, including its run-key exclusion.
    const bool eye_off = type == 4 && m_InventoryMgr.eye && m_InventoryMgr.eye->active;
    if (!g_pServerDE->IsObjectAlive(m_hObject) || m_bDead || m_damage.IsDead() ||
        (!eye_off &&
         (MovementBlocked() || camera_movement_blocked || (type != 7 && type != 8 && type != 9 && command_run))))
        return false;
    return m_InventoryMgr.SetActiveItem(type) == InventoryResult::Ok;
}

// Original OnCommandOn COMMAND_NEXTINVENTORY / COMMAND_PREVINVENTORY.
bool CPlayerObj::OnCommandItemCursor(bool next)
{
    if (!g_pServerDE->IsObjectAlive(m_hObject) || m_bDead || m_damage.IsDead() || MovementBlocked() ||
        camera_movement_blocked)
        return false;
    next ? m_InventoryMgr.SelectNextItem() : m_InventoryMgr.SelectPrevItem();
    return true;
}
// Original OnCommandOn COMMAND_USEINVENTORY: no run-key exclusion, and an active Eye lets
// the command through a movement block (it then uses the current item, whatever it is).
bool CPlayerObj::OnCommandUseInventory()
{
    const bool eye_active = m_InventoryMgr.eye && m_InventoryMgr.eye->active;
    if (!g_pServerDE->IsObjectAlive(m_hObject) || m_bDead || m_damage.IsDead() ||
        (!eye_active && (MovementBlocked() || camera_movement_blocked)))
        return false;
    const auto item = m_InventoryMgr.GetCurrentItem();
    return item && m_InventoryMgr.SetActiveItem(item->type) == InventoryResult::Ok;
}
bool CPlayerObj::UpdateInventoryClient(bool force)
{
    auto items = m_InventoryMgr.ItemView();
    if (!force && !items.show && last_sent_items_ == items)
        return true;
    if (GetServerDE()->SendToClient(m_hClient, items) != DE_OK)
        return false;
    items.show = false; // A one-shot request, never part of the change comparison.
    last_sent_items_ = std::move(items);
    ++inventory_item_messages;
    return true;
}
bool CPlayerObj::OnCommandAltFire()
{
    auto* s = GetServerDE();
    if (!s->IsObjectAlive(m_hObject) || m_bDead || m_damage.IsDead() || MovementBlocked() || camera_movement_blocked ||
        (m_InventoryMgr.eye && m_InventoryMgr.eye->active))
        return false;
    const auto* weapon = m_InventoryMgr.CurrentWeapon();
    if (!weapon || !weapon->data.m_bAltFireZoom ||
        (m_bZoomView && m_InventoryMgr.binoculars && m_InventoryMgr.binoculars->active))
        return false;
    OpenBlood2::SoundRequest sound;
    sound.resource = "Sounds\\Weapons\\sniper\\Zoom.wav";
    sound.speaker = m_hObject;
    sound.inner_radius = 100;
    sound.outer_radius = 1000;
    sound.priority = 1; // SOUNDPRIORITY_MISC_MEDIUM, unretained attached one-shot.
    sound.reverb_requested = true;
    zoom_sound_error.clear();
    s->PlayCharacterSound(m_hObject, sound, false, zoom_sound_error);
    SetZoomMode(!m_bZoomView, 99);
    return true;
}

// Original PlayerObj::GrabObject / DoTaunt / OnCommandOn(COMMAND_DROPWEAPON).
bool CPlayerObj::OnControlAction(const PlayerControlAction& action)
{
    control_error.clear();
    auto* s = GetServerDE();
    if (!s->IsObjectAlive(m_hObject) || m_bDead || m_damage.IsDead() || MovementBlocked() || camera_movement_blocked)
        return false;
    if (action.kind == PlayerControlAction::DropWeapon)
    {
        const auto result = m_InventoryMgr.DropCurrentWeapon();
        control_error = m_InventoryMgr.drop_error;
        return result == InventoryResult::Ok;
    }
    if (action.kind == PlayerControlAction::Taunt)
    {
        ++taunt_commands;
        return PlayVoiceGroupEventOnClient(10, true); // VME_TAUNT; VoiceMgr owns playback cooldowns.
    }
    if (action.kind != PlayerControlAction::Grab || !physics_vector(action.displacement))
        return false;
    ++grab_commands;
    grab_target = nullptr;
    DVector from;
    if (s->GetObjectPos(m_hObject, &from) != DE_OK)
        return false;
    from.y += eye_level;
    const auto forward = rotate_vector(rotation_from_euler({m_fPitch, m_fYaw, 0}), {0, 0, 1});
    SegmentHit hit;
    const auto result = s->IntersectSegment(from, vector_add(from, vector_scale(forward, 82)), m_hObject, hit, false,
                                            SegmentObjects::All);
    if (result != SegmentResult::Hit || !hit.object || !(s->GetObjectUserFlags(hit.object) & (1u << 4)))
        return result != SegmentResult::Invalid && result != SegmentResult::Unavailable;
    grab_target = hit.object;
    DVector target;
    if (s->GetObjectPos(hit.object, &target) != DE_OK)
        return false;
    target = vector_add(target, action.displacement);
    if (s->MoveObject(hit.object, &target) != DE_OK)
    {
        control_error = "Grab movement refused by collision";
        return false;
    }
    ++grab_moves;
    return true;
}
