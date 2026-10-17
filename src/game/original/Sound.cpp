#include "Sound.h"
#include <algorithm>
using namespace OpenBlood2;
Sound::Sound()
{
    request.inner_radius = 10;
    request.outer_radius = 100;
    request.space = SoundSpace::Ambient;
    request.reverb_requested = false;
    request.loop = true;
}
DDWORD Sound::EngineMessageFn(DDWORD message, void* data, DFLOAT reason)
{
    if (message == MID_PRECREATE)
    {
        if (!data || reason == 3)
            return 0; // Save/restore is unavailable; runtime filename is safely empty.
        // Observed historical metadata is retained, not mapped to a guessed audio behavior.
        GenericProp legacy_voice;
        if (g_pServerDE->GetPropGeneric("VoiceType", &legacy_voice) == DE_OK && legacy_voice.m_Long != 0)
            return 0;
        static_cast<ObjectCreateStruct*>(data)->m_ObjectType = OT_NORMAL;
        if (reason != 0)
        {
            GenericProp prop;
            if (g_pServerDE->GetPropGeneric("Filename", &prop) == DE_OK)
                request.resource = prop.m_String;
            g_pServerDE->GetPropReal("OuterRadius", &request.outer_radius);
            g_pServerDE->GetPropReal("InnerRadius", &request.inner_radius);
            if (g_pServerDE->GetPropGeneric("Volume", &prop) == DE_OK)
                request.volume = std::min<DBYTE>(static_cast<DBYTE>(prop.m_Long), 100) / 100.0f;
            if (g_pServerDE->GetPropGeneric("Priority", &prop) == DE_OK)
                request.priority = static_cast<DBYTE>(prop.m_Long);
            if (g_pServerDE->GetPropGeneric("Ambient", &prop) == DE_OK)
                request.space = prop.m_Bool ? SoundSpace::Ambient : SoundSpace::Spatial;
            file_stream = true; // Original fallback; registered FileStream default is false.
            if (g_pServerDE->GetPropGeneric("FileStream", &prop) == DE_OK)
                file_stream = prop.m_Bool != 0;
        }
    }
    else if (message == MID_INITIALUPDATE)
    {
        sound = g_pServerDE->PlayWorldSound(m_hObject, request, file_stream, error);
        g_pServerDE->RemoveObject(m_hObject); // Original unconditional removal, even when playback fails.
    }
    return BaseClass::EngineMessageFn(message, data, reason);
}
