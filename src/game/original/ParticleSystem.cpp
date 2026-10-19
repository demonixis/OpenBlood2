#include "ParticleSystem.h"
using namespace OpenBlood2;
DDWORD ParticleSystem::EngineMessageFn(DDWORD message, void* data, DFLOAT reason)
{
    auto* s = GetServerDE();
    if (message == MID_PRECREATE)
    {
        if (reason != PRECREATE_WORLDFILE && reason != PRECREATE_STRINGPROP)
        {
            s->RejectPrecreate("ParticleSystem runtime/save descriptor unavailable");
            return 0;
        }
        settings.system = true;
        settings.count = 0;
        settings.ramp_time = 0;
        settings.alpha = 1;
        GenericProp p;
        if (s->GetPropGeneric("Static", &p) == DE_OK)
            static_system = p.m_Bool;
        if (s->GetPropGeneric("ParticleFlags", &p) == DE_OK && p.m_Long != 0)
        {
            s->RejectPrecreate("ParticleSystem nonzero render flags unavailable");
            return 0;
        }
        const char* keys[]{"BurstWait",       "ParticlesPerSecond", "EmissionRadius",   "MinimumVelocity",
                           "MaximumVelocity", "VelocityOffset",     "ParticleLifetime", "ParticleRadius",
                           "Gravity",         "RotationVelocity"};
        float* values[]{&settings.burst_wait,      &settings.particles_per_second,
                        &settings.position_radius, &settings.min_velocity,
                        &settings.max_velocity,    &settings.spread,
                        &settings.min_life,        &settings.radius,
                        &settings.gravity,         &settings.rotation_velocity};
        for (unsigned i = 0; i < 10; ++i)
            if (s->GetPropGeneric(keys[i], &p) == DE_OK)
                *values[i] = p.m_Float;
        settings.max_life = settings.min_life;
        if (s->GetPropGeneric("TextureName", &p) == DE_OK)
            settings.texture = p.m_String;
        s->GetPropVector("Color1", &settings.min_color);
        s->GetPropVector("Color2", &settings.max_color);
        if (!valid_particle_stream(settings))
        {
            s->RejectPrecreate("Invalid ParticleSystem descriptor");
            return 0;
        }
    }
    if (message == MID_INITIALUPDATE)
    {
        effect = s->PublishParticleStream(m_hObject, settings, error);
        if (!effect)
            s->RemoveObject(m_hObject);
    }
    if (message == MID_UPDATE)
        s->RemoveObject(m_hObject); // Original callback; no timer is armed here.
    return CClientSFX::EngineMessageFn(message, data, reason);
}
