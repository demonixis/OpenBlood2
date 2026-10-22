// Selected original ObjectDLL/ClientLightningSFX.cpp.
#include "ClientLightningSFX.h"
#include "SaveFields.h"
#include "engine/collision.h"
#include "radius_damage.h"
#include <cmath>
using namespace OpenBlood2;
DDWORD CClientLightningSFX::EngineMessageFn(DDWORD message, void* data, DFLOAT reason)
{
    auto* s = GetServerDE();
    if (message == MID_PRECREATE)
    {
        if (!data)
            return 0;
        // The source reads its properties for every reason; on a restore the reads fail and
        // Load overwrites every field, so the port reads nothing.
        if (reason == PRECREATE_SAVEGAME)
            return CClientSFX::EngineMessageFn(message, data, reason);
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
    else if (message == MID_SAVEOBJECT && data)
        Save(*static_cast<HMESSAGEWRITE*>(data));
    else if (message == MID_LOADOBJECT && data)
        Load(*static_cast<HMESSAGEREAD*>(data));
    else if (message == MID_INITIALUPDATE) // Unconditional: a restore is rescheduled at 0.1 s.
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
    // The source query (INTERSECT_OBJECTS | IGNORE_NONSOLID) requests no interior
    // hits: an enclosing body, such as the Ancient One around its emitters, is skipped.
    const auto result =
        s->IntersectSegment(bolt.source, bolt.destination, nullptr, hit, false, SegmentObjects::SolidOnly,
                            SegmentFilter::IgnoreVolumes, SegmentStart::SkipInterior);
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
void CClientLightningSFX::Save(HMESSAGEWRITE h)
{
    // Original ClientLightningSFX.cpp:203-222. The endpoints are the last bolt's; a bolt is an
    // instant effect and never saved. bTriggered is never assigned after the constructor's 0.
    auto* s = g_pServerDE;
    s->WriteToMessageVector(h, &bolt.source);
    s->WriteToMessageVector(h, &bolt.destination);
    s->WriteToMessageByte(h, bolt.shape);
    s->WriteToMessageByte(h, bolt.form);
    s->WriteToMessageByte(h, bolt.type);
    s->WriteToMessageFloat(h, damage);
    s->WriteToMessageFloat(h, damage_radius);
    save_fields::write_string(h, sound);
    s->WriteToMessageFloat(h, sound_radius);
    s->WriteToMessageByte(h, 0);
}
void CClientLightningSFX::Load(HMESSAGEREAD h)
{
    // Original ClientLightningSFX.cpp:232-251, decoded and checked before any member changes,
    // with the precreate bounds.
    auto* s = g_pServerDE;
    namespace F = save_fields;
    LightningSettings saved;
    s->ReadFromMessageVector(h, &saved.source);
    s->ReadFromMessageVector(h, &saved.destination);
    saved.shape = s->ReadFromMessageByte(h);
    saved.form = s->ReadFromMessageByte(h);
    saved.type = s->ReadFromMessageByte(h);
    const float saved_damage = s->ReadFromMessageFloat(h), saved_radius = s->ReadFromMessageFloat(h);
    std::string saved_sound;
    bool ok = F::read_string(h, saved_sound);
    const float saved_sound_radius = s->ReadFromMessageFloat(h);
    const DBYTE triggered = s->ReadFromMessageByte(h);
    if (!ok)
        return;
    if (!physics_vector(saved.source) || !std::isfinite(saved.destination.x) || !std::isfinite(saved.destination.y) ||
        !std::isfinite(saved.destination.z) || saved.type > 2 || !std::isfinite(saved_damage) || saved_damage < 0 ||
        saved_damage > 1e6f || !std::isfinite(saved_radius) || saved_radius < 0 || saved_radius > 1e6f ||
        !std::isfinite(saved_sound_radius) || saved_sound_radius <= 0 || saved_sound_radius > 1e6f || triggered)
    {
        s->RejectPayload(h, "lightning block holds values the port cannot represent");
        return;
    }
    bolt = saved;
    damage = saved_damage;
    damage_radius = saved_radius;
    sound = std::move(saved_sound);
    sound_radius = saved_sound_radius;
}
