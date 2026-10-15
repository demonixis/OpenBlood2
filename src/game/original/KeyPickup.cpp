#include "KeyPickup.h"
#include "trigger_helpers.h"
#include <algorithm>
using namespace OpenBlood2;
DDWORD KeyPickup::EngineMessageFn(DDWORD message, void* data, DFLOAT reason)
{
    if (message == MID_PRECREATE)
    {
        if (!B2BaseClass::EngineMessageFn(message, data, reason) || reason == 3 || !data)
            return 0;
        auto& create = *static_cast<ObjectCreateStruct*>(data);
        auto string = [&](const char* key, std::string& out)
        {
            GenericProp prop;
            if (g_pServerDE->GetPropGeneric(key, &prop) == DE_OK)
                out = prop.m_String;
        };
        string("ObjectName", item.name);
        string("IconFile", item.icon);
        string("HiIconFile", item.highlighted_icon);
        string("PickupTriggerTarget", pickup_target);
        string("PickupTriggerMessage", pickup_command);
        std::string model, skin;
        string("ModelFile", model);
        string("ModelSkin", skin);
        SAFE_STRCPY(create.m_Filename, model.c_str());
        SAFE_STRCPY(create.m_SkinName, skin.c_str());
        GenericProp prop;
        if (g_pServerDE->GetPropGeneric("UseCount", &prop) == DE_OK)
            item.uses = static_cast<DBYTE>(std::clamp(prop.m_Long, 0, 255));
        if (g_pServerDE->GetPropGeneric("ResourceNum", &prop) == DE_OK)
            resource_id = prop.m_Long;
        item.display_name = item.name;
        if (resource_id)
            error = "Localized ResourceNum display name unavailable; ObjectName shown";
        create.m_ObjectType = OT_MODEL;
        create.m_Flags |= FLAG_TOUCH_NOTIFY | FLAG_VISIBLE | FLAG_GRAVITY;
        create.m_Flags &= ~FLAG_SHADOW;
        return 1;
    }
    if (message == MID_INITIALUPDATE)
    {
        DVector dims;
        if (g_pServerDE->GetModelAnimUserDims(m_hObject, &dims, g_pServerDE->GetModelAnimation(m_hObject)) != DE_OK)
        {
            error = "Key pickup has no usable animation dimensions; touch/gravity disabled";
            g_pServerDE->SetObjectFlags(m_hObject, FLAG_VISIBLE);
        }
        else
        {
            dims.x = std::max(dims.x, 10.0f);
            dims.z = std::max(dims.z, dims.x);
            g_pServerDE->SetObjectDims(m_hObject, &dims);
            if (reason == INITIALUPDATE_WORLDFILE && g_pServerDE->MoveObjectToGround(m_hObject) != DE_OK)
                error = "Initial key ground placement failed";
        }
        g_pServerDE->SetObjectUserFlags(m_hObject, (1u << 3) | (1u << 10));
        g_pServerDE->SetNextUpdate(m_hObject, 0);
    }
    else if (message == MID_TOUCHNOTIFY && !touching_ && !picked_up &&
             (g_pServerDE->GetObjectFlags(m_hObject) & FLAG_TOUCH_NOTIFY))
    {
        auto toucher = static_cast<HOBJECT>(data);
        if (g_pServerDE->IsObjectAlive(toucher) &&
            g_pServerDE->IsKindOf(g_pServerDE->GetObjectClass(toucher), g_pServerDE->GetClass("CPlayerObj")))
        {
            touching_ = toucher;
            ++attempts;
            auto packet = g_pServerDE->StartMessageToObject(this, toucher, key_pickup_message);
            if (packet)
            {
                g_pServerDE->WriteKeyMessage(packet, {item, false});
                g_pServerDE->EndMessage(packet);
            }
            touching_ = nullptr;
        }
    }
    return B2BaseClass::EngineMessageFn(message, data, reason);
}
DDWORD KeyPickup::ObjectMessageFn(HOBJECT sender, DDWORD message, HMESSAGEREAD read)
{
    KeyMessage reply;
    if (message == picked_up_message && sender && sender == touching_ && !picked_up &&
        g_pServerDE->ReadKeyMessage(read, reply) && reply.have && same_name(reply.item.name, item.name))
    {
        picked_up = true;
        g_pServerDE->SetObjectFlags(m_hObject,
                                    g_pServerDE->GetObjectFlags(m_hObject) & ~FLAG_VISIBLE & ~FLAG_TOUCH_NOTIFY);
        ++sound_requests; // Original powerup4.wav, not connected audio.
        if (!pickup_target.empty() && !pickup_command.empty())
        {
            auto target = g_pServerDE->CreateString(pickup_target.c_str());
            auto command = g_pServerDE->CreateString(pickup_command.c_str());
            SendTriggerMsgToObjects(this, target, command);
            g_pServerDE->FreeString(target);
            g_pServerDE->FreeString(command);
        }
        g_pServerDE->RemoveObject(m_hObject); // Single-player: never respawn.
    }
    return B2BaseClass::ObjectMessageFn(sender, message, read);
}
