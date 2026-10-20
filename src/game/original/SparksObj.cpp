#include "SparksObj.h"
#include "SparksFX.h"
#include "engine/collision.h"
#include <cmath>
using namespace OpenBlood2;
bool SparksObj::ReadProp()
{
    auto* s = GetServerDE();
    GenericProp p;
    if (s->GetPropGeneric("SparkSwitch", &p) == DE_OK)
        on = p.m_Bool != 0;
    // Source asks GetPropLongInt("SparkSwitch"), a BOOL property, instead of "Type".
    // Typed original lookup fails: retain type 0, even when authored Type is flesh.
    s->GetPropReal("SparkCountMin", &count_min);
    s->GetPropReal("SparkCountMax", &count_max);
    s->GetPropReal("SparkDuration", &duration);
    s->GetPropReal("SparkEmissionRadius", &emission_radius);
    s->GetPropReal("DelaySecsMin", &delay_min);
    s->GetPropReal("DelaySecsMax", &delay_max);
    s->GetPropReal("MaxSecs", &max_seconds);
    if (s->GetPropGeneric("SparkSound", &p) == DE_OK)
        sound = p.m_String;
    // Explicit bounds avoid undefined float-to-byte conversions and match the particle service budgets.
    const auto within = [](float value, float lo, float hi)
    { return std::isfinite(value) && value >= lo && value <= hi; };
    return within(count_min, 0, 255) && within(count_max, count_min, 255) && within(duration, 0, 10) && duration > 0 &&
           within(emission_radius, 0, 100) && within(delay_min, 0, 1e6f) && within(delay_max, delay_min, 1e6f) &&
           within(max_seconds, 0, 1e6f);
}
DDWORD SparksObj::EngineMessageFn(DDWORD message, void* data, DFLOAT reason)
{
    auto* s = GetServerDE();
    if (message == MID_PRECREATE)
    {
        const auto accepted = B2BaseClass::EngineMessageFn(message, data, reason);
        if (!accepted || !data || reason == PRECREATE_SAVEGAME ||
            ((reason == PRECREATE_WORLDFILE || reason == PRECREATE_STRINGPROP) && !ReadProp()))
            return 0;
        auto& c = *static_cast<ObjectCreateStruct*>(data);
        c.m_NextUpdate = .001f;
        c.m_Flags = FLAG_VISIBLE;
        return accepted;
    }
    if (message == MID_INITIALUPDATE)
    {
        s->SetNextUpdate(m_hObject, .001f);
        on_time = start_time = s->GetTime();
        if (!s->RandomFloat(delay_min, delay_max, next_delay))
            error = "Sparks delay RNG unavailable";
    }
    else if (message == MID_UPDATE)
        Update();
    return B2BaseClass::EngineMessageFn(message, data, reason);
}
void SparksObj::Update()
{
    auto* s = GetServerDE();
    s->SetNextUpdate(m_hObject, .01f);
    const auto now = s->GetTime();
    // Source tests expiry BEFORE refreshing the off-state clock.
    if (max_seconds > 0 && now > on_time + max_seconds)
    {
        s->RemoveObject(m_hObject);
        return;
    }
    if (!on)
    {
        on_time = now;
        return;
    }
    if (now <= start_time + next_delay)
        return;
    start_time = now;
    if (!s->RandomFloat(delay_min, delay_max, next_delay))
    {
        error = "Sparks delay RNG unavailable";
        return;
    }
    DRotation rotation;
    if (s->GetObjectRotation(m_hObject, &rotation) != DE_OK || s->GetObjectPos(m_hObject, &sound_position) != DE_OK)
    {
        error = "Sparks emitter pose unavailable";
        return;
    }
    direction = rotate_vector({0, 0, 1}, rotation);
    if (!sound.empty())
    {
        SoundRequest request;
        request.resource = sound;
        request.inner_radius = 100;
        request.outer_radius = 1000;
        request.priority = 2;
        std::string failure;
        voice = s->PlayDetachedSound(m_hObject, request, sound_position, failure);
        if (!failure.empty())
            sound_error = failure;
    }
    float count = 0;
    if (!s->RandomFloat(count_min, count_max, count))
    {
        error = "Sparks count RNG unavailable";
        return;
    }
    SparkSettings settings;
    // CClientSparksSFX::Setup sends ZERO position and identity rotation, not its spawn position.
    settings.direction = direction;
    settings.color1 = {200, 200, 200};
    settings.color2 = {200, 200, 0};
    settings.texture = "spritetextures\\particle1.dtx";
    settings.count = static_cast<DBYTE>(count);
    settings.duration = duration;
    settings.emission_radius = emission_radius;
    settings.radius = 500;
    settings.gravity = -500;
    std::string failure;
    effect = s->PublishSparks(m_hObject, settings, failure);
    if (!failure.empty())
        error = failure;
    ++bursts;
}
DDWORD SparksObj::ObjectMessageFn(HOBJECT sender, DDWORD message, HMESSAGEREAD read)
{
    if (message == 1000)
    {
        auto* s = GetServerDE();
        auto text = s->ReadFromMessageHString(read);
        const char* bytes = text ? s->GetStringData(text) : nullptr;
        const std::string_view command = bytes ? bytes : "";
        if (command.starts_with("SWITCH"))
            on = !on;
        else if (command.starts_with("ON"))
            on = true;
        else if (command.starts_with("OFF"))
            on = false;
        if (text)
            s->FreeString(text);
        ++commands;
        // No clock reset or schedule change on a trigger.
    }
    return B2BaseClass::ObjectMessageFn(sender, message, read);
}
