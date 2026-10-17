// Selected original ObjectDLL/ClientParticleStreamSFX.cpp. See particle_stream_contracts.md.
#include "ClientParticleStreamSFX.h"
using namespace OpenBlood2;
bool CClientParticleStreamSFX::ReadProp()
{
    GenericProp p;
    auto real = [&](const char* name, float& value)
    {
        if (g_pServerDE->GetPropGeneric(name, &p) == DE_OK)
            value = p.m_Float;
    };
    real("SystemRadius", settings.radius);
    real("PositionRadius", settings.position_radius);
    real("MinVelocity", settings.min_velocity);
    real("MaxVelocity", settings.max_velocity);
    real("VelSpread", settings.spread);
    real("SystemAlpha", settings.alpha);
    real("MinLifetime", settings.min_life);
    real("MaxLifetime", settings.max_life);
    real("RampTime", settings.ramp_time);
    real("AddDelay", settings.delay);
    real("Gravity", settings.gravity);
    real("SoundRadius", settings.sound_radius);
    if (g_pServerDE->GetPropGeneric("NumParticles", &p) == DE_OK)
        settings.count = unsigned(p.m_Long);
    if (g_pServerDE->GetPropGeneric("MinColor", &p) == DE_OK)
        settings.min_color = p.m_Color;
    if (g_pServerDE->GetPropGeneric("MaxColor", &p) == DE_OK)
        settings.max_color = p.m_Color;
    if (g_pServerDE->GetPropGeneric("InitiallyOn", &p) == DE_OK)
        settings.on = p.m_Bool;
    if (g_pServerDE->GetPropGeneric("ParticleFile", &p) == DE_OK)
        settings.texture = p.m_String;
    const char* flags[]{"RampAmount", "RampOffset", "RampVelocity", "RampLifetime"};
    const char* sounds[]{"RampUpSound", "LoopSound", "RampDownSound"};
    settings.ramp_flags = 0;
    for (unsigned i = 0; i < 4; ++i)
        if (g_pServerDE->GetPropGeneric(flags[i], &p) == DE_OK && p.m_Bool)
            settings.ramp_flags |= 1u << i;
    for (unsigned i = 0; i < 3; ++i)
        if (g_pServerDE->GetPropGeneric(sounds[i], &p) == DE_OK)
            settings.sounds[i] = p.m_String;
    return valid_particle_stream(settings);
}
DDWORD CClientParticleStreamSFX::EngineMessageFn(DDWORD message, void* data, DFLOAT reason)
{
    if (!g_pServerDE)
        return 0;
    if (message == MID_PRECREATE)
    {
        // Source only initialized its fields for world-file creation. Reject other reasons explicitly.
        if (!data || reason != PRECREATE_WORLDFILE || !ReadProp())
            return 0;
    }
    else if (message == MID_INITIALUPDATE)
    {
        g_pServerDE->SetNextUpdate(m_hObject, 0);
        g_pServerDE->SetObjectFlags(m_hObject,
                                    (g_pServerDE->GetObjectFlags(m_hObject) & ~FLAG_GRAVITY) | FLAG_FORCECLIENTUPDATE);
        auto flags = g_pServerDE->GetObjectUserFlags(m_hObject) | 8u;
        flags = settings.on ? flags | 1u : flags & ~1u;
        g_pServerDE->SetObjectUserFlags(m_hObject, flags);
        effect = g_pServerDE->PublishParticleStream(m_hObject, settings, error);
        if (!effect)
            g_pServerDE->RemoveObject(m_hObject);
    }
    return CClientSFX::EngineMessageFn(message, data, reason);
}
DDWORD CClientParticleStreamSFX::ObjectMessageFn(HOBJECT sender, DDWORD message, HMESSAGEREAD read)
{
    if (message == 1000)
    {
        const auto text = g_pServerDE->ReadFromMessageHString(read);
        const auto* value = g_pServerDE->GetStringData(text);
        last_command = value ? value : "";
        g_pServerDE->FreeString(text);
        ++commands;
        if (last_command.starts_with("TOGGLE"))
            settings.on = !settings.on;
        else if (last_command.starts_with("ON"))
            settings.on = true;
        else if (last_command.starts_with("OFF"))
            settings.on = false;
        auto flags = g_pServerDE->GetObjectUserFlags(m_hObject);
        g_pServerDE->SetObjectUserFlags(m_hObject, settings.on ? flags | 1u : flags & ~1u);
    }
    return CClientSFX::ObjectMessageFn(sender, message, read);
}
