// Selected ObjectDLL/ClientExplosionSFX.cpp; see SCENE_explosion_contracts.md.
#include "ClientExplosionSFX.h"
#include "SaveFields.h"
#include "engine/collision.h"
#include "radius_damage.h"
#include <cmath>
using namespace OpenBlood2;
bool CClientExplosionSFX::ReadProp()
{
    GenericProp prop;
    if (g_pServerDE->GetPropGeneric("ExplosionType", &prop) == DE_OK)
        type = static_cast<DDWORD>(prop.m_Long);
    g_pServerDE->GetPropReal("Damage", &damage);
    g_pServerDE->GetPropReal("DamageRadius", &radius);
    g_pServerDE->GetPropVector("Direction", &normal);
    char text[MAX_CS_FILENAME_LEN]{};
    g_pServerDE->GetPropString("WaveFile", text, sizeof(text));
    sound = text;
    return physics_vector(normal) && std::isfinite(damage) && std::abs(damage) <= 1e6f && std::isfinite(radius) &&
           std::abs(radius) <= 1e6f;
}
DDWORD CClientExplosionSFX::EngineMessageFn(DDWORD message, void* data, DFLOAT reason)
{
    if (!g_pServerDE)
        return 0;
    if (message == MID_PRECREATE)
    {
        // Source reads properties for every creation reason, unlike most B2 classes. On a
        // restore its reads fail and leave garbage that Load overwrites; the port reads nothing.
        if (!data || (reason != PRECREATE_SAVEGAME && !ReadProp()))
            return 0;
    }
    else if (message == MID_SAVEOBJECT && data)
        Save(*static_cast<HMESSAGEWRITE*>(data));
    else if (message == MID_LOADOBJECT && data)
        Load(*static_cast<HMESSAGEREAD*>(data));
    else if (message == MID_INITIALUPDATE)
    {
        // Unconditional in the source, so a restore is rescheduled at 0.1 s and its position
        // (then overwritten by Load) is re-read.
        g_pServerDE->SetNextUpdate(m_hObject, .1f);
        g_pServerDE->GetObjectPos(m_hObject, &position);
        g_pServerDE->SetObjectUserFlags(m_hObject, g_pServerDE->GetObjectUserFlags(m_hObject) | 8u);
    }
    else if (message == MID_UPDATE)
    {
        g_pServerDE->SetNextUpdate(m_hObject, .1f);
        if (triggered)
            g_pServerDE->RemoveObject(m_hObject);
    }
    return CClientSFX::EngineMessageFn(message, data, reason);
}
DDWORD CClientExplosionSFX::ObjectMessageFn(HOBJECT sender, DDWORD message, HMESSAGEREAD read)
{
    if (message == 1000)
    {
        ++commands;
        const auto text = g_pServerDE->ReadFromMessageHString(read);
        const auto expected = g_pServerDE->CreateString("TRIGGER");
        const bool activate = text && g_pServerDE->CompareStringsUpper(text, expected);
        g_pServerDE->FreeString(text);
        g_pServerDE->FreeString(expected);
        if (activate)
        {
            SetupFX();
            triggered = true; // Source allows another trigger until its scheduled removal.
        }
    }
    return CClientSFX::ObjectMessageFn(sender, message, read);
}
void CClientExplosionSFX::SetupFX()
{
    ++activations;
    SceneExplosionReport report;
    report.source = g_pServerDE->GetObjectName(m_hObject);
    report.position = position; // Initial position, not the later moved object's position.
    report.normal = normal;     // Source sends the authored vector without server normalization.
    report.type = type;
    report.damage = damage;
    report.radius = radius;
    report.sound = sound;
    report.model_event = g_pServerDE->PublishExplosionModels(m_hObject, type, position, normal, report.model_error);
    report.sprite_event = g_pServerDE->PublishExplosionSprites(m_hObject, type, position, normal, report.sprite_error);
    report.ring_event = g_pServerDE->PublishExplosionRings(m_hObject, type, position, normal, report.ring_error);
    report.fragment_event =
        g_pServerDE->PublishExplosionFragments(m_hObject, type, position, normal, report.fragment_error);
    report.flame_event = g_pServerDE->PublishExplosionFlames(m_hObject, type, position, normal, report.flame_error);
    report.light_event = g_pServerDE->PublishExplosionLights(m_hObject, type, position, normal, report.light_error);
    // IMPORTANT: the source passes NULL, so DamageObject's null-damager guard
    // suppresses every delivery. Do not silently "repair" this gameplay behavior.
    targets = DamageObjectsInRadius(m_hObject, nullptr, position, radius, damage, DamageType::Normal);
    report.targets = targets;
    voice = 0;
    sound_error.clear();
    if (!sound.empty())
    {
        SoundRequest request;
        request.resource = sound;
        request.inner_radius = 100;
        request.outer_radius = 1000;
        request.priority = 2; // SOUNDPRIORITY_MISC_HIGH.
        voice = g_pServerDE->PlayDetachedSound(m_hObject, request, position, sound_error);
    }
    report.voice = voice;
    report.sound_error = sound_error;
    // Durable inspection distinguishes actual sprite components from missing composite FX.
    g_pServerDE->RecordSceneExplosion(report);
}
void CClientExplosionSFX::Save(HMESSAGEWRITE h)
{
    // Original ClientExplosionSFX.cpp:414-426. World-placed emitters never call Setup, so the
    // offset keeps its constructor zero. The explosion itself is an instant effect, never saved.
    auto* s = g_pServerDE;
    s->WriteToMessageVector(h, &position);
    s->WriteToMessageVector(h, &normal);
    s->WriteToMessageFloat(h, 0.0f);
    s->WriteToMessageFloat(h, damage);
    s->WriteToMessageFloat(h, radius);
    s->WriteToMessageDWord(h, type);
    save_fields::write_string(h, sound);
    s->WriteToMessageByte(h, triggered);
}
void CClientExplosionSFX::Load(HMESSAGEREAD h)
{
    // Original ClientExplosionSFX.cpp:438-450, decoded and checked before any member changes. A
    // triggered emitter is removed at its next update, as in the saved world.
    auto* s = g_pServerDE;
    namespace F = save_fields;
    DVector saved_position{}, saved_normal{};
    s->ReadFromMessageVector(h, &saved_position);
    s->ReadFromMessageVector(h, &saved_normal);
    const float offset = s->ReadFromMessageFloat(h), saved_damage = s->ReadFromMessageFloat(h),
                saved_radius = s->ReadFromMessageFloat(h);
    const DDWORD saved_type = s->ReadFromMessageDWord(h);
    std::string saved_sound;
    bool saved_triggered = false;
    bool ok = F::read_string(h, saved_sound, MAX_CS_FILENAME_LEN - 1);
    ok = F::read_bool(h, saved_triggered) && ok;
    if (!ok)
        return;
    if (!physics_vector(saved_position) || !physics_vector(saved_normal) || offset != 0 || !F::finite(saved_damage) ||
        !F::finite(saved_radius))
    {
        s->RejectPayload(h, "explosion block holds values the port cannot represent");
        return;
    }
    position = saved_position;
    normal = saved_normal;
    damage = saved_damage;
    radius = saved_radius;
    type = saved_type;
    sound = std::move(saved_sound);
    triggered = saved_triggered;
}
