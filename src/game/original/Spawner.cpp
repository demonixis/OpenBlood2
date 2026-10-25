#include "CampaignProjectiles.h"
// Selected original ObjectDLL/Spawner.cpp class; the global helper lives in
// spawn_object.cpp. See spawner_contracts.md.
#include "SaveFields.h"
#include "Spawner.h"
#include "engine/collision.h"
#include "game/key_messages.h"
#include "game/pickup_messages.h"
#include "spawn_object.h"
#include <cmath>
using namespace OpenBlood2;

bool Spawner::ReadProp()
{
    GenericProp prop;
    if (g_pServerDE->GetPropGeneric("SpawnObject", &prop) == DE_OK && prop.m_String[0])
        m_hstrSpawnObject = prop.m_String;
    if (g_pServerDE->GetPropGeneric("MinVelocity", &prop) == DE_OK)
        m_vMinVelocity = prop.m_Vec;
    if (g_pServerDE->GetPropGeneric("MaxVelocity", &prop) == DE_OK)
        m_vMaxVelocity = prop.m_Vec;
    if (g_pServerDE->GetPropGeneric("Sound", &prop) == DE_OK && prop.m_String[0])
        m_hstrSound = prop.m_String;
    if (g_pServerDE->GetPropGeneric("SoundRadius", &prop) == DE_OK)
        m_fSoundRadius = prop.m_Float;
    if (g_pServerDE->GetPropGeneric("UseTriggerObjPos", &prop) == DE_OK)
        m_bUseTriggerObjPos = prop.m_Bool;
    if (g_pServerDE->GetPropGeneric("StartActive", &prop) == DE_OK)
        m_bStartActive = prop.m_Bool;
    if (g_pServerDE->GetPropGeneric("CreateRiftEffect", &prop) == DE_OK)
        m_bCreateRiftEffect = prop.m_Bool;
    if (g_pServerDE->GetPropGeneric("RespawnCount", &prop) == DE_OK)
        m_dwRespawnCount = static_cast<DDWORD>(prop.m_Long);
    if (g_pServerDE->GetPropGeneric("RespawnRate", &prop) == DE_OK)
        m_fRespawnRate = prop.m_Float;
    // Explicit bounded adaptation: the source schedules and draws from unchecked values.
    return physics_vector(m_vMinVelocity) && physics_vector(m_vMaxVelocity) && std::isfinite(m_fSoundRadius) &&
           m_fSoundRadius >= 0 && m_fSoundRadius <= 1e6f && std::isfinite(m_fRespawnRate) && m_fRespawnRate >= 0 &&
           m_fRespawnRate <= 1e6f;
}

DDWORD Spawner::EngineMessageFn(DDWORD message, void* data, DFLOAT reason)
{
    switch (message)
    {
        case MID_PRECREATE:
            if (!data)
                return 0;
            // Source reads properties for world files only, not string creation or a savegame.
            if (reason == PRECREATE_WORLDFILE && !ReadProp())
            {
                error = "Spawner velocity, sound radius or respawn rate outside the supported range";
                return 0;
            }
            break;
        case MID_INITIALUPDATE:
            // Decision-4 departure (save_load_contracts lot 7): the original runs this for a
            // savegame too, rescheduling from the constructor's StartActive = FALSE, so a running
            // spawner stopped after a restore. The restored schedule is kept instead; the engine
            // record already restores the marking.
            if (reason == INITIALUPDATE_SAVEGAME)
                break;
            g_pServerDE->SetNextUpdate(m_hObject, m_bStartActive ? m_fRespawnRate : 0.0f);
            g_pServerDE->SetObjectUserFlags(m_hObject, g_pServerDE->GetObjectUserFlags(m_hObject) | (1u << 3));
            break;
        case MID_SAVEOBJECT:
            if (data)
                Save(*static_cast<HMESSAGEWRITE*>(data));
            break;
        case MID_LOADOBJECT:
            if (data)
                Load(*static_cast<HMESSAGEREAD*>(data));
            break;
        case MID_UPDATE:
            Update();
            break;
    }
    return B2BaseClass::EngineMessageFn(message, data, reason);
}

DDWORD Spawner::ObjectMessageFn(HOBJECT sender, DDWORD message, HMESSAGEREAD read)
{
    if (message == 1000 /* MID_TRIGGER */)
    {
        ++triggers;
        if (HSTRING text = g_pServerDE->ReadFromMessageHString(read))
        {
            if (const char* bytes = g_pServerDE->GetStringData(text))
            {
                // _mbsicmp: whole-string, case-insensitive commands.
                if (same_name(bytes, "START"))
                {
                    g_pServerDE->SetNextUpdate(m_hObject, m_fRespawnRate);
                    g_pServerDE->FreeString(text);
                    return B2BaseClass::ObjectMessageFn(sender, message, read);
                }
                if (same_name(bytes, "STOP"))
                {
                    g_pServerDE->SetNextUpdate(m_hObject, 0);
                    g_pServerDE->FreeString(text);
                    return B2BaseClass::ObjectMessageFn(sender, message, read);
                }
                if (same_name(bytes, "SINGLE"))
                {
                    if (m_dwRespawnCount > 0)
                    {
                        --m_dwRespawnCount;
                        SpawnObject(nullptr);
                    }
                    g_pServerDE->FreeString(text);
                    return B2BaseClass::ObjectMessageFn(sender, message, read);
                }
            }
            g_pServerDE->FreeString(text);
        }
        // Any other payload, including none, spawns at the configured position.
        SpawnObject(sender);
    }
    return B2BaseClass::ObjectMessageFn(sender, message, read);
}

void Spawner::Update()
{
    if (m_dwRespawnCount >= 1)
        --m_dwRespawnCount;
    SpawnObject(nullptr);
    if (m_dwRespawnCount != 0)
        g_pServerDE->SetNextUpdate(m_hObject, m_fRespawnRate);
}

bool Spawner::SpawnObject(HOBJECT sender)
{
    if (m_hstrSpawnObject.empty())
        return false;
    ++spawn_attempts;
    auto* server = g_pServerDE;
    SpawnReport report;
    if (auto* name = server->GetObjectName(m_hObject))
        report.source = name;
    report.request = m_hstrSpawnObject;
    const HOBJECT origin = m_bUseTriggerObjPos && sender ? sender : m_hObject;
    DRotation rotation;
    // Bounded: a missing pose or failed draw is reported, never replaced by guessed values.
    const bool posed = server->GetObjectPos(origin, &report.position) == DE_OK &&
                       server->GetObjectRotation(origin, &rotation) == DE_OK;
    if (!posed)
        report.outcome = "Failed: spawn position/rotation unavailable";
    else
    {
        // Source draws x, y, z in order. A unit draw with the linear interval
        // formula also covers reversed authored endpoints (see ai_runtime_contracts.md).
        float* axes[]{&report.velocity.x, &report.velocity.y, &report.velocity.z};
        const float low[]{m_vMinVelocity.x, m_vMinVelocity.y, m_vMinVelocity.z};
        const float high[]{m_vMaxVelocity.x, m_vMaxVelocity.y, m_vMaxVelocity.z};
        bool drawn = true;
        for (unsigned i = 0; i < 3 && drawn; ++i)
        {
            float unit;
            drawn = server->RandomFloat(0, 1, unit);
            *axes[i] = low[i] + (high[i] - low[i]) * unit;
        }
        if (!drawn)
            report.outcome = "Failed: velocity draw unavailable";
        else
        {
            report.child = ::SpawnObject(m_hstrSpawnObject, report.position, rotation, report.velocity);
            report.outcome = report.child ? "Created" : "Failed: unsupported spawn class, properties or velocity";
        }
    }
    if (report.child)
        ++spawned;
    else
        ++spawn_failures;
    last_child = report.child;
    if (m_bCreateRiftEffect)
    {
        const auto effect =
            PublishCampaignExplosion(m_hObject, report.position, rotate_vector(rotation, {0, 0, 1}), 23);
        if (!effect.model_error.empty() || !effect.ring_error.empty() || !effect.light_error.empty())
        {
            ++rift_unavailable;
            report.outcome += "; rift FX: " + effect.model_error + effect.ring_error + effect.light_error;
        }
    }
    if (!m_hstrSound.empty() && posed)
    {
        // Original PlaySoundFromPos: 3D + reverb, inner radius = radius * 0.1,
        // SOUNDPRIORITY_MISC_LOW, played even when creation failed.
        SoundRequest request;
        request.resource = m_hstrSound;
        request.inner_radius = m_fSoundRadius * .1f;
        request.outer_radius = m_fSoundRadius;
        request.priority = 0;
        std::string sound_error;
        last_voice = server->PlayDetachedSound(m_hObject, request, report.position, sound_error);
        if (!last_voice)
            report.outcome += "; sound failed: " + sound_error;
    }
    error = report.child ? std::string{} : report.outcome;
    server->RecordSpawn(report);
    return true;
}

void Spawner::Save(HMESSAGEWRITE h)
{
    // Original Spawner.cpp:429-447.
    namespace F = save_fields;
    auto* s = g_pServerDE;
    F::write_string(h, m_hstrSound);
    F::write_string(h, m_hstrSpawnObject);
    s->WriteToMessageVector(h, &m_vMinVelocity);
    s->WriteToMessageVector(h, &m_vMaxVelocity);
    s->WriteToMessageFloat(h, m_fSoundRadius);
    s->WriteToMessageFloat(h, m_fRespawnRate);
    s->WriteToMessageByte(h, m_bUseTriggerObjPos);
    s->WriteToMessageByte(h, m_bStartActive);
    s->WriteToMessageByte(h, m_bCreateRiftEffect);
    s->WriteToMessageDWord(h, m_dwRespawnCount);
}

void Spawner::Load(HMESSAGEREAD h)
{
    // Original Spawner.cpp:458-476, checked with ReadProp's bounds before any member changes.
    namespace F = save_fields;
    auto* s = g_pServerDE;
    std::string sound, spawn;
    bool ok = F::read_string(h, sound);
    ok = F::read_string(h, spawn) && ok;
    DVector low{}, high{};
    s->ReadFromMessageVector(h, &low);
    s->ReadFromMessageVector(h, &high);
    const float radius = s->ReadFromMessageFloat(h), rate = s->ReadFromMessageFloat(h);
    bool trigger_pos = false, start_active = false, rift = false;
    ok = F::read_bool(h, trigger_pos) && ok;
    ok = F::read_bool(h, start_active) && ok;
    ok = F::read_bool(h, rift) && ok;
    const DDWORD count = s->ReadFromMessageDWord(h);
    if (!ok)
        return;
    if (!physics_vector(low) || !physics_vector(high) || !F::finite(radius) || radius < 0 || !F::finite(rate) ||
        rate < 0)
    {
        s->RejectPayload(h, "spawner block holds values the port cannot represent");
        return;
    }
    m_hstrSound = std::move(sound);
    m_hstrSpawnObject = std::move(spawn);
    m_vMinVelocity = low;
    m_vMaxVelocity = high;
    m_fSoundRadius = radius;
    m_fRespawnRate = rate;
    m_bUseTriggerObjPos = trigger_pos;
    m_bStartActive = start_active;
    m_bCreateRiftEffect = rift;
    m_dwRespawnCount = count;
}
