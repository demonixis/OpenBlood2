// Selected original ObjectDLL/RandomSoundFX.cpp. Save/load and precache remain unavailable.
#include "RandomSoundFX.h"
#include <algorithm>
#include <cmath>
using namespace OpenBlood2;
RandomSoundFX::~RandomSoundFX()
{
    if (g_pServerDE && sound)
        g_pServerDE->KillSound(sound);
}
bool RandomSoundFX::ReadProp()
{
    auto boolean = [&](const char* name, bool& out)
    {
        GenericProp p;
        if (g_pServerDE->GetPropGeneric(name, &p) == DE_OK)
            out = p.m_Bool != 0;
    };
    boolean("InitiallyOn", on);
    boolean("PositionalSound", positional);
    boolean("SequentialPlay", sequential);
    boolean("LoopSequence", loop_sequence);
    g_pServerDE->GetPropReal("PosRangeRadius", &position_radius);
    g_pServerDE->GetPropReal("SoundRadius", &sound_radius);
    g_pServerDE->GetPropReal("MinWaitTime", &min_wait);
    g_pServerDE->GetPropReal("MaxWaitTime", &max_wait);
    GenericProp prop;
    if (g_pServerDE->GetPropGeneric("Volume", &prop) == DE_OK)
        volume = std::min<DBYTE>(static_cast<DBYTE>(prop.m_Long), 100); // Source casts before clamping.
    for (unsigned i = 0; i < sounds.size(); ++i)
        if (g_pServerDE->GetPropGeneric(("SoundFile" + std::to_string(i + 1)).c_str(), &prop) == DE_OK)
        {
            if (!prop.m_String[0])
                break; // First gap terminates the playable list; later authored slots remain in World properties.
            sounds[i] = prop.m_String;
            ++count;
        }
    // Explicit bounded adaptation: reject malformed ranges rather than scheduling undefined time/positions.
    return std::isfinite(position_radius) && position_radius >= 0 && position_radius <= 1e6f &&
           std::isfinite(sound_radius) && sound_radius > 0 && sound_radius <= 1e6f && std::isfinite(min_wait) &&
           std::isfinite(max_wait) && min_wait >= 0 && min_wait <= max_wait && max_wait <= 1e6f;
}
void RandomSoundFX::ScheduleWait()
{
    float wait = 0;
    if (g_pServerDE->RandomFloat(min_wait, max_wait, wait))
        g_pServerDE->SetNextUpdate(m_hObject, wait); // Source zero disables, never an invented minimum delay.
}
DDWORD RandomSoundFX::EngineMessageFn(DDWORD message, void* data, DFLOAT reason)
{
    if (message == MID_PRECREATE)
    {
        if (!data || reason == 3)
            return 0;
        if ((reason == PRECREATE_WORLDFILE || reason == PRECREATE_STRINGPROP) && !ReadProp())
            return 0;
    }
    else if (message == MID_INITIALUPDATE)
    {
        // The original never marks RandomSoundFX saveable, so it vanished from a restore.
        // Decision-4 departure (save_load_contracts lot 4): it is saveable; until its payload
        // lands a save naming it is refused.
        g_pServerDE->SetObjectUserFlags(m_hObject, g_pServerDE->GetObjectUserFlags(m_hObject) | USRFLG_SAVEABLE);
        if (on)
            ScheduleWait();
    }
    else if (message == MID_UPDATE && !Update())
        g_pServerDE->RemoveObject(m_hObject);
    return B2BaseClass::EngineMessageFn(message, data, reason);
}
DDWORD RandomSoundFX::ObjectMessageFn(HOBJECT sender, DDWORD message, HMESSAGEREAD read)
{
    if (message == 1000)
    {
        ++commands;
        auto text = g_pServerDE->ReadFromMessageHString(read);
        const char* bytes = g_pServerDE->GetStringData(text);
        const std::string_view command = bytes ? bytes : "";
        if (command.starts_with("TOGGLE"))
            on = !on;
        else if (command.starts_with("ON"))
            on = true;
        else if (command.starts_with("OFF"))
            on = false;
        g_pServerDE->FreeString(text); // Original frees this handle twice; release our owned read once.
        if (on && sequential)
            current = 0;
        g_pServerDE->SetNextUpdate(m_hObject, on ? .1f : 0.0f);
        // OFF cancels scheduling, not the currently playing voice. Unknown payloads also rearm/reset if on.
    }
    return B2BaseClass::ObjectMessageFn(sender, message, read);
}
bool RandomSoundFX::Update()
{
    if (sound)
    {
        g_pServerDE->KillSound(sound);
        sound = 0;
    }
    if (!count)
        return false;
    int choice = int(current);
    if (!sequential && !g_pServerDE->RandomInt(0, int(count) - 1, choice))
        return false;
    last_choice = unsigned(choice);
    g_pServerDE->GetObjectPos(m_hObject, &last_position);
    float x = 0, z = 0;
    if (!g_pServerDE->RandomFloat(-position_radius, position_radius, x) ||
        !g_pServerDE->RandomFloat(-position_radius, position_radius, z))
        return false;
    last_position.x += x;
    last_position.z += z;
    SoundRequest request;
    request.resource = sounds[last_choice];
    request.space = positional ? SoundSpace::Spatial : SoundSpace::Ambient;
    request.inner_radius = sound_radius * .5f;
    request.outer_radius = sound_radius;
    request.volume = volume / 100.0f;
    request.reverb_requested = positional;
    ++attempts;
    sound = g_pServerDE->PlaySoundAt(m_hObject, request, last_position, error);
    if (!sound)
        ++failures;
    if (sequential && ++current == count)
    {
        current = 0;
        if (!loop_sequence)
            on = false;
    }
    if (on)
        ScheduleWait(); // Delays are independent of the previous clip's duration, including failed starts.
    return true;
}
