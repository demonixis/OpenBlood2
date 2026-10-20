// Selected original ObjectDLL/ClientLightningSFX.cpp.
#include "ClientLightningSFX.h"
#include "engine/collision.h"
#include "radius_damage.h"
#include <cmath>
using namespace OpenBlood2;
DDWORD CClientLightningSFX::EngineMessageFn(DDWORD message, void* data, DFLOAT reason)
{
    auto* s = GetServerDE();
    if (message == MID_PRECREATE)
    {
        if (!data || reason == PRECREATE_SAVEGAME)
            return 0;
        GenericProp prop;
        if (s->GetPropGeneric("LightningShape", &prop) == DE_OK)
            bolt.shape = DBYTE(prop.m_Long);
        if (s->GetPropGeneric("LightningForm", &prop) == DE_OK)
            bolt.form = DBYTE(prop.m_Long);
        if (s->GetPropGeneric("LightningType", &prop) == DE_OK)
            bolt.type = DBYTE(prop.m_Long);
        s->GetPropReal("Damage", &damage);
        s->GetPropReal("DamageRadius", &damage_radius);
        s->GetPropReal("SoundRadius", &sound_radius);
        if (s->GetPropGeneric("Sound", &prop) == DE_OK)
            sound = prop.m_String;
        if (bolt.type > 2 || !std::isfinite(damage) || damage < 0 || damage > 1e6f || !std::isfinite(damage_radius) ||
            damage_radius < 0 || damage_radius > 1e6f || !std::isfinite(sound_radius) || sound_radius <= 0 ||
            sound_radius > 1e6f)
            return 0;
    }
    else if (message == MID_INITIALUPDATE)
    {
        s->SetObjectFlags(m_hObject, (s->GetObjectFlags(m_hObject) & ~FLAG_GRAVITY) | FLAG_FORCECLIENTUPDATE);
        s->SetObjectUserFlags(m_hObject, s->GetObjectUserFlags(m_hObject) | (1u << 3));
        s->SetNextUpdate(m_hObject, .1f);
    }
    else if (message == MID_UPDATE)
        s->SetNextUpdate(m_hObject, .1f);
    return CClientSFX::EngineMessageFn(message, data, reason);
}
DDWORD CClientLightningSFX::ObjectMessageFn(HOBJECT sender, DDWORD message, HMESSAGEREAD read)
{
    if (message == 1000)
    {
        auto* s = GetServerDE();
        auto string = s->ReadFromMessageHString(read);
        const auto* value = string ? s->GetStringData(string) : nullptr;
        const bool fire = value && std::string_view(value).starts_with("FIRE");
        if (string)
            s->FreeString(string);
        if (fire)
            Fire(sender);
    }
    return CClientSFX::ObjectMessageFn(sender, message, read);
}
void CClientLightningSFX::Fire(HOBJECT sender)
{
    auto* s = GetServerDE();
    if (delivering_damage_)
    {
        error = "Reentrant lightning damage activation";
        return;
    }
    DRotation rotation;
    if (s->GetObjectPos(m_hObject, &bolt.source) != DE_OK || s->GetObjectRotation(m_hObject, &rotation) != DE_OK)
    {
        error = "Lightning pose unavailable";
        return;
    }
    bolt.destination = vector_add(bolt.source, vector_scale(rotate_vector({0, 0, 1}, rotation), 5000));
    SegmentHit hit;
    const auto result = s->IntersectSegment(bolt.source, bolt.destination, nullptr, hit, false,
                                            SegmentObjects::SolidOnly, SegmentFilter::IgnoreVolumes);
    if (result != SegmentResult::Hit && result != SegmentResult::Miss)
    {
        error = "Lightning ray unavailable";
        return;
    }
    if (result == SegmentResult::Hit)
        bolt.destination = hit.point;
    std::string failure;
    event = s->PublishLightning(m_hObject, bolt, failure);
    if (!failure.empty())
        error = failure;
    ++fires;
    if (damage_radius)
    {
        damage_sender_ = sender;
        delivering_damage_ = true;
        targets = DamageObjectsInRadius(sender, this, bolt.destination, damage_radius, damage, DamageType::Normal);
        delivering_damage_ = false;
        damage_sender_ = nullptr;
        if (targets.cancelled)
            error = "Lightning radius damage cancelled";
    }
    if (!sound.empty())
    {
        SoundRequest request;
        request.resource = sound;
        request.inner_radius = sound_radius * .1f;
        request.outer_radius = sound_radius;
        request.priority = 2;
        voice = s->PlayDetachedSound(m_hObject, request, bolt.source, failure);
        if (!failure.empty())
            sound_error = failure;
    }
}
