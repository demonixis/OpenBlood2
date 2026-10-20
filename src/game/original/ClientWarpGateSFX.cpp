// Original ObjectDLL/ClientWarpGateSFX.cpp, with owned settings and typed client publication.
#include "ClientWarpGateSFX.h"
using namespace OpenBlood2;
DDWORD CClientWarpGateSFX::EngineMessageFn(DDWORD message, void* data, DFLOAT reason)
{
    auto* s = GetServerDE();
    if (message == MID_PRECREATE)
    {
        if (!data || reason == PRECREATE_SAVEGAME)
            return 0;
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
    else if (message == MID_INITIALUPDATE)
    {
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
