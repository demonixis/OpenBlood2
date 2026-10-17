// Selected ObjectDLL/KeyFramer.cpp: original sequencing and interpolation rules.
#include "KeyFramer.h"
#include "CameraObj.h"
#include "engine/collision.h"
#include "trigger_helpers.h"
#include <algorithm>
#include <cmath>
using namespace OpenBlood2;
KeyFramer::~KeyFramer()
{
    if (sound_voice && g_pServerDE)
        g_pServerDE->KillSound(sound_voice);
}
DDWORD KeyFramer::EngineMessageFn(DDWORD message, void* data, DFLOAT reason)
{
    if (message == MID_PRECREATE)
    {
        if (!data || reason == 3)
            return 0;
        if (reason == 1)
        {
            GenericProp p;
            if (g_pServerDE->GetPropGeneric("ObjectName", &p) == DE_OK)
                object_name = p.m_String;
            if (g_pServerDE->GetPropGeneric("BaseKeyName", &p) == DE_OK)
                base_key_name = p.m_String;
            if (g_pServerDE->GetPropGeneric("StartActive", &p) == DE_OK)
                start_active = p.m_Bool;
            if (g_pServerDE->GetPropGeneric("Looping", &p) == DE_OK)
                looping = p.m_Bool;
            static_cast<ObjectCreateStruct*>(data)->m_NextUpdate = .001f;
        }
    }
    else if (message == MID_INITIALUPDATE)
    {
        g_pServerDE->SetObjectUserFlags(m_hObject, g_pServerDE->GetObjectUserFlags(m_hObject) | (1u << 3));
        g_pServerDE->SetNextUpdate(m_hObject, .001f);
    }
    else if (message == MID_UPDATE)
        Update();
    else if (message == MID_LINKBROKEN)
        std::erase_if(targets, [data](const auto& t) { return t.object == static_cast<HOBJECT>(data); });
    return B2BaseClass::EngineMessageFn(message, data, reason);
}
bool KeyFramer::BuildKeyList()
{
    initialized = true;
    if (object_name.empty() || base_key_name.empty())
    {
        error = "Missing ObjectName or BaseKeyName";
        return false;
    }
    std::vector<HOBJECT> consumed;
    std::vector<KeyData> copied;
    // Discover before committing removals: malformed/over-limit paths consume no keys.
    for (std::size_t i = 0; i <= 1024; ++i)
    {
        const auto name = base_key_name + std::to_string(i);
        auto* list = g_pServerDE->FindNamedObjects(name.c_str());
        if (!list)
        {
            error = "Key snapshot unavailable";
            return false;
        }
        Key* key = nullptr;
        if (list)
            for (auto* link = list->m_pFirstLink; link; link = link->m_pNext)
                if (g_pServerDE->IsObjectAlive(link->m_hObject))
                    if ((key = dynamic_cast<Key*>(g_pServerDE->HandleToObject(link->m_hObject))))
                        break;
        if (list)
            g_pServerDE->RelinquishList(list);
        if (!key)
            break; // Original contiguous decimal suffix search, first typed match.
        KeyData value;
        if (i == 1024 || !value.Copy(*key))
        {
            error = "Invalid key data or more than 1024 contiguous keys";
            return false;
        }
        value.time = value.timestamp + (copied.empty() ? 0 : copied.back().time);
        if (!std::isfinite(value.time) || std::abs(value.time) > 1e6f)
        {
            error = "Key timeline exceeds finite one-million-second bound";
            return false;
        }
        consumed.push_back(key->m_hObject);
        copied.push_back(std::move(value));
    }
    auto* list = g_pServerDE->FindNamedObjects(object_name.c_str());
    if (!list)
    {
        error = "Target snapshot unavailable";
        return false;
    }
    std::vector<Target> found;
    for (auto* link = list->m_pFirstLink; link; link = link->m_pNext)
    {
        if (!g_pServerDE->IsObjectAlive(link->m_hObject))
            continue;
        if (found.size() == 256 || link->m_hObject == m_hObject)
        {
            g_pServerDE->RelinquishList(list);
            error = "Self-target or more than 256 controlled objects";
            return false;
        }
        Target t;
        t.object = link->m_hObject;
        if (!copied.empty())
        {
            g_pServerDE->GetObjectPos(t.object, &t.offset);
            t.offset = vector_subtract(t.offset, copied.front().position);
            g_pServerDE->GetObjectRotation(t.object, &t.rotation_offset);
            t.rotation_offset.m_Vec = vector_subtract(t.rotation_offset.m_Vec, copied.front().rotation.m_Vec);
            t.rotation_offset.m_Spin -= copied.front().rotation.m_Spin; // Source component offset, not composition.
        }
        found.push_back(t);
    }
    g_pServerDE->RelinquishList(list);
    for (const auto& t : found)
        if (g_pServerDE->CreateInterObjectLink(m_hObject, t.object) != DE_OK)
        {
            for (const auto& linked : found)
                g_pServerDE->BreakInterObjectLink(m_hObject, linked.object);
            error = "Cannot retain target lifetime link";
            return false;
        }
    targets = std::move(found);
    keys = std::move(copied);
    for (auto h : consumed)
        g_pServerDE->RemoveObject(h);
    if (keys.empty())
        error = "No contiguous keys found";
    else if (targets.empty())
        error = "No loaded targets; authored target class/name may be missing";
    return true;
}
void KeyFramer::GoActive()
{
    if (active || keys.empty() || !g_pServerDE->IsObjectAlive(m_hObject))
        return;
    start_active = active = true;
    time = 0;
    next = 0;
    ++revision;
    g_pServerDE->SetNextUpdate(m_hObject, .001f);
}
DDWORD KeyFramer::ObjectMessageFn(HOBJECT sender, DDWORD message, HMESSAGEREAD read)
{
    if (message == 1000)
    {
        auto h = g_pServerDE->ReadFromMessageHString(read);
        auto* value = g_pServerDE->GetStringData(h);
        last_command = value ? value : "";
        g_pServerDE->FreeString(h);
        if (same_name(last_command, "ON") || same_name(last_command, "TRIGGER"))
            GoActive();
        else if (same_name(last_command, "OFF"))
        {
            active = false;
            ++revision;
            g_pServerDE->SetNextUpdate(m_hObject, 0);
        }
    }
    return B2BaseClass::ObjectMessageFn(sender, message, read);
}
void KeyFramer::ProcessKey(const KeyData& key)
{
    ++processed;
    if (!key.sound.empty())
    {
        last_sound = key.sound;
        for (const auto& t : targets)
            if (g_pServerDE->IsObjectAlive(t.object))
            {
                // The source retains ONE handle for the entire target list. Each
                // target replaces the preceding target's sound, even on failure.
                if (sound_voice)
                    g_pServerDE->KillSound(sound_voice);
                sound_voice = 0;
                ++sound_requests;
                sound_target = t.object;
                sound_radius = key.sound_radius;
                sound_loop = key.loop_sound;
                sound_error.clear();
                SoundRequest request;
                request.resource = key.sound;
                request.speaker = t.object;
                request.space = SoundSpace::Spatial;
                request.inner_radius = key.sound_radius * .1f;
                request.outer_radius = key.sound_radius;
                request.loop = key.loop_sound;
                request.priority = 1; // SOUNDPRIORITY_MISC_MEDIUM
                request.reverb_requested = true;
                sound_voice = key.loop_sound ? g_pServerDE->PlayObjectLoopSound(t.object, request, sound_error)
                                             : g_pServerDE->PlayCharacterSound(t.object, request, true, sound_error);
                if (sound_voice)
                    ++sound_starts;
                else
                    ++sound_failures; // Original messages/motion still follow a failed sound start.
            }
    }
    if (!key.message_target.empty() && !key.message.empty())
    {
        ++message_keys;
        last_message_matches = 0;
        auto* list = g_pServerDE->FindNamedObjects(key.message_target.c_str());
        if (list)
        {
            for (auto* link = list->m_pFirstLink; link; link = link->m_pNext)
                if (g_pServerDE->IsObjectAlive(link->m_hObject))
                    ++last_message_matches;
            g_pServerDE->RelinquishList(list);
        }
        if (!last_message_matches)
            ++unresolved_messages;
        auto target = g_pServerDE->CreateString(key.message_target.c_str());
        auto message = g_pServerDE->CreateString(key.message.c_str());
        if (target && message)
            SendTriggerMsgToObjects(this, target, message);
        else
            error = "Key message string allocation failed";
        g_pServerDE->FreeString(target);
        g_pServerDE->FreeString(message);
    }
    if (!key.print.empty())
    {
        ++print_requests;
        last_print = key.print;
    }
}
void KeyFramer::Update()
{
    if (!initialized)
    {
        if (BuildKeyList() && start_active)
            GoActive();
        // Original first update builds keys and returns without advancing time.
        return;
    }
    if (!active)
    {
        g_pServerDE->SetNextUpdate(m_hObject, 0);
        return;
    }
    g_pServerDE->SetNextUpdate(m_hObject, .001f);
    time += g_pServerDE->GetFrameTime();
    const auto command_revision = revision;
    while (next < keys.size() && keys[next].time <= time)
    {
        position1 = next;
        position2 = next + 1;
        have_positions = position2 < keys.size(); // Every original KeyData::Copy sets POSITION_KEY.
        ProcessKey(keys[next]);
        if (!g_pServerDE->IsObjectAlive(m_hObject) || revision != command_revision)
            return;
        ++next;
    }
    if (next == keys.size())
    {
        next = 0;
        time = 0; // Source discards overshoot and processes at most one cycle per update.
        if (!looping)
        {
            active = false;
            return;
        } // Source does not snap to final endpoint.
    }
    if (!have_positions || time < keys[position1].time)
        return;
    const auto& a = keys[position1];
    const auto& b = keys[position2];
    const auto duration = b.time - a.time;
    DVector pos = b.position;
    DRotation rot = b.rotation;
    if (duration > .001f)
    {
        if (time > b.time)
            return;
        const auto t = (time - a.time) / duration;
        pos = vector_add(a.position, vector_scale(vector_subtract(b.position, a.position), t));
        if (g_pServerDE->InterpolateRotation(rot, a.rotation, b.rotation, t) != DE_OK)
        {
            error = "Invalid key rotation interpolation";
            return;
        }
    }
    for (auto& target : targets)
    {
        if (!g_pServerDE->IsObjectAlive(target.object))
            continue;
        auto destination = vector_add(pos, target.offset);
        DRotation rotation{vector_add(rot.m_Vec, target.rotation_offset.m_Vec),
                           rot.m_Spin + target.rotation_offset.m_Spin};
        auto* camera = dynamic_cast<CameraObj*>(g_pServerDE->HandleToObject(target.object));
        ++target.attempts;
        target.result = g_pServerDE->MoveKeyframedObject(target.object, destination,
                                                         camera && camera->GetLinkObject() ? nullptr : &rotation);
        if (target.result == MoverResult::Moved)
            ++target.moves;
        if (!g_pServerDE->IsObjectAlive(m_hObject) || revision != command_revision)
            return;
    }
}
