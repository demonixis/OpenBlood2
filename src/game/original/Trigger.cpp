#include "Trigger.h"
#include "PlayerObj.h"
#include "SaveFields.h"
#include "engine/collision.h"
#include "game/game_settings.h"
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
        !std::isfinite(sound_radius) || sound_radius < 0 || sound_radius > 1e6f)
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
void Trigger::Sound(const std::string& name, HOBJECT owner, float radius, DBYTE priority, std::uint32_t action_string)
{
    last_sound = name;
    last_sound_owner = owner;
    last_sound_radius = radius;
    last_sound_priority = priority;
    last_voice = 0;
    sound_error.clear();
    if (name.empty())
        return;
    ++sound_requests;
    // Original Action mode (GAME_GAMETYPE, Single Player > CHOSEN) replaces the lock and unlock
    // sounds with FormatString(IDS_GENERAL_LOCKED/UNLOCKED) on g_pPlayerObj's console. Without a
    // local player nothing is shown or played; a missing string stays visible.
    if (action_string && GetGameType() == GAMETYPE_ACTION)
    {
        auto* player = CPlayerObj::LocalVoicePlayer();
        std::string text;
        if (!player)
            return;
        if (!g_pServerDE->GetClientResourceString(action_string, text))
            sound_error = "Action-mode lock feedback string " + std::to_string(action_string) + " is unavailable";
        else if (!player->SendConsoleMessage(text))
            sound_error = "Action-mode lock feedback could not reach the client";
        else
            ++console_feedback;
        return;
    }
    SoundRequest request;
    request.resource = name;
    request.speaker = owner;
    request.space = SoundSpace::Spatial;
    request.inner_radius = radius * .1f;
    request.outer_radius = radius;
    request.volume = 1;
    request.priority = priority;
    request.reverb_requested = true;
    // Original PlaySoundFromObject defaults: attached, no loop or retained handle.
    last_voice = g_pServerDE->PlayCharacterSound(owner, request, false, sound_error);
    if (last_voice)
        ++sound_starts;
    else
        ++sound_failures; // Missing feedback never cancels original trigger/lock progression.
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
                Sound(locked_sound, sender, 200, 2, 268); // MISC_HIGH on the denied player; IDS_GENERAL_LOCKED.
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
    Sound(activation_sound, m_hObject, sound_radius, 1); // MISC_MEDIUM, attached to this sensor.
    last_sender = sender;
    if (max_delay != 0)
    {
        delay_start = g_pServerDE->GetTime();
        g_pServerDE->SetNextUpdate(m_hObject, .01f);
        for (unsigned i = 0; i < slots.size() && g_pServerDE->IsObjectAlive(m_hObject); ++i)
            if (slots[i].delay != 0)
                slots[i].pending = true;
            else if (!slots[i].target.empty() && !slots[i].message.empty())
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
    {
        // Original: properties are read for world files and string creation only; a saved
        // trigger keeps the precreate the engine record supplies.
        const auto accepted = B2BaseClass::EngineMessageFn(message, data, reason);
        return accepted && (reason == PRECREATE_SAVEGAME || ReadProp());
    }
    if (message == MID_SAVEOBJECT && data)
        Save(*static_cast<HMESSAGEWRITE*>(data));
    else if (message == MID_LOADOBJECT && data)
        Load(*static_cast<HMESSAGEREAD*>(data));
    // Original InitialUpdate (dims, touch notify, marking, schedule) is skipped for a savegame.
    if (message == MID_INITIALUPDATE && reason != INITIALUPDATE_SAVEGAME)
    {
        g_pServerDE->SetObjectDims(m_hObject, &dims);
        g_pServerDE->SetObjectFlags(m_hObject, FLAG_TOUCH_NOTIFY);
        // Mark this object as savable.
        g_pServerDE->SetObjectUserFlags(m_hObject, g_pServerDE->GetObjectUserFlags(m_hObject) | USRFLG_SAVEABLE);
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
                Sound(unlocked_sound, sender, 200, 2, 269); // The command sender; IDS_GENERAL_UNLOCKED.
            locked = false;
            g_pServerDE->SetNextUpdate(m_hObject, 0);
        }
    }
    return B2BaseClass::ObjectMessageFn(sender, message, read);
}
DDWORD ToggleTrigger::EngineMessageFn(DDWORD message, void* data, DFLOAT reason)
{
    // Derived block first, then Trigger's (original dispatch order).
    if (message == MID_SAVEOBJECT && data)
        Save(*static_cast<HMESSAGEWRITE*>(data));
    else if (message == MID_LOADOBJECT && data)
        Load(*static_cast<HMESSAGEREAD*>(data));
    else if (message == MID_PRECREATE && reason != PRECREATE_SAVEGAME)
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
namespace
{
    namespace F = OpenBlood2::save_fields;
    bool read_strings(HMESSAGEREAD h, std::initializer_list<std::string*> out)
    {
        bool ok = true;
        for (auto* text : out)
            ok = F::read_string(h, *text) && ok;
        return ok;
    }
} // namespace
void Trigger::Save(HMESSAGEWRITE h)
{
    // Original Trigger.cpp:1046-1090. m_hLastSender and the reentrancy guard are not saved; the
    // owning door of a self-spawned trigger is port state that Door::Load restores.
    auto* s = g_pServerDE;
    const float now = s->GetTime();
    s->WriteToMessageByte(h, active);
    s->WriteToMessageVector(h, &dims);
    s->WriteToMessageFloat(h, reset_time);
    F::write_string(h, activation_sound);
    s->WriteToMessageFloat(h, sound_radius);
    for (const auto& slot : slots)
    {
        F::write_string(h, slot.target);
        F::write_string(h, slot.message);
        s->WriteToMessageFloat(h, slot.delay);
        s->WriteToMessageByte(h, slot.pending);
    }
    s->WriteToMessageFloat(h, max_delay);
    s->WriteToMessageFloat(h, delay_start - now);
    for (bool value :
         {touch_activate, player_activate, ai_activate, object_activate, relay_activate, named_activate, locked})
        s->WriteToMessageByte(h, value);
    s->WriteToMessageDWord(h, activation_count);
    s->WriteToMessageDWord(h, current_activation);
    F::write_string(h, locked_message);
    F::write_string(h, locked_sound);
    F::write_string(h, unlocked_message);
    F::write_string(h, unlocked_sound);
    F::write_string(h, key_name);
    F::write_string(h, activation_name);
}
void Trigger::Load(HMESSAGEREAD h)
{
    // Original Trigger.cpp:1099-1141, decoded and checked before any member changes. Pending
    // delayed sends resume from the relative delay start and the restored schedule.
    auto* s = g_pServerDE;
    bool saved_active = false;
    bool ok = F::read_bool(h, saved_active);
    DVector saved_dims{};
    s->ReadFromMessageVector(h, &saved_dims);
    const float reset = s->ReadFromMessageFloat(h);
    std::string sound;
    ok = F::read_string(h, sound) && ok;
    const float radius = s->ReadFromMessageFloat(h);
    std::array<Slot, 10> saved;
    for (unsigned i = 0; i < saved.size(); ++i)
    {
        auto& slot = saved[i];
        slot.off_message = slots[i].off_message; // ToggleTrigger's block, read first.
        ok = read_strings(h, {&slot.target, &slot.message}) && ok;
        slot.delay = s->ReadFromMessageFloat(h);
        ok = F::read_bool(h, slot.pending) && ok;
    }
    const float max = s->ReadFromMessageFloat(h);
    const float start = s->ReadFromMessageFloat(h);
    bool flags[7]{};
    for (bool& value : flags)
        ok = F::read_bool(h, value) && ok;
    const DDWORD count = s->ReadFromMessageDWord(h), current = s->ReadFromMessageDWord(h);
    std::string text[6];
    ok = read_strings(h, {&text[0], &text[1], &text[2], &text[3], &text[4], &text[5]}) && ok;
    if (!ok)
        return;
    ok = OpenBlood2::physics_vector(saved_dims) && saved_dims.x >= 0 && saved_dims.y >= 0 && saved_dims.z >= 0 &&
         std::isfinite(reset) && F::finite(radius) && radius >= 0 && F::finite(max) && F::finite(start, 1e9f);
    for (const auto& slot : saved)
        ok = ok && F::finite(slot.delay);
    if (!ok)
    {
        s->RejectPayload(h, "trigger block holds values the port cannot represent");
        return;
    }
    active = saved_active;
    dims = saved_dims;
    reset_time = reset;
    activation_sound = std::move(sound);
    sound_radius = radius;
    slots = std::move(saved);
    max_delay = max;
    delay_start = F::restored_time(start, s->GetTime());
    touch_activate = flags[0];
    player_activate = flags[1];
    ai_activate = flags[2];
    object_activate = flags[3];
    relay_activate = flags[4];
    named_activate = flags[5];
    locked = flags[6];
    activation_count = count;
    current_activation = current;
    locked_message = std::move(text[0]);
    locked_sound = std::move(text[1]);
    unlocked_message = std::move(text[2]);
    unlocked_sound = std::move(text[3]);
    key_name = std::move(text[4]);
    activation_name = std::move(text[5]);
}
void ToggleTrigger::Save(HMESSAGEWRITE h)
{
    // Original Trigger.cpp:1305-1316.
    for (const auto& slot : slots)
        F::write_string(h, slot.off_message);
    g_pServerDE->WriteToMessageByte(h, on);
}
void ToggleTrigger::Load(HMESSAGEREAD h)
{
    // Original Trigger.cpp:1327-1337.
    std::array<std::string, 10> off;
    bool ok = true;
    for (auto& text : off)
        ok = F::read_string(h, text) && ok;
    bool saved_on = false;
    if (!F::read_bool(h, saved_on) || !ok)
        return;
    for (unsigned i = 0; i < slots.size(); ++i)
        slots[i].off_message = std::move(off[i]);
    on = saved_on;
}
