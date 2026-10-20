// Selected ObjectDLL/KeyFramer.cpp: original sequencing and interpolation rules.
#include "KeyFramer.h"
#include "CameraObj.h"
#include "SaveFields.h"
#include "engine/collision.h"
#include "trigger_helpers.h"
#include <algorithm>
#include <cmath>
#include <tuple>
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
        if (!data)
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
    else if (message == MID_SAVEOBJECT && data)
        Save(*static_cast<HMESSAGEWRITE*>(data));
    else if (message == MID_LOADOBJECT && data)
        Load(*static_cast<HMESSAGEREAD*>(data));
    else if (message == MID_INITIALUPDATE && reason != INITIALUPDATE_SAVEGAME)
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
    start_active = active = cursor = true;
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
    if (rebuild_targets)
        RestoreTargets();
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
        // Every original KeyData::Copy sets POSITION_KEY: position 2 is simply the next key.
        position1 = static_cast<std::ptrdiff_t>(next);
        position2 = next + 1 < keys.size() ? position1 + 1 : -1;
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
    if (position1 < 0 || position2 < 0 || time < keys[position1].time)
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
namespace
{
    namespace F = OpenBlood2::save_fields;
    // Original KeyData.h key types; every copied key is a position key.
    constexpr DDWORD position_key = 1, sound_key = 2, message_key = 4, print_key = 8;
    DDWORD key_type(const KeyData& key)
    {
        return position_key | (key.sound.empty() ? 0 : sound_key) |
               (key.message_target.empty() || key.message.empty() ? 0 : message_key) |
               (key.print.empty() ? 0 : print_key);
    }
    void save_key(HMESSAGEWRITE h, const KeyData& key)
    {
        // Original KeyData.cpp:121-139. Copy keeps a target and message only as a pair.
        auto* s = g_pServerDE;
        const auto type = key_type(key);
        s->WriteToMessageRotation(h, &key.rotation);
        s->WriteToMessageVector(h, &key.position);
        s->WriteToMessageDWord(h, type);
        s->WriteToMessageFloat(h, key.timestamp);
        s->WriteToMessageFloat(h, key.time);
        s->WriteToMessageFloat(h, key.sound_radius);
        s->WriteToMessageByte(h, key.loop_sound);
        F::write_string(h, key.sound);
        F::write_string(h, type & message_key ? key.message_target : std::string{});
        F::write_string(h, type & message_key ? key.message : std::string{});
        F::write_string(h, key.print);
    }
    bool load_key(HMESSAGEREAD h, KeyData& key)
    {
        // Original KeyData.cpp:148-165, with KeyData::Copy's bounds and type rules.
        auto* s = g_pServerDE;
        s->ReadFromMessageRotation(h, &key.rotation);
        s->ReadFromMessageVector(h, &key.position);
        const DDWORD type = s->ReadFromMessageDWord(h);
        key.timestamp = s->ReadFromMessageFloat(h);
        key.time = s->ReadFromMessageFloat(h);
        key.sound_radius = s->ReadFromMessageFloat(h);
        bool ok = F::read_bool(h, key.loop_sound);
        for (auto* text : {&key.sound, &key.message_target, &key.message, &key.print})
            ok = F::read_string(h, *text) && ok;
        if (!ok)
            return false;
        const auto& q = key.rotation;
        const bool rotation = std::isfinite(q.m_Vec.x) && std::isfinite(q.m_Vec.y) && std::isfinite(q.m_Vec.z) &&
                              std::isfinite(q.m_Spin) &&
                              (q.m_Vec.x != 0 || q.m_Vec.y != 0 || q.m_Vec.z != 0 || q.m_Spin != 0);
        if (!rotation || !OpenBlood2::physics_vector(key.position) || !F::finite(key.timestamp) ||
            !F::finite(key.time) || !F::finite(key.sound_radius) || key.sound_radius < 0 ||
            key.message_target.empty() != key.message.empty() || type != key_type(key))
        {
            s->RejectPayload(h, "keyframer key holds values the port cannot represent");
            return false;
        }
        return true;
    }
    // Original pointer members are saved as float-encoded list indices, -1 for NULL.
    bool load_index(HMESSAGEREAD h, std::size_t count, std::ptrdiff_t& out)
    {
        const float value = g_pServerDE->ReadFromMessageFloat(h);
        if (!(value >= -1 && value < float(count) && value == std::floor(value)))
            return false;
        out = static_cast<std::ptrdiff_t>(value);
        return true;
    }
} // namespace
void KeyFramer::RestoreTargets()
{
    // Original Update rebuilds m_pObjectList by name (BuildObjectList) and links each object
    // again; the engine already restored the links of saved targets, and linking is idempotent.
    // Offsets are assigned in list order from the saved offsets; a target past them (one that
    // was not there at the save) gets a zero offset.
    if (object_name.empty())
    {
        rebuild_targets = false;
        saved_offsets.clear();
        return;
    }
    auto* list = g_pServerDE->FindNamedObjects(object_name.c_str());
    if (!list)
    {
        error = "Target snapshot unavailable";
        return;
    }
    std::vector<Target> found;
    bool valid = true;
    for (auto* link = list->m_pFirstLink; link && valid; link = link->m_pNext)
    {
        if (!g_pServerDE->IsObjectAlive(link->m_hObject))
            continue;
        valid = found.size() < 256 && link->m_hObject != m_hObject;
        Target t;
        t.object = link->m_hObject;
        if (found.size() < saved_offsets.size())
            std::tie(t.offset, t.rotation_offset) = saved_offsets[found.size()];
        else
            t.rotation_offset = {{0, 0, 0}, 0};
        found.push_back(t);
    }
    g_pServerDE->RelinquishList(list);
    rebuild_targets = false;
    saved_offsets.clear();
    if (!valid)
    {
        error = "Self-target or more than 256 controlled objects";
        return;
    }
    for (const auto& t : found)
        if (g_pServerDE->CreateInterObjectLink(m_hObject, t.object) != DE_OK)
        {
            error = "Cannot retain target lifetime link";
            return;
        }
    targets = std::move(found);
}
void KeyFramer::Save(HMESSAGEWRITE h)
{
    // Original KeyFramer.cpp:714-770. The key count is a byte; the original then desynchronized
    // past 255 keys, which the port refuses. Offsets are written for as many entries as there
    // are keys (the original indexes its per-target arrays by the key count), zero past the
    // last target. Decision-4 departure: a port tail carries the offsets of the targets past
    // the key count, which the original lost (SubwayA's twelve tunnel lights on a two-key path).
    // The retained key sound is never saved.
    auto* s = g_pServerDE;
    if (keys.size() > 255)
    {
        s->RejectPayload(h, "more than 255 keys do not fit the original byte count");
        return;
    }
    s->WriteToMessageFloat(h, time);
    F::write_string(h, object_name);
    F::write_string(h, base_key_name);
    s->WriteToMessageByte(h, start_active);
    s->WriteToMessageByte(h, looping);
    s->WriteToMessageByte(h, active);
    s->WriteToMessageByte(h, !initialized);
    s->WriteToMessageByte(h, static_cast<DBYTE>(keys.size()));
    for (const auto& key : keys)
        save_key(h, key);
    const bool current = cursor && next < keys.size();
    s->WriteToMessageFloat(h, current ? float(next) : -1.0f);
    s->WriteToMessageFloat(h, float(position1));
    s->WriteToMessageFloat(h, float(position2));
    const std::size_t count = rebuild_targets ? saved_offsets.size() : std::max(keys.size(), targets.size());
    for (std::size_t i = 0; i < count; ++i)
    {
        if (i == keys.size())
            s->WriteToMessageDWord(h, static_cast<DDWORD>(count - keys.size()));
        std::pair<DVector, DRotation> offset{{}, {{0, 0, 0}, 0}};
        if (rebuild_targets)
            offset = saved_offsets[i];
        else if (i < targets.size())
            offset = {targets[i].offset, targets[i].rotation_offset};
        s->WriteToMessageVector(h, &offset.first);
        s->WriteToMessageRotation(h, &offset.second);
    }
    if (count == keys.size())
        s->WriteToMessageDWord(h, 0);
}
void KeyFramer::Load(HMESSAGEREAD h)
{
    // Original KeyFramer.cpp:780-845, decoded and checked before any member changes. A restored
    // controller never collects keys again: the always-load Keys it consumed are back in the
    // world but inert (original behavior).
    auto* s = g_pServerDE;
    const float saved_time = s->ReadFromMessageFloat(h);
    std::string object, base;
    bool ok = F::read_string(h, object);
    ok = F::read_string(h, base) && ok;
    bool flags[4]{};
    for (bool& value : flags)
        ok = F::read_bool(h, value) && ok;
    const std::size_t count = s->ReadFromMessageByte(h);
    if (!ok)
        return;
    std::vector<KeyData> saved(count);
    for (std::size_t i = 0; i < count; ++i)
    {
        if (!load_key(h, saved[i]))
            return;
        saved[i].name = base + std::to_string(i); // The source Key's name, by construction.
    }
    std::ptrdiff_t current = -1, first = -1, second = -1;
    ok = load_index(h, count, current);
    ok = load_index(h, count, first) && ok;
    ok = load_index(h, count, second) && ok;
    std::vector<std::pair<DVector, DRotation>> offsets(count);
    const auto read_offsets = [&](std::size_t from)
    {
        for (std::size_t i = from; i < offsets.size(); ++i)
        {
            auto& [offset, rotation] = offsets[i];
            s->ReadFromMessageVector(h, &offset);
            s->ReadFromMessageRotation(h, &rotation);
            ok = ok && OpenBlood2::physics_vector(offset) && std::isfinite(rotation.m_Vec.x) &&
                 std::isfinite(rotation.m_Vec.y) && std::isfinite(rotation.m_Vec.z) && std::isfinite(rotation.m_Spin);
        }
    };
    read_offsets(0);
    // The port tail: offsets of the targets past the key count (at most the 256-target bound).
    const DDWORD extra = s->ReadFromMessageDWord(h);
    ok = ok && extra <= 256;
    if (ok)
    {
        offsets.resize(count + extra);
        read_offsets(count);
    }
    const bool started = flags[3] == false, running = flags[2];
    // The states the port's sequencing can reach: position 2 follows position 1 whenever a
    // next key exists, a running path has a current key, and nothing is collected before the
    // first update.
    ok = ok && F::finite(saved_time) &&
         second == (first >= 0 && std::size_t(first) + 1 < count ? first + 1 : -1) && (!running || current >= 0) &&
         (started || (count == 0 && extra == 0 && current < 0 && first < 0 && !running));
    if (!ok)
    {
        s->RejectPayload(h, "keyframer block holds values the port cannot represent");
        return;
    }
    time = saved_time;
    object_name = std::move(object);
    base_key_name = std::move(base);
    start_active = flags[0];
    looping = flags[1];
    active = running;
    initialized = started;
    keys = std::move(saved);
    cursor = current >= 0;
    next = cursor ? std::size_t(current) : 0;
    position1 = first;
    position2 = second;
    targets.clear();
    rebuild_targets = started;
    saved_offsets = started ? std::move(offsets) : decltype(offsets){};
}
