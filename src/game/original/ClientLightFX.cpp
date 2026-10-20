// Selected original ObjectDLL/ClientLightFX.cpp. See light_contracts.md.
#include "ClientLightFX.h"
#include "engine/collision.h"
#include <algorithm>
#include <cmath>
using namespace OpenBlood2;
ClientLightFX::ClientLightFX()
{
    AddAggregate(&damage);
}
ClientLightFX::~ClientLightFX()
{
    if (g_pServerDE)
        g_pServerDE->RemoveLightEffect(m_hObject);
}
bool ClientLightFX::ReadProp()
{
    GenericProp p;
    auto real = [&](const char* name, float& value)
    {
        if (g_pServerDE->GetPropGeneric(name, &p) == DE_OK)
            value = p.m_Float;
    };
    real("HitPoints", hit_points);
    real("LifeTime", lifetime);
    if (g_pServerDE->GetPropGeneric("LightSwitch", &p) == DE_OK)
        on = p.m_Bool;
    if (g_pServerDE->GetPropGeneric("Color", &p) == DE_OK)
        settings.color = p.m_Color;
    auto wave = [&](const char* prefix, LightWave& w)
    {
        std::string name = prefix;
        real((name + "Min").c_str(), w.min);
        real((name + "Max").c_str(), w.max);
        real((name + "Freq").c_str(), w.frequency);
        real((name + "Phase").c_str(), w.phase);
        if (g_pServerDE->GetPropGeneric((name + "Waveform").c_str(), &p) == DE_OK)
        {
            if (p.m_Long < 0 || p.m_Long > WAVE_SEARCH)
                return false;
            w.kind = static_cast<DBYTE>(p.m_Long);
        }
        return true;
    };
    if (!wave("Intensity", settings.intensity) || !wave("Radius", settings.radius))
        return false;
    if (!std::isfinite(lifetime) || !std::isfinite(hit_points) || std::abs(lifetime) > 1e6f ||
        std::abs(hit_points) > 1e6f || !valid_light_settings(settings))
        return false;
    lifetime = std::max(lifetime, 0.0f);
    settings.intensity.min = std::max(settings.intensity.min, 0.0f);
    settings.intensity.max = std::min(settings.intensity.max, 255.0f);
    settings.radius.min = std::max(settings.radius.min, 0.0f);
    if (g_pServerDE->GetPropGeneric("RampUpSound", &p) == DE_OK)
        settings.ramp_up_sound = p.m_String;
    if (g_pServerDE->GetPropGeneric("RampDownSound", &p) == DE_OK)
        settings.ramp_down_sound = p.m_String;
    const char* names[] = {"CastShadowsFlag", "SolidLightFlag", "OnlyLightWorldFlag", "DontLightBackfacingFlag",
                           "FogLightFlag"};
    for (unsigned i = 0; i < 5; ++i)
        if (g_pServerDE->GetPropGeneric(names[i], &p) == DE_OK && p.m_Bool)
            settings.flags |= 1u << (i + 3);
    return valid_light_settings(settings);
}
bool ClientLightFX::InitialUpdate()
{
    if (!damage.Init(m_hObject))
        return false;
    damage.m_fMaxHitPoints = hit_points;
    damage.SetHitPoints(hit_points);
    float offset = 0;
    if (!dynamic && !g_pServerDE->RandomFloat(.01f, .5f, offset))
        return false;
    initial_delay = .1f + offset;
    if (on)
        g_pServerDE->SetNextUpdate(m_hObject, initial_delay);
    settings.color = vector_scale(settings.color, 1.0f / 511);
    start_time = g_pServerDE->GetTime();
    settings.intensity.phase *= 3.14159265358979323846f / 180;
    settings.radius.phase *= 3.14159265358979323846f / 180;
    // The original marks only a light that starts on (USRFLG_VISIBLE | USRFLG_SAVEABLE), so an
    // initially-off light vanished from a restore. Decision-4 departure (save_load_contracts
    // lot 4): every light is saveable; until its payload lands a save naming it is refused.
    g_pServerDE->SetObjectUserFlags(m_hObject,
                                    g_pServerDE->GetObjectUserFlags(m_hObject) | (on ? 1u : 0u) | USRFLG_SAVEABLE);
    return g_pServerDE->PublishLightEffect(m_hObject, settings);
}
DDWORD ClientLightFX::EngineMessageFn(DDWORD message, void* data, DFLOAT reason)
{
    if (message == MID_PRECREATE)
    {
        if (!data || reason == 3 || !B2BaseClass::EngineMessageFn(message, data, reason))
            return 0;
        if (reason == 1 || reason == 2)
        {
            dynamic = false;
            if (!ReadProp())
                return 0;
        }
        if (!damage.spawn_valid)
            return 0;
        auto& c = *static_cast<ObjectCreateStruct*>(data);
        if (on)
        {
            c.m_NextUpdate = .01f;
            c.m_Flags = FLAG_VISIBLE;
        }
        else
            c.m_NextUpdate = 0;
        c.m_Flags |= FLAG_GOTHRUWORLD;
        return 1;
    }
    if (message == MID_INITIALUPDATE)
    {
        if (!InitialUpdate())
            g_pServerDE->RemoveObject(m_hObject);
    }
    else if (message == MID_UPDATE)
    {
        g_pServerDE->SetNextUpdate(m_hObject, .1f);
        if (lifetime > 0 && g_pServerDE->GetTime() - start_time >= lifetime)
            g_pServerDE->RemoveObject(m_hObject);
    }
    return B2BaseClass::EngineMessageFn(message, data, reason);
}
DDWORD ClientLightFX::ObjectMessageFn(HOBJECT sender, DDWORD message, HMESSAGEREAD read)
{
    if (message == 1000)
    {
        auto h = g_pServerDE->ReadFromMessageHString(read);
        const auto* text = g_pServerDE->GetStringData(h);
        last_command = text ? text : "";
        g_pServerDE->FreeString(h);
        auto flags = g_pServerDE->GetObjectUserFlags(m_hObject);
        // Source compares case-sensitive prefixes, not whole command tokens.
        if (last_command.starts_with("TOGGLE"))
        {
            flags ^= 1u;
            on = !on;
        }
        else if (last_command.starts_with("ON"))
        {
            flags |= 1u;
            on = true;
        }
        else if (last_command.starts_with("OFF"))
        {
            flags &= ~1u;
            on = false;
        }
        g_pServerDE->SetObjectUserFlags(m_hObject, flags);
    }
    const auto result = B2BaseClass::ObjectMessageFn(sender, message, read);
    if (message == damage_message && damage.IsDead())
        g_pServerDE->RemoveObject(m_hObject);
    return result;
}
