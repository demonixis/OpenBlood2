// Selected original ObjectDLL/ClientParticleStreamSFX.cpp. See particle_stream_contracts.md.
#include "ClientParticleStreamSFX.h"
#include "SaveFields.h"
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
        // Source only initialized its fields for world-file creation. Reject other reasons
        // explicitly, except a restore, whose Load supplies every field.
        if (!data || (reason != PRECREATE_SAVEGAME && (reason != PRECREATE_WORLDFILE || !ReadProp())))
            return 0;
    }
    else if (message == MID_SAVEOBJECT && data)
        Save(*static_cast<HMESSAGEWRITE*>(data));
    else if (message == MID_LOADOBJECT && data)
        Load(*static_cast<HMESSAGEREAD*>(data));
    else if (message == MID_INITIALUPDATE && reason != INITIALUPDATE_SAVEGAME)
    {
        g_pServerDE->SetNextUpdate(m_hObject, 0);
        g_pServerDE->SetObjectFlags(m_hObject,
                                    (g_pServerDE->GetObjectFlags(m_hObject) & ~FLAG_GRAVITY) | FLAG_FORCECLIENTUPDATE);
        auto flags = g_pServerDE->GetObjectUserFlags(m_hObject) | 8u;
        flags = settings.on ? flags | 1u : flags & ~1u;
        g_pServerDE->SetObjectUserFlags(m_hObject, flags);
        published = settings;
        effect = g_pServerDE->PublishParticleStream(m_hObject, published, error);
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
namespace
{
    // The persistent SFX_PARTICLESTREAM message of the original SetupFX
    // (ClientParticleStreamSFX.cpp:49-77), in its field order without the effect ID byte. The
    // original engine record kept it; OpenBlood2's has none, so it is a tail of this block.
    void write_stream_message(HMESSAGEWRITE h, const ParticleStreamSettings& p)
    {
        auto* s = g_pServerDE;
        for (float value : {p.radius, p.position_radius, p.min_velocity, p.max_velocity})
            s->WriteToMessageFloat(h, value);
        s->WriteToMessageDWord(h, p.count);
        s->WriteToMessageFloat(h, p.spread);
        s->WriteToMessageVector(h, &p.min_color);
        s->WriteToMessageVector(h, &p.max_color);
        for (float value : {p.alpha, p.min_life, p.max_life, p.ramp_time, p.delay, p.gravity})
            s->WriteToMessageFloat(h, value);
        s->WriteToMessageByte(h, p.ramp_flags);
        s->WriteToMessageByte(h, p.on);
        save_fields::write_string(h, p.texture);
        s->WriteToMessageFloat(h, p.sound_radius);
        for (const auto& sound : p.sounds)
            save_fields::write_string(h, sound);
    }
    bool read_stream_message(HMESSAGEREAD h, ParticleStreamSettings& p)
    {
        auto* s = g_pServerDE;
        for (float* value : {&p.radius, &p.position_radius, &p.min_velocity, &p.max_velocity})
            *value = s->ReadFromMessageFloat(h);
        p.count = s->ReadFromMessageDWord(h);
        p.spread = s->ReadFromMessageFloat(h);
        s->ReadFromMessageVector(h, &p.min_color);
        s->ReadFromMessageVector(h, &p.max_color);
        for (float* value : {&p.alpha, &p.min_life, &p.max_life, &p.ramp_time, &p.delay, &p.gravity})
            *value = s->ReadFromMessageFloat(h);
        p.ramp_flags = s->ReadFromMessageByte(h);
        bool ok = save_fields::read_bool(h, p.on);
        ok = save_fields::read_string(h, p.texture, 1000) && ok;
        p.sound_radius = s->ReadFromMessageFloat(h);
        for (auto& sound : p.sounds)
            ok = save_fields::read_string(h, sound, 1000) && ok;
        return ok;
    }
} // namespace
void CClientParticleStreamSFX::Save(HMESSAGEWRITE h)
{
    // Original ClientParticleStreamSFX.cpp:261-271: only ON changes; the rest is the effect message.
    g_pServerDE->WriteToMessageByte(h, settings.on);
    write_stream_message(h, published);
}
void CClientParticleStreamSFX::Load(HMESSAGEREAD h)
{
    // Original ClientParticleStreamSFX.cpp:281-288, decoded and checked before any member
    // changes. The stream comes back from its saved effect message, as the original engine
    // replayed it: the client starts from the published ON and USRFLG_VISIBLE drives any ramp.
    auto* s = g_pServerDE;
    bool on = false;
    ParticleStreamSettings saved;
    bool ok = save_fields::read_bool(h, on);
    ok = read_stream_message(h, saved) && ok;
    if (!ok)
        return;
    if (!valid_particle_stream(saved))
    {
        s->RejectPayload(h, "particle stream message holds values the port cannot represent");
        return;
    }
    published = std::move(saved);
    settings = published;
    settings.on = on;
    effect = s->PublishParticleStream(m_hObject, published, error);
    if (!effect)
        s->RejectPayload(h, "the saved particle stream cannot be published again: " + error);
}
