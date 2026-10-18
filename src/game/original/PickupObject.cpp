#include "PickupObject.h"
#include "AI_Mgr.h"
#include "trigger_helpers.h"
#include <algorithm>
using namespace OpenBlood2;
PickupReport OpenBlood2::PlayPickupSound(HOBJECT source, std::string_view sound)
{
    PickupReport report;
    report.sound = sound;
    if (sound.empty())
        return report;
    if (g_pServerDE->GetObjectPos(source, &report.position) != DE_OK)
    {
        report.sound_error = "Pickup sound position unavailable";
        return report;
    }
    SoundRequest request;
    request.resource = sound;
    request.inner_radius = 30;
    request.outer_radius = 300;
    request.priority = 2; // SOUNDPRIORITY_MISC_HIGH; original helper requests reverb.
    report.voice = g_pServerDE->PlayDetachedSound(source, request, report.position, report.sound_error);
    return report;
}
DDWORD PickupObject::EngineMessageFn(DDWORD message, void* data, DFLOAT reason)
{
    if (message == MID_PRECREATE)
    {
        if (!B2BaseClass::EngineMessageFn(message, data, reason) || reason == 3 || !data || file.empty())
            return 0;
        auto& create = *static_cast<ObjectCreateStruct*>(data);
        auto text = [&](const char* key, std::string& out)
        {
            GenericProp p;
            if (g_pServerDE->GetPropGeneric(key, &p) == DE_OK)
                out = p.m_String;
        };
        text("PickupTriggerTarget", pickup_target);
        text("PickupTriggerMessage", pickup_command);
        SAFE_STRCPY(create.m_Filename, ("Models/Powerups/" + file + ".abc").c_str());
        SAFE_STRCPY(create.m_SkinName, ("Skins/Powerups/" + file + ".dtx").c_str());
        create.m_ObjectType = OT_MODEL;
        create.m_Flags |= FLAG_TOUCH_NOTIFY | FLAG_VISIBLE | FLAG_GRAVITY;
        create.m_Flags &= ~FLAG_SHADOW;
        return 1;
    }
    // A subclass may already have removed itself (original _DEMO bomb pickups).
    // The source keeps configuring the doomed object; checked transforms reject a
    // pending removal, so skip placement while the base chain still runs.
    if (message == MID_INITIALUPDATE && g_pServerDE->IsObjectAlive(m_hObject))
    {
        DVector dims;
        if (g_pServerDE->GetModelAnimUserDims(m_hObject, &dims, g_pServerDE->GetModelAnimation(m_hObject)) != DE_OK)
        {
            error = "No usable model dimensions; touch/gravity disabled";
            g_pServerDE->SetObjectFlags(m_hObject, FLAG_VISIBLE);
        }
        else
        {
            dims.x = std::max(dims.x, 10.0f);
            dims.z = std::max(dims.z, dims.x);
            g_pServerDE->SetObjectDims(m_hObject, &dims);
            if (reason == INITIALUPDATE_WORLDFILE && g_pServerDE->MoveObjectToGround(m_hObject) != DE_OK)
                error = "Initial pickup ground placement failed";
        }
        g_pServerDE->SetObjectUserFlags(m_hObject, g_pServerDE->GetObjectUserFlags(m_hObject) | (1u << 3) | (1u << 10) |
                                                       (rotate_fx ? (1u << 4) : 0));
        g_pServerDE->SetNextUpdate(m_hObject, 0);
        // String creation follows SpawnItem, with no ground snap; the helper applies
        // SpawnObjectVel after initial update. Bouncing pickups rise 10 units first.
        if (reason == INITIALUPDATE_STRINGPROP && bounce)
            SpawnBounce();
    }
    else if (message == MID_TOUCHNOTIFY && !touching_ && !picked_up &&
             (g_pServerDE->GetObjectFlags(m_hObject) & FLAG_TOUCH_NOTIFY) &&
             g_pServerDE->IsObjectAlive(static_cast<HOBJECT>(data)))
    {
        touching_ = static_cast<HOBJECT>(data);
        ++attempts;
        auto packet = g_pServerDE->StartMessageToObject(this, touching_,
                                                        value.kind == PickupKind::Ammo     ? add_ammo_message
                                                        : value.kind == PickupKind::Weapon ? add_weapon_message
                                                        : value.kind == PickupKind::Item   ? add_item_message
                                                                                           : add_powerup_message);
        if (packet)
        {
            g_pServerDE->WritePickupMessage(packet, value);
            g_pServerDE->EndMessage(packet);
        }
        touching_ = nullptr;
    }
    return B2BaseClass::EngineMessageFn(message, data, reason);
}
void PickupObject::SpawnBounce()
{
    // Original SpawnItem: InitialUpdate already scheduled no update (m_bBouncing was
    // false), so the bounce flag stays set but the Update spring is never reached.
    if (g_pServerDE->GetObjectPos(m_hObject, &rest_position) != DE_OK)
    {
        error = "String-spawn rest position unavailable";
        return;
    }
    bounce_acceleration = 500;
    bouncing = true;
    DVector raised{rest_position.x, rest_position.y + 10, rest_position.z};
    if (g_pServerDE->MoveObject(m_hObject, &raised) != DE_OK)
        error = "String-spawn bounce move failed";
}
DDWORD PickupObject::ObjectMessageFn(HOBJECT sender, DDWORD message, HMESSAGEREAD read)
{
    PickupMessage reply;
    if (message == picked_up_message && sender && sender == touching_ && !picked_up &&
        g_pServerDE->ReadPickupMessage(read, reply) && reply == value &&
        (g_pServerDE->IsKindOf(g_pServerDE->GetObjectClass(sender), g_pServerDE->GetClass("CPlayerObj")) ||
         (value.kind == PickupKind::Ammo && dynamic_cast<AI_Mgr*>(g_pServerDE->HandleToObject(sender)))))
    {
        picked_up = true;
        g_pServerDE->SetObjectFlags(m_hObject,
                                    g_pServerDE->GetObjectFlags(m_hObject) & ~FLAG_VISIBLE & ~FLAG_TOUCH_NOTIFY);
        ++sound_requests;
        auto report = PlayPickupSound(m_hObject, pickup_sound);
        report.value = value;
        g_pServerDE->RecordPickup(m_hObject, report);
        if (!pickup_target.empty() && !pickup_command.empty())
        {
            auto target = g_pServerDE->CreateString(pickup_target.c_str());
            auto command = g_pServerDE->CreateString(pickup_command.c_str());
            SendTriggerMsgToObjects(this, target, command);
            g_pServerDE->FreeString(target);
            g_pServerDE->FreeString(command);
        }
        g_pServerDE->RemoveObject(m_hObject); // SP: no respawn. Reclaim after callbacks.
    }
    return B2BaseClass::ObjectMessageFn(sender, message, read);
}
