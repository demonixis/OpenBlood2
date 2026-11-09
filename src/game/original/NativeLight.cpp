// Original ObjectDLL LightFX server state and update order.
#include "LightFX.h"
#include "NativeLight.h"
#include "engine/collision.h"
#include <algorithm>
#include <cmath>

using namespace OpenBlood2;

LightFX::LightFX() : BaseClass(OT_LIGHT)
{
    AddAggregate(&damage);
}
LightFX::~LightFX()
{
    if (g_pServerDE)
        g_pServerDE->RemoveLightEffect(m_hObject);
}

int LightFX::Cycle::Advance(float now, bool intensity)
{
    if (!remaining)
        return 0;
    if (stage == Init)
    {
        stage = Minimum;
        at = now;
        return 0;
    }
    int ramp = 0;
    if (stage == Minimum || stage == Maximum)
    {
        const bool minimum = stage == Minimum;
        // The original intensity maximum tests IntensityMinTime as well.
        if (now - at <= (minimum || intensity ? hold_min : hold_max))
        {
            value = minimum ? low : high;
            return 0;
        }
        stage = minimum ? RampUp : RampDown;
        at = now;
        ramp = minimum ? 1 : -1;
    }
    const bool up_now = stage == RampUp;
    const float duration = up_now ? up : down;
    if (duration > 0 && now - at <= duration)
    {
        const float fraction = (now - at) / duration;
        value = up_now ? low + fraction * (high - low) : high - fraction * (high - low);
    }
    else
    {
        value = up_now ? high : low;
        stage = up_now ? Maximum : Minimum;
        at = now;
        if (!up_now && remaining > 0)
            --remaining;
    }
    return ramp;
}

bool LightFX::ReadProperties()
{
    auto* server = GetServerDE();
    GenericProp property;
    const auto real = [&](const char* name, float& value)
    {
        server->GetPropReal(name, &value);
        return std::isfinite(value) && std::abs(value) <= 1e6f;
    };
    const auto cycle = [&](const char* name, Cycle& track)
    {
        const std::string prefix(name);
        if (server->GetPropGeneric(("Num" + prefix + "Cycles").c_str(), &property) == DE_OK)
            track.remaining = property.m_Long;
        return real((prefix + "Min").c_str(), track.low) && real((prefix + "Max").c_str(), track.high) &&
               real((prefix + "MinTime").c_str(), track.hold_min) &&
               real((prefix + "MaxTime").c_str(), track.hold_max) && real((prefix + "RampUpTime").c_str(), track.up) &&
               real((prefix + "RampDownTime").c_str(), track.down);
    };
    if (server->GetPropGeneric("LightSwitch", &property) == DE_OK)
        on = property.m_Bool != 0;
    if (server->GetPropGeneric("NumColorCycles", &property) == DE_OK)
        color_cycles = property.m_Long;
    for (unsigned i = 0; i < colors.size(); ++i)
    {
        const auto prefix = "Color" + std::to_string(i + 1);
        if (server->GetPropGeneric(prefix.c_str(), &property) == DE_OK)
            colors[i] = property.m_Color;
        if (!physics_vector(colors[i]) || !real((prefix + "Time").c_str(), color_times[i]))
            return false;
    }
    if (server->GetPropGeneric("RampUpSound", &property) == DE_OK)
        ramp_up = property.m_String;
    if (server->GetPropGeneric("RampDownSound", &property) == DE_OK)
        ramp_down = property.m_String;
    if (!real("LifeTime", lifetime) || !real("HitPoints", hit_points) || !cycle("Intensity", intensity) ||
        !cycle("Radius", radius))
        return false;
    intensity.low = std::max(0.f, intensity.low);
    intensity.high = std::min(255.f, intensity.high);
    radius.low = std::max(0.f, radius.low);
    return true;
}

bool LightFX::Publish()
{
    auto* server = GetServerDE();
    return server->PublishLightEffect(
        m_hObject,
        native_light_settings(vector_scale(color, intensity.value), radius.value, server->GetObjectFlags(m_hObject)));
}
bool LightFX::SetConstant(DVector value, float distance)
{
    if (!physics_vector(value) || !std::isfinite(distance) || distance < 0 || distance > 1e6f ||
        std::min({value.x, value.y, value.z}) < 0 || std::max({value.x, value.y, value.z}) > 1)
        return false;
    color = value;
    intensity.value = 1;
    radius.value = distance;
    return Publish();
}
void LightFX::SetOn(bool enabled)
{
    auto* server = GetServerDE();
    on = enabled;
    const auto flags = server->GetObjectFlags(m_hObject);
    const auto user_flags = server->GetObjectUserFlags(m_hObject);
    server->SetObjectFlags(m_hObject, on ? flags | FLAG_VISIBLE : flags & ~FLAG_VISIBLE);
    server->SetObjectUserFlags(m_hObject, on ? user_flags | 1u : user_flags & ~1u);
    server->SetNextUpdate(m_hObject, on ? .001f : 0.f);
    if (!on)
        start = server->GetTime();
}

bool LightFX::Tick()
{
    auto* server = GetServerDE();
    const auto now = server->GetTime();
    server->SetNextUpdate(m_hObject, .1f);
    const auto old_color = color;
    const auto old_intensity = intensity.value, old_radius = radius.value;
    if (color_cycles)
    {
        if (!color_stage)
        {
            color_stage = 1;
            color_time = now;
        }
        else if (now - color_time >= color_times[color_stage - 1])
        {
            if (color_stage == colors.size() || (color_stage == 2 && colors[2].x == -1))
            {
                color_stage = 0;
                if (color_cycles > 0)
                    --color_cycles;
            }
            color = vector_scale(colors[color_stage], 1.f / 255);
            if (color_stage)
                ++color_stage;
            color_time = now;
        }
    }
    const auto ramp = intensity.Advance(now, true);
    radius.Advance(now, false);
    if (ramp)
    {
        SoundRequest request;
        request.resource = ramp > 0 && !ramp_up.empty() ? ramp_up : ramp_down;
        if (!request.resource.empty())
        {
            request.speaker = m_hObject;
            request.space = SoundSpace::Spatial;
            request.outer_radius = 1000;
            request.inner_radius = 100;
            request.priority = 2;
            std::string error;
            server->PlayCharacterSound(m_hObject, request, false, error);
        }
    }
    if (lifetime > 0 && now - start >= lifetime)
        return false;
    return (old_color.x == color.x && old_color.y == color.y && old_color.z == color.z &&
            old_intensity == intensity.value && old_radius == radius.value) ||
           Publish();
}

DDWORD LightFX::EngineMessageFn(DDWORD message, void* data, DFLOAT reason)
{
    auto* server = GetServerDE();
    if (message == MID_PRECREATE)
    {
        const auto accepted = BaseClass::EngineMessageFn(message, data, reason);
        if (!accepted || !data ||
            ((reason == PRECREATE_WORLDFILE || reason == PRECREATE_STRINGPROP) && !ReadProperties()))
            return 0;
        auto& state = *static_cast<ObjectCreateStruct*>(data);
        state.m_Flags |= FLAG_GOTHRUWORLD;
        return accepted;
    }
    if (message == MID_INITIALUPDATE)
    {
        damage.Init(m_hObject);
        damage.SetHitPoints(hit_points);
        damage.m_fMaxHitPoints = hit_points;
        color = vector_scale(colors[0], 1.f / 255);
        radius.value = radius.low;
        start = server->GetTime();
        SetOn(on);
        float delay = 0;
        if (reason == INITIALUPDATE_WORLDFILE || reason == INITIALUPDATE_STRINGPROP)
            server->RandomFloat(.01f, .5f, delay);
        if (on)
            server->SetNextUpdate(m_hObject, .1f + delay);
        if (!Publish())
            server->RemoveObject(m_hObject);
    }
    else if (message == MID_UPDATE && !Tick())
        server->RemoveObject(m_hObject);
    return BaseClass::EngineMessageFn(message, data, reason);
}

DDWORD LightFX::ObjectMessageFn(HOBJECT sender, DDWORD message, HMESSAGEREAD read)
{
    auto* server = GetServerDE();
    if (message == 1000 /* MID_TRIGGER */)
        if (auto text = server->ReadFromMessageHString(read))
        {
            if (const auto* raw = server->GetStringData(text))
            {
                const std::string_view command(raw);
                if (command.starts_with("SWITCH"))
                    SetOn(!on);
                else if (command.starts_with("ON"))
                    SetOn(true);
                else if (command.starts_with("OFF"))
                    SetOn(false);
            }
            server->FreeString(text);
        }
    const auto result = BaseClass::ObjectMessageFn(sender, message, read);
    if (message == damage_message && damage.IsDead())
        server->RemoveObject(m_hObject);
    return result;
}
