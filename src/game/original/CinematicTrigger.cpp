// Selected original ObjectDLL/CinematicTrigger.cpp; owned cues and checked audio handles.
#include "CinematicTrigger.h"
#include "engine/objects.h"
#include "trigger_helpers.h"
#include <cmath>
using namespace OpenBlood2;
CinematicTrigger::~CinematicTrigger()
{
    if (g_pServerDE && sound)
        g_pServerDE->KillSound(sound);
}
bool CinematicTrigger::ReadProp()
{
    for (std::size_t i = 0; i < cues.size(); ++i)
    {
        auto& cue = cues[i];
        const auto suffix = std::to_string(i + 1);
        g_pServerDE->GetPropReal(("Delay" + suffix).c_str(), &cue.delay);
        auto string = [&](const char* name, std::string& out)
        {
            GenericProp p;
            if (g_pServerDE->GetPropGeneric((name + suffix).c_str(), &p) == DE_OK)
                out = p.m_String;
        };
        string("Sound", cue.sound);
        string("WhoPlaysSound", cue.speaker);
        string("StartSoundTriggerTarget", cue.target);
        string("StartSoundTriggerMsg", cue.message);
        if (!std::isfinite(cue.delay) || cue.delay < 0 || cue.delay > 1e6f)
            return false;
    }
    return true;
}
DDWORD CinematicTrigger::EngineMessageFn(DDWORD message, void* data, DFLOAT reason)
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
        g_pServerDE->SetObjectUserFlags(m_hObject, g_pServerDE->GetObjectUserFlags(m_hObject) | (1u << 3));
        g_pServerDE->SetNextUpdate(m_hObject, 0);
    }
    else if (message == MID_UPDATE && !Update())
    {
        g_pServerDE->RemoveObject(m_hObject);
        return 0;
    }
    return B2BaseClass::EngineMessageFn(message, data, reason);
}
DDWORD CinematicTrigger::ObjectMessageFn(HOBJECT sender, DDWORD message, HMESSAGEREAD read)
{
    if (message == 1000)
    {
        ++commands;
        // Every trigger payload rearms delay zero; neither index nor sound resets.
        g_pServerDE->SetNextUpdate(m_hObject, cues[0].delay + .0001f);
    }
    return B2BaseClass::ObjectMessageFn(sender, message, read);
}
bool CinematicTrigger::Update()
{
    if (sound)
    {
        bool done = false;
        if (!g_pServerDE->IsSoundDone(sound, done) || done)
        {
            g_pServerDE->KillSound(sound);
            sound = 0;
            ++current;
            if (current < cues.size())
            {
                g_pServerDE->SetNextUpdate(m_hObject, cues[current].delay + .0001f);
                return true;
            }
            return false;
        }
    }
    else
    {
        if (current >= cues.size() || cues[current].sound.empty())
            return false;
        StartSound();
        if (!g_pServerDE->IsObjectAlive(m_hObject))
            return false;
    }
    g_pServerDE->SetNextUpdate(m_hObject, .001f);
    return true;
}
HOBJECT CinematicTrigger::PlayedBy(const std::string& name)
{
    if (name.empty())
        return m_hObject;
    auto* list = g_pServerDE->FindNamedObjects(name.c_str());
    HOBJECT found = m_hObject;
    if (list)
    {
        if (list->m_pFirstLink && g_pServerDE->IsObjectAlive(list->m_pFirstLink->m_hObject))
            found = list->m_pFirstLink->m_hObject;
        g_pServerDE->RelinquishList(list); // Fix source's early-return list leak.
    }
    return found;
}
void CinematicTrigger::StartSound()
{
    if (current >= cues.size())
        return;
    if (sound)
    {
        g_pServerDE->KillSound(sound);
        sound = 0;
    }
    const auto& cue = cues[current];
    if (!cue.target.empty() && !cue.message.empty())
    {
        ++messages;
        auto target = g_pServerDE->CreateString(cue.target.c_str()),
             message = g_pServerDE->CreateString(cue.message.c_str());
        if (target && message)
            SendTriggerMsgToObjects(this, target, message);
        else
            error = "Cinematic message allocation failed";
        g_pServerDE->FreeString(target);
        g_pServerDE->FreeString(message);
    }
    if (!g_pServerDE->IsObjectAlive(m_hObject))
        return;
    SoundRequest request;
    request.resource = cue.sound;
    request.space = same_name(cue.speaker, "player") ? SoundSpace::Local : SoundSpace::Spatial;
    request.speaker = request.space == SoundSpace::Local ? nullptr : PlayedBy(cue.speaker);
    last_speaker = request.speaker;
    sound = g_pServerDE->PlayCinematicSound(m_hObject, request, error);
    ++starts;
    if (!sound)
        ++failures; // Original retries on next update, resending its cue message.
}
