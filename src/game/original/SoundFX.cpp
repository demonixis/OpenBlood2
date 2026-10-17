// Original ObjectDLL/SoundFX.cpp rules; see docs/sound_fx_contracts.md.
#include "SoundFX.h"
#include "game/key_messages.h"
#include <algorithm>
using namespace OpenBlood2;
SoundFX::~SoundFX()
{
    if (g_pServerDE && sound)
        g_pServerDE->KillSound(sound);
    // Value-owned paths also remove the source destructor's duplicate RampDown FreeString.
}
const char* SoundFX::state_name() const
{
    switch (state)
    {
        case Off:
            return "off";
        case RampUp:
            return "ramp up";
        case On:
            return "on";
        case RampDown:
            return "ramp down";
    }
    return "invalid";
}
void SoundFX::ReadProp()
{
    GenericProp p;
    auto boolean = [&](const char* key, bool& value)
    {
        if (g_pServerDE->GetPropGeneric(key, &p) == DE_OK)
            value = p.m_Bool != 0;
    };
    auto path = [&](const char* key, std::string& value)
    {
        if (g_pServerDE->GetPropGeneric(key, &p) == DE_OK && p.m_String[0])
            value = p.m_String;
    };
    boolean("StartOn", start_on);
    boolean("Ambient", ambient);
    boolean("FileStream", file_stream);
    path("RampUpSound", ramp_up);
    path("RampDownSound", ramp_down);
    path("Sound", sound_path);
    g_pServerDE->GetPropReal("OuterRadius", &outer_radius);
    g_pServerDE->GetPropReal("InnerRadius", &inner_radius);
    if (g_pServerDE->GetPropGeneric("Priority", &p) == DE_OK)
        priority = static_cast<DBYTE>(p.m_Long);
    if (g_pServerDE->GetPropGeneric("Volume", &p) == DE_OK)
        volume = static_cast<DBYTE>(p.m_Long);
}
DDWORD SoundFX::EngineMessageFn(DDWORD message, void* data, DFLOAT reason)
{
    if (message == MID_PRECREATE)
    {
        if (!data || reason == 3)
            return 0; // Save/restore is unavailable.
        // Observed historical metadata is retained, not mapped to a guessed audio behavior.
        GenericProp legacy_voice;
        if (g_pServerDE->GetPropGeneric("VoiceType", &legacy_voice) == DE_OK && legacy_voice.m_Long != 0)
            return 0;
        const auto result = B2BaseClass::EngineMessageFn(message, data, reason);
        if (reason == PRECREATE_WORLDFILE || reason == PRECREATE_STRINGPROP)
            ReadProp();
        static_cast<ObjectCreateStruct*>(data)->m_NextUpdate = start_on ? .01f : 0;
        return result;
    }
    if (message == MID_INITIALUPDATE)
    {
        g_pServerDE->SetObjectUserFlags(m_hObject, g_pServerDE->GetObjectUserFlags(m_hObject) | (1u << 3));
        if (start_on)
        {
            state = On; // Startup bypasses RampUp.
            UpdateSound();
        }
        // Preserve the source cancellation even for a non-ambient initial one-shot.
        g_pServerDE->SetNextUpdate(m_hObject, 0);
    }
    else if (message == MID_UPDATE)
    {
        g_pServerDE->SetNextUpdate(m_hObject, 0);
        if (state == RampUp)
            state = On;
        else if (state == RampDown)
            state = Off;
        else if (state == On)
            state = RampDown;
        else
            return B2BaseClass::EngineMessageFn(message, data, reason);
        UpdateSound();
    }
    // Original filtering happens after sound startup; deferred teardown owns its cleanup.
    return B2BaseClass::EngineMessageFn(message, data, reason);
}
DDWORD SoundFX::ObjectMessageFn(HOBJECT sender, DDWORD message, HMESSAGEREAD read)
{
    if (message == 1000)
    {
        ++commands;
        const auto text = g_pServerDE->ReadFromMessageHString(read);
        const char* bytes = g_pServerDE->GetStringData(text);
        const std::string command = bytes ? bytes : "";
        g_pServerDE->FreeString(text);
        if (same_name(command, "TOGGLE"))
        {
            state = state == Off ? RampUp : RampDown;
            UpdateSound();
        }
        else if (same_name(command, "ON") || same_name(command, "TRIGGER"))
        {
            if (state == Off)
                state = RampUp;
            UpdateSound(); // Even an already-on command kills and restarts its current sound.
        }
        else if (same_name(command, "OFF"))
        {
            if (state != Off)
                state = RampDown;
            UpdateSound();
        }
    }
    return B2BaseClass::ObjectMessageFn(sender, message, read);
}
void SoundFX::UpdateSound()
{
    if (sound)
        g_pServerDE->KillSound(sound);
    sound = 0;
    const std::string* path = nullptr;
    switch (state)
    {
        case Off:
            return;
        case RampUp:
            if (!ramp_up.empty())
                path = &ramp_up;
            else
            {
                state = On;
                path = &sound_path;
            }
            break;
        case RampDown:
            if (!ramp_down.empty())
                path = &ramp_down;
            else
            {
                state = Off;
                return;
            }
            break;
        case On:
            path = &sound_path;
            break;
    }
    if (!path || path->empty())
        return;
    ++attempts;
    error.clear();
    SoundRequest request;
    request.resource = *path;
    request.inner_radius = inner_radius;
    request.outer_radius = outer_radius;
    request.priority = priority;
    request.volume = std::min<DBYTE>(volume, 100) / 100.0f;
    request.space = ambient ? SoundSpace::Ambient : SoundSpace::Spatial;
    request.loop = state == On && ambient;
    request.reverb_requested = !ambient;
    request.streaming = file_stream;
    if (g_pServerDE->GetObjectPos(m_hObject, &last_position) != DE_OK)
        error = "SoundFX source position unavailable";
    else
        sound = g_pServerDE->PlaySoundAt(m_hObject, request, last_position, error);
    if (!sound)
        ++failures;
    // Original precedence also queries duration for a failed non-ambient ON request.
    // UpdateSound does not cancel a previous deadline when no new one is obtained.
    if ((sound && (state == RampUp || state == RampDown)) || (state == On && !ambient))
    {
        float duration = 0;
        if (g_pServerDE->GetSoundDuration(sound, duration))
            g_pServerDE->SetNextUpdate(m_hObject, duration);
    }
}
