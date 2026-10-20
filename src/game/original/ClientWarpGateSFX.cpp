// Original ObjectDLL/ClientWarpGateSFX.cpp, with owned settings and typed client publication.
#include "ClientWarpGateSFX.h"
#include "SaveFields.h"
using namespace OpenBlood2;
DDWORD CClientWarpGateSFX::EngineMessageFn(DDWORD message, void* data, DFLOAT reason)
{
    auto* s = GetServerDE();
    if (message == MID_PRECREATE)
    {
        if (!data)
            return 0;
        if (reason == PRECREATE_SAVEGAME)
            return CClientSFX::EngineMessageFn(message, data, reason); // Load supplies every field.
        if (reason == PRECREATE_WORLDFILE)
        {
            GenericProp prop;
            const auto real = [&](std::string key, float& value) { s->GetPropReal(key.c_str(), &value); };
            const auto integer = [&](std::string key, unsigned& value)
            {
                if (s->GetPropGeneric(key.c_str(), &prop) == DE_OK)
                    value = unsigned(prop.m_Long);
            };
            const auto boolean = [&](std::string key, bool& value)
            {
                if (s->GetPropGeneric(key.c_str(), &prop) == DE_OK)
                    value = prop.m_Bool != 0;
            };
            const auto text = [&](std::string key, std::string& value)
            {
                if (s->GetPropGeneric(key.c_str(), &prop) == DE_OK)
                    value = prop.m_String;
            };
            const auto vector = [&](std::string key, DVector& value)
            {
                if (s->GetPropGeneric(key.c_str(), &prop) == DE_OK)
                    value = prop.m_Vec;
            };
            real("RampUpTime", settings.ramp_up);
            real("RampDownTime", settings.ramp_down);
            boolean("InitiallyOn", on);
            auto& sprite = settings.sprite;
            real("SpriteMinScale", sprite.min_scale);
            real("SpriteMaxScale", sprite.max_scale);
            real("SpriteAlpha", sprite.alpha);
            integer("SpriteRampUpType", sprite.ramp_up);
            integer("SpriteRampDownType", sprite.ramp_down);
            boolean("SpriteAlign", sprite.align);
            text("SpriteFile", sprite.resource);
            for (unsigned i = 0; i < 2; ++i)
            {
                auto& p = settings.particles[i];
                const auto prefix = "PS" + std::to_string(i + 1);
                real(prefix + "SystemRadius", p.radius);
                real(prefix + "PositionRadius", p.position_radius);
                vector(prefix + "Offset", p.offset);
                vector(prefix + "Rotations", p.rotations);
                real(prefix + "MinVelocity", p.min_velocity);
                real(prefix + "MaxVelocity", p.max_velocity);
                integer(prefix + "NumParticles", p.count);
                integer(prefix + "EmitType", p.emit);
                vector(prefix + "MinColor", p.min_color);
                vector(prefix + "MaxColor", p.max_color);
                real(prefix + "SystemAlpha", p.alpha);
                real(prefix + "MinLifetime", p.min_life);
                real(prefix + "MaxLifetime", p.max_life);
                real(prefix + "AddDelay", p.delay);
                real(prefix + "Gravity", p.gravity);
                integer(prefix + "RampUpType", p.ramp_up);
                integer(prefix + "RampDownType", p.ramp_down);
                boolean(prefix + "Align", p.align);
                text(prefix + "ParticleFile", p.resource);
            }
            real("SoundRadius", sound_radius);
            text("RampUpSound", sounds[0]);
            text("LoopSound", sounds[1]);
            text("RampDownSound", sounds[2]);
        }
        if (!valid_warp_gate(settings, error))
        {
            s->RejectPrecreate(error);
            return 0;
        }
    }
    else if (message == MID_SAVEOBJECT && data)
        Save(*static_cast<HMESSAGEWRITE*>(data));
    else if (message == MID_LOADOBJECT && data)
        Load(*static_cast<HMESSAGEREAD*>(data));
    else if (message == MID_INITIALUPDATE)
    {
        // Unconditional in the source: a restore is rescheduled at one second; Load then replaces
        // the first-update flag, and the engine the user flags.
        s->SetNextUpdate(m_hObject, 1);
        s->SetObjectFlags(m_hObject, (s->GetObjectFlags(m_hObject) & ~FLAG_GRAVITY) | FLAG_FORCECLIENTUPDATE);
        auto flags = s->GetObjectUserFlags(m_hObject);
        s->SetObjectUserFlags(m_hObject, (on ? flags | 1u : flags & ~1u) | (1u << 3));
        first_update = true;
    }
    else if (message == MID_UPDATE)
    {
        if (first_update)
        {
            event = s->PublishWarpGate(m_hObject, settings, error);
            first_update = false;
        }
        const auto flags = s->GetObjectUserFlags(m_hObject);
        s->SetObjectUserFlags(m_hObject, on ? flags | 1u : flags & ~1u);
        s->SetNextUpdate(m_hObject, .1f);
    }
    return CClientSFX::EngineMessageFn(message, data, reason);
}
DDWORD CClientWarpGateSFX::ObjectMessageFn(HOBJECT sender, DDWORD message, HMESSAGEREAD read)
{
    if (message == 1000)
    {
        auto* s = GetServerDE();
        const auto string = s->ReadFromMessageHString(read);
        const auto* raw = string ? s->GetStringData(string) : nullptr;
        const auto flags = s->GetObjectUserFlags(m_hObject);
        if (raw)
        {
            const std::string_view text(raw);
            if (text.starts_with("TOGGLE"))
            {
                on = !on;
                ++commands;
            }
            else if (text.starts_with("ON"))
            {
                on = true;
                ++commands;
            }
            else if (text.starts_with("OFF"))
            {
                on = false;
                ++commands;
            }
        }
        s->SetObjectUserFlags(m_hObject, flags); // Original defers visible change until its next scheduled update.
        if (string)
            s->FreeString(string);
    }
    return CClientSFX::ObjectMessageFn(sender, message, read);
}
namespace
{
    void write_particles(HMESSAGEWRITE h, const WarpParticleSettings& p)
    {
        auto* s = g_pServerDE;
        s->WriteToMessageFloat(h, p.radius);
        s->WriteToMessageFloat(h, p.position_radius);
        s->WriteToMessageVector(h, &p.offset);
        s->WriteToMessageVector(h, &p.rotations);
        s->WriteToMessageFloat(h, p.min_velocity);
        s->WriteToMessageFloat(h, p.max_velocity);
        s->WriteToMessageDWord(h, p.count);
        s->WriteToMessageDWord(h, p.emit);
        s->WriteToMessageVector(h, &p.min_color);
        s->WriteToMessageVector(h, &p.max_color);
        for (float value : {p.alpha, p.min_life, p.max_life, p.delay, p.gravity})
            s->WriteToMessageFloat(h, value);
        s->WriteToMessageDWord(h, p.ramp_up);
        s->WriteToMessageDWord(h, p.ramp_down);
        s->WriteToMessageByte(h, p.align);
        save_fields::write_string(h, p.resource);
    }
    bool read_particles(HMESSAGEREAD h, WarpParticleSettings& p)
    {
        auto* s = g_pServerDE;
        p.radius = s->ReadFromMessageFloat(h);
        p.position_radius = s->ReadFromMessageFloat(h);
        s->ReadFromMessageVector(h, &p.offset);
        s->ReadFromMessageVector(h, &p.rotations);
        p.min_velocity = s->ReadFromMessageFloat(h);
        p.max_velocity = s->ReadFromMessageFloat(h);
        p.count = s->ReadFromMessageDWord(h);
        p.emit = s->ReadFromMessageDWord(h);
        s->ReadFromMessageVector(h, &p.min_color);
        s->ReadFromMessageVector(h, &p.max_color);
        for (float* value : {&p.alpha, &p.min_life, &p.max_life, &p.delay, &p.gravity})
            *value = s->ReadFromMessageFloat(h);
        p.ramp_up = s->ReadFromMessageDWord(h);
        p.ramp_down = s->ReadFromMessageDWord(h);
        const bool ok = save_fields::read_bool(h, p.align);
        return save_fields::read_string(h, p.resource, 1000) && ok;
    }
} // namespace
void CClientWarpGateSFX::Save(HMESSAGEWRITE h)
{
    // Original ClientWarpGateSFX.cpp:468-535: every member. bInitiallyOn is never assigned after
    // the constructor's FALSE (ReadProp reads InitiallyOn into bOn).
    auto* s = g_pServerDE;
    namespace F = save_fields;
    s->WriteToMessageFloat(h, settings.ramp_up);
    s->WriteToMessageFloat(h, settings.ramp_down);
    s->WriteToMessageByte(h, 0);
    const auto& sprite = settings.sprite;
    s->WriteToMessageFloat(h, sprite.min_scale);
    s->WriteToMessageFloat(h, sprite.max_scale);
    s->WriteToMessageFloat(h, sprite.alpha);
    s->WriteToMessageDWord(h, sprite.ramp_up);
    s->WriteToMessageDWord(h, sprite.ramp_down);
    s->WriteToMessageByte(h, sprite.align);
    F::write_string(h, sprite.resource);
    for (const auto& particles : settings.particles)
        write_particles(h, particles);
    s->WriteToMessageFloat(h, sound_radius);
    for (const auto& sound : sounds)
        F::write_string(h, sound);
    s->WriteToMessageByte(h, on);
    s->WriteToMessageByte(h, first_update);
}
void CClientWarpGateSFX::Load(HMESSAGEREAD h)
{
    // Original ClientWarpGateSFX.cpp:545-612, decoded and checked before any member changes.
    auto* s = g_pServerDE;
    namespace F = save_fields;
    WarpGateSettings saved;
    saved.ramp_up = s->ReadFromMessageFloat(h);
    saved.ramp_down = s->ReadFromMessageFloat(h);
    const DBYTE initially_on = s->ReadFromMessageByte(h);
    auto& sprite = saved.sprite;
    sprite.min_scale = s->ReadFromMessageFloat(h);
    sprite.max_scale = s->ReadFromMessageFloat(h);
    sprite.alpha = s->ReadFromMessageFloat(h);
    sprite.ramp_up = s->ReadFromMessageDWord(h);
    sprite.ramp_down = s->ReadFromMessageDWord(h);
    bool ok = F::read_bool(h, sprite.align);
    ok = F::read_string(h, sprite.resource, 1000) && ok;
    for (auto& particles : saved.particles)
        ok = read_particles(h, particles) && ok;
    const float saved_sound_radius = s->ReadFromMessageFloat(h);
    std::array<std::string, 3> saved_sounds;
    for (auto& sound : saved_sounds)
        ok = F::read_string(h, sound, 1000) && ok;
    bool saved_on = false, saved_first = false;
    ok = F::read_bool(h, saved_on) && ok;
    ok = F::read_bool(h, saved_first) && ok;
    if (!ok)
        return;
    std::string why;
    if (initially_on || !valid_warp_gate(saved, why) || !F::finite(saved_sound_radius, 1e7f))
    {
        s->RejectPayload(h, "warp gate block holds values the port cannot represent" +
                                (why.empty() ? std::string() : ": " + why));
        return;
    }
    settings = std::move(saved);
    sound_radius = saved_sound_radius;
    sounds = std::move(saved_sounds);
    on = saved_on;
    first_update = saved_first;
    // Decision-4 departure (save_load_contracts lot 10): SetupFX's three effects were instant
    // messages the original engine never kept, and the restored first-update flag is 0, so a
    // restored gate lost its visuals. A gate that had published them publishes them again here,
    // once, through the same path; one still before its first update publishes there as before.
    if (!first_update)
    {
        event = s->PublishWarpGate(m_hObject, settings, error);
        ++republished;
    }
}
