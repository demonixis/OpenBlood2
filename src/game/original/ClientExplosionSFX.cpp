// Selected ObjectDLL/ClientExplosionSFX.cpp; see SCENE_EXPLOSION_CONTRACTS.md.
#include "ClientExplosionSFX.h"
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
        // Source reads properties for every creation reason, unlike most B2 classes.
        if (!data || reason == 3 || !ReadProp())
            return 0;
    }
    else if (message == MID_INITIALUPDATE)
    {
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
