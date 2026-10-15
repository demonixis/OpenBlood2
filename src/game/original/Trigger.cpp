#include "Trigger.h"
#include "engine/collision.h"
#include "trigger_helpers.h"
#include <algorithm>
#include <cmath>
using namespace OpenBlood2;
namespace
{
    void string_prop(const char* name, std::string& out)
    {
        GenericProp prop;
        if (g_pServerDE->GetPropGeneric(name, &prop) == DE_OK)
            out = prop.m_String;
    }
    void bool_prop(const char* name, bool& out)
    {
        DBOOL value = out;
        g_pServerDE->GetPropBool(name, &value);
        out = value != 0;
    }
} // namespace
bool Trigger::ReadProp()
{
    g_pServerDE->GetPropVector("Dims", &dims);
    g_pServerDE->GetPropReal("ResetTime", &reset_time);
    g_pServerDE->GetPropReal("SoundRadius", &sound_radius);
    string_prop("ActivationSound", activation_sound);
    string_prop("ActivationObjectName", activation_name);
    string_prop("LockedSound", locked_sound);
    string_prop("UnlockedSound", unlocked_sound);
    string_prop("LockedMsg", locked_message);
    string_prop("UnlockedMsg", unlocked_message);
    string_prop("UnlockKeyName", key_name);
    bool_prop("TouchActivate", touch_activate);
    bool_prop("PlayerActivate", player_activate);
    bool_prop("AIActivate", ai_activate);
    bool_prop("ObjectActivate", object_activate);
    bool_prop("TriggerRelayActivate", relay_activate);
    bool_prop("NamedObjectActivate", named_activate);
    bool_prop("Locked", locked);
    GenericProp prop;
    if (g_pServerDE->GetPropGeneric("ActivationCount", &prop) == DE_OK)
        activation_count = static_cast<std::uint32_t>(prop.m_Long);
    if (!physics_vector(dims) || dims.x < 0 || dims.y < 0 || dims.z < 0 || !std::isfinite(reset_time) ||
        std::abs(reset_time) > 1e6f || !std::isfinite(sound_radius) || sound_radius < 0 || sound_radius > 1e6f)
        return false;
    for (unsigned i = 0; i < slots.size(); ++i)
    {
        auto suffix = std::to_string(i + 1);
        auto& slot = slots[i];
        string_prop(("TargetName" + suffix).c_str(), slot.target);
        string_prop(("MessageName" + suffix).c_str(), slot.message);
        g_pServerDE->GetPropReal((i ? "MessageDelay" + suffix : "MessageDelay").c_str(), &slot.delay);
        if (!std::isfinite(slot.delay) || std::abs(slot.delay) > 1e6f)
            return false;
        max_delay = std::max(max_delay, slot.delay);
    }
    return true;
}
void Trigger::Sound(const std::string& name)
{
    last_sound = name;
    if (!name.empty())
        ++sound_requests; // Audio/Action-mode console localization is not connected.
}
bool Trigger::ValidateSender(HOBJECT sender)
{
    if (!g_pServerDE->IsObjectAlive(sender))
        return false;
    auto type = g_pServerDE->GetObjectClass(sender);
    bool accepted = false;
    if (relay_activate && g_pServerDE->IsKindOf(type, g_pServerDE->GetClass("Trigger")))
        accepted = true;
    else if (g_pServerDE->IsKindOf(type, g_pServerDE->GetClass("CPlayerObj")))
    {
        if (player_activate)
        {
            if (locked && !key_name.empty())
            {
                ++key_queries;
                querying_ = sender;
                KeyMessage query;
                query.item.name = key_name;
                auto message = g_pServerDE->StartMessageToObject(this, sender, key_query_message);
                if (message)
                {
                    g_pServerDE->WriteKeyMessage(message, query);
                    g_pServerDE->EndMessage(message);
                }
                querying_ = nullptr;
            }
            accepted = !locked;
            if (locked)
                Sound(locked_sound);
        }
    }
    else if (g_pServerDE->IsKindOf(type, g_pServerDE->GetClass("CBaseCharacter")))
        accepted = ai_activate;
    else
        accepted = object_activate;
    // Source order: querying consumes a use before the optional named filter.
    if (accepted && named_activate && !activation_name.empty())
    {
        const auto* name = g_pServerDE->GetObjectName(sender);
        accepted = name && same_name(name, activation_name);
    }
    return accepted;
}
void Trigger::Activate(HOBJECT sender, bool touch)
{
    if (!active || activating_ || (touch && !touch_activate) || (owner_door && !g_pServerDE->IsObjectAlive(owner_door)))
        return;
    // Also covers ToggleTrigger and delayed batches, whose source lacked the
    // SendMessages guard. Cycles cannot consume counters/keys recursively.
    activating_ = true;
    if (!ValidateSender(sender))
    {
        ++rejected;
        activating_ = false;
        return;
    }
    if (touch)
        last_sender = sender;
    if (++current_activation < activation_count)
    {
        activating_ = false;
        return;
    }
    current_activation = 0;
    ++activations;
    Sound(activation_sound);
    last_sender = sender;
    if (max_delay != 0)
    {
        delay_start = g_pServerDE->GetTime();
        g_pServerDE->SetNextUpdate(m_hObject, .01f);
        for (unsigned i = 0; i < slots.size() && g_pServerDE->IsObjectAlive(m_hObject); ++i)
            if (slots[i].delay != 0)
                slots[i].pending = true;
            else
                SendMessage(i, slots[i].message);
    }
    else
    {
        SendMessages();
        if (g_pServerDE->IsObjectAlive(m_hObject))
            g_pServerDE->SetNextUpdate(m_hObject, std::max(0.0f, reset_time));
    }
    active = false;
    activating_ = false;
}
void Trigger::SendMessage(unsigned index, const std::string& command)
{
    auto& slot = slots[index];
    if (slot.target.empty() || command.empty() || !g_pServerDE->IsObjectAlive(m_hObject))
        return;
    auto target = g_pServerDE->CreateString(slot.target.c_str());
    auto text = g_pServerDE->CreateString(command.c_str());
    SendTriggerMsgToObjects(this, target, text);
    g_pServerDE->FreeString(target);
    g_pServerDE->FreeString(text);
    ++slot.sent; // Attempted named dispatch, not proof that the target exists.
}
void Trigger::SendMessages()
{
    for (unsigned i = 0; i < slots.size(); ++i)
        SendMessage(i, slots[i].message);
}
void Trigger::Update()
{
    if (std::none_of(slots.begin(), slots.end(), [](const Slot& s) { return s.pending; }))
    {
        active = true;
        g_pServerDE->SetNextUpdate(m_hObject, 0);
        return;
    }
    activating_ = true;
    bool delayed = false;
    for (unsigned i = 0; i < slots.size() && g_pServerDE->IsObjectAlive(m_hObject); ++i)
        if (slots[i].pending)
        {
            if (g_pServerDE->GetTime() >= delay_start + slots[i].delay)
            {
                SendMessage(i, slots[i].message);
                slots[i].pending = false;
            }
            else
                delayed = true;
        }
    if (g_pServerDE->IsObjectAlive(m_hObject))
        g_pServerDE->SetNextUpdate(m_hObject, delayed ? .01f : std::max(0.0f, reset_time));
    activating_ = false;
}
DDWORD Trigger::EngineMessageFn(DDWORD message, void* data, DFLOAT reason)
{
    if (message == MID_PRECREATE)
        return B2BaseClass::EngineMessageFn(message, data, reason) && reason != 3 && ReadProp();
    if (message == MID_INITIALUPDATE)
    {
        g_pServerDE->SetObjectDims(m_hObject, &dims);
        g_pServerDE->SetObjectFlags(m_hObject, FLAG_TOUCH_NOTIFY);
        g_pServerDE->SetObjectUserFlags(m_hObject, 1u << 3);
        g_pServerDE->SetNextUpdate(m_hObject, 0);
    }
    else if (message == MID_TOUCHNOTIFY)
        Activate(static_cast<HOBJECT>(data), true);
    else if (message == MID_UPDATE)
        Update();
    return B2BaseClass::EngineMessageFn(message, data, reason);
}
DDWORD Trigger::ObjectMessageFn(HOBJECT sender, DDWORD message, HMESSAGEREAD read)
{
    if (message == key_response_message)
    {
        KeyMessage response;
        if (sender && sender == querying_ && g_pServerDE->ReadKeyMessage(read, response) && locked && response.have &&
            same_name(response.item.name, key_name))
            locked = false;
    }
    // The original early return gates RESET/LOCK/UNLOCK as well as TRIGGER.
    else if (message == 1000 && active && !activating_)
    {
        auto text = g_pServerDE->ReadFromMessageHString(read);
        const auto* raw = g_pServerDE->GetStringData(text);
        const std::string command = raw ? raw : "";
        g_pServerDE->FreeString(text);
        last_command = command;
        if (same_name(command, "TRIGGER"))
            Activate(sender, false);
        else if (same_name(command, "RESET"))
        {
            active = true;
            g_pServerDE->SetNextUpdate(m_hObject, 0);
        }
        else if (same_name(command, "LOCK"))
        {
            locked = true;
            g_pServerDE->SetNextUpdate(m_hObject, 0);
        }
        else if (same_name(command, "UNLOCK"))
        {
            if (locked)
                Sound(unlocked_sound);
            locked = false;
            g_pServerDE->SetNextUpdate(m_hObject, 0);
        }
    }
    return B2BaseClass::ObjectMessageFn(sender, message, read);
}
DDWORD ToggleTrigger::EngineMessageFn(DDWORD message, void* data, DFLOAT reason)
{
    if (message == MID_PRECREATE)
    {
        bool_prop("StartOn", on);
        for (unsigned i = 0; i < slots.size(); ++i)
            string_prop(("OffMessageName" + std::to_string(i + 1)).c_str(), slots[i].off_message);
    }
    return Trigger::EngineMessageFn(message, data, reason);
}
void ToggleTrigger::SendMessages()
{
    for (unsigned i = 0; i < slots.size(); ++i)
        SendMessage(i, on ? slots[i].off_message : slots[i].message);
    on = !on;
}
