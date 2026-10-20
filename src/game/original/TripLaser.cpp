// Selected original ObjectDLL/TripLaser.cpp. See trip_laser_contracts.md.
#include "TripLaser.h"
#include "SaveFields.h"
#include "engine/collision.h"
#include "game/key_messages.h"
#include "game/weapon_messages.h"
#include <cmath>
using namespace OpenBlood2;
namespace
{
    constexpr float damage_update_delta = .1f; // Source DAMAGE_UPDATE_DELTA.
} // namespace
TripLaser::~TripLaser()
{
    if (m_hSound && GetServerDE())
        GetServerDE()->KillSound(m_hSound);
}
DDWORD TripLaser::EngineMessageFn(DDWORD message, void* data, DFLOAT reason)
{
    switch (message)
    {
        case MID_UPDATE:
            Update();
            break;
        case MID_PRECREATE:
        {
            const auto result = B2BaseClass::EngineMessageFn(message, data, reason);
            if (!data)
                return 0;
            auto* state = static_cast<ObjectCreateStruct*>(data);
            if ((reason == PRECREATE_WORLDFILE || reason == PRECREATE_STRINGPROP) && !ReadProp())
                return 0;
            // The source forces the beam model and skin for every creation.
            SAFE_STRCPY(state->m_Filename, "Models\\default.abc");
            SAFE_STRCPY(state->m_SkinName, "Skins\\ammo\\beamred.dtx");
            return result;
        }
        case MID_INITIALUPDATE:
            if (reason != INITIALUPDATE_SAVEGAME && g_pServerDE->IsObjectAlive(m_hObject))
                InitialUpdate();
            break;
        case MID_TOUCHNOTIFY:
            HandleTouch(static_cast<HOBJECT>(data));
            break;
        case MID_SAVEOBJECT:
            if (data)
                Save(*static_cast<HMESSAGEWRITE*>(data));
            break;
        case MID_LOADOBJECT:
            if (data)
                Load(*static_cast<HMESSAGEREAD*>(data));
            break;
    }
    return B2BaseClass::EngineMessageFn(message, data, reason);
}
DDWORD TripLaser::ObjectMessageFn(HOBJECT sender, DDWORD message, HMESSAGEREAD read)
{
    if (message == 1000 /* MID_TRIGGER */)
        if (HSTRING text = g_pServerDE->ReadFromMessageHString(read))
        {
            if (const char* bytes = g_pServerDE->GetStringData(text))
                TriggerMsg(bytes);
            g_pServerDE->FreeString(text);
        }
    return B2BaseClass::ObjectMessageFn(sender, message, read);
}
bool TripLaser::ReadProp()
{
    GenericProp prop;
    if (g_pServerDE->GetPropGeneric("Dims", &prop) == DE_OK)
        m_vDims = prop.m_Vec;
    if (g_pServerDE->GetPropGeneric("Color", &prop) == DE_OK)
        m_vColor = prop.m_Color;
    if (g_pServerDE->GetPropGeneric("Alpha", &prop) == DE_OK)
        m_fAlpha = prop.m_Float;
    if (g_pServerDE->GetPropGeneric("StartOn", &prop) == DE_OK)
        m_bTriggered = prop.m_Bool != 0;
    if (g_pServerDE->GetPropGeneric("Damage", &prop) == DE_OK)
        m_fDamage = prop.m_Float;
    auto text = [&](const char* key, std::string& out)
    {
        if (g_pServerDE->GetPropGeneric(key, &prop) == DE_OK && prop.m_String[0])
            out = prop.m_String;
    };
    text("TouchTriggerTarget", m_hstrTriggerTarget);
    text("TouchTriggerMessage", m_hstrTriggerMessage);
    text("Sound", m_hstrSound);
    text("TouchSound", m_hstrTouchSound);
    if (!physics_vector(m_vDims) || !physics_vector(m_vColor) || !std::isfinite(m_fAlpha) ||
        !std::isfinite(m_fDamage) || std::abs(m_fDamage) > 1e6f)
    {
        g_pServerDE->RejectPrecreate("TripLaser Dims, Color, Alpha or Damage outside the supported range");
        return false;
    }
    return true;
}
void TripLaser::StartLoop()
{
    // PlaySoundFromObject(.., 400, SOUNDPRIORITY_MISC_LOW, loop, handle, time): attached,
    // inner radius 10% of the outer one.
    if (m_hstrSound.empty())
        return;
    SoundRequest request;
    request.resource = m_hstrSound;
    request.speaker = m_hObject;
    request.space = SoundSpace::Spatial;
    request.outer_radius = 400;
    request.inner_radius = 40;
    request.priority = 0;
    request.reverb_requested = true;
    request.loop = true;
    m_hSound = g_pServerDE->PlayObjectLoopSound(m_hObject, request, sound_error);
}
void TripLaser::StopLoop()
{
    if (m_hSound)
    {
        g_pServerDE->KillSound(m_hSound);
        m_hSound = 0;
    }
}
void TripLaser::Update()
{
    // Flicker the beam back on for a short time after TRIGGEROFF/KILL.
    if (m_bTriggered)
        return;
    ++flickers;
    if (g_pServerDE->GetObjectFlags(m_hObject) & FLAG_VISIBLE)
    {
        g_pServerDE->SetObjectFlags(m_hObject, m_dwFlags);
        if (m_bRemove)
            g_pServerDE->RemoveObject(m_hObject);
        else
            g_pServerDE->SetNextUpdate(m_hObject, 0);
    }
    else
    {
        g_pServerDE->SetObjectFlags(m_hObject, m_dwFlags | FLAG_VISIBLE);
        float next = .2f;
        if (!g_pServerDE->RandomFloat(.2f, .3f, next))
            error = "TripLaser flicker RNG failed";
        g_pServerDE->SetNextUpdate(m_hObject, next);
    }
}
void TripLaser::InitialUpdate()
{
    g_pServerDE->SetObjectUserFlags(m_hObject, g_pServerDE->GetObjectUserFlags(m_hObject) | 1u << 3); // SAVEABLE
    m_vColor = vector_scale(m_vColor, 1.f / 255);
    m_vDims = vector_scale(m_vDims, 2);
    if (!g_pServerDE->ScaleObject(m_hObject, m_vDims) ||
        !g_pServerDE->SetObjectColor(m_hObject, m_vColor, m_fAlpha))
        error = "TripLaser scale or color rejected";
    DVector position;
    DRotation rotation;
    g_pServerDE->GetObjectPos(m_hObject, &position);
    g_pServerDE->GetObjectRotation(m_hObject, &rotation);
    const auto right = rotate_vector({1, 0, 0}, rotation), up = rotate_vector({0, 1, 0}, rotation),
               forward = rotate_vector({0, 0, 1}, rotation);
    // Use the longest scaled dimension; ties prefer x, then y, as the source branch order.
    DVector half{};
    if (m_vDims.x >= m_vDims.y && m_vDims.x >= m_vDims.z)
        half = vector_scale(right, m_vDims.x);
    else if (m_vDims.y >= m_vDims.x && m_vDims.y >= m_vDims.z)
        half = vector_scale(up, m_vDims.y);
    else if (m_vDims.z >= m_vDims.x && m_vDims.z >= m_vDims.y)
        half = vector_scale(forward, m_vDims.z);
    half = vector_scale(half, .5f);
    m_vEndPoints[0] = vector_subtract(position, half);
    m_vEndPoints[1] = vector_add(position, half);
    DVector dims{std::max(std::abs(half.x), 1.f), std::max(std::abs(half.y), 1.f), std::max(std::abs(half.z), 1.f)};
    if (g_pServerDE->SetObjectDims(m_hObject, &dims) != DE_OK)
        error = "TripLaser dimensions rejected";
    if (m_bTriggered)
    {
        m_dwFlags |= FLAG_VISIBLE | FLAG_TOUCH_NOTIFY | FLAG_NOLIGHT;
        StartLoop();
    }
    g_pServerDE->SetObjectFlags(m_hObject, m_dwFlags);
    g_pServerDE->SetNextUpdate(m_hObject, 0);
}
void TripLaser::TriggerMsg(const char* message)
{
    // CompareStringsUpper: whole-string, case-insensitive; each command is tested in turn.
    if (same_name(message, "TRIGGER"))
    {
        m_bTriggered = true;
        m_dwFlags = FLAG_TOUCH_NOTIFY | FLAG_VISIBLE | FLAG_NOLIGHT;
        if (!m_hSound)
            StartLoop();
    }
    for (auto command : {"TRIGGEROFF", "KILL"})
        if (same_name(message, command))
        {
            m_bTriggered = false;
            m_dwFlags = 0;
            StopLoop();
            float next = .25f;
            if (!g_pServerDE->RandomFloat(.25f, .5f, next))
                error = "TripLaser flicker RNG failed";
            g_pServerDE->SetNextUpdate(m_hObject, next);
            if (same_name(command, "KILL"))
                m_bRemove = true;
        }
    g_pServerDE->SetObjectFlags(m_hObject, m_dwFlags);
}
void TripLaser::HandleTouch(HOBJECT other)
{
    ++touches;
    // Apply damage ten times a second.
    const float now = g_pServerDE->GetTime();
    if (now <= m_fDamageTime)
        return;
    m_fDamageTime = now + damage_update_delta;
    // Something touches the box: the beam segment must hit that object first.
    SegmentHit hit;
    if (g_pServerDE->IntersectSegment(m_vEndPoints[0], m_vEndPoints[1], nullptr, hit) != SegmentResult::Hit ||
        hit.object != other || !g_pServerDE->IsObjectAlive(other))
        return;
    ++beam_hits;
    const auto direction = vector_subtract(m_vEndPoints[1], m_vEndPoints[0]);
    if (auto message = g_pServerDE->StartMessageToObject(this, other, damage_message))
    {
        if (g_pServerDE->WriteDamage(message, {direction, hit.point, m_fDamage * damage_update_delta,
                                               DamageType::Normal, m_hObject}) == DE_OK)
        {
            g_pServerDE->EndMessage(message);
            ++damage_messages;
        }
        else
            g_pServerDE->EndHMessageWrite(message);
    }
    if (!m_hstrTouchSound.empty())
    {
        SoundRequest request;
        request.resource = m_hstrTouchSound;
        request.speaker = m_hObject;
        request.space = SoundSpace::Spatial;
        request.outer_radius = 800;
        request.inner_radius = 80;
        request.priority = 2; // SOUNDPRIORITY_MISC_HIGH
        request.reverb_requested = true;
        last_touch_voice = g_pServerDE->PlayCharacterSound(m_hObject, request, false, sound_error);
    }
}
void TripLaser::Save(HMESSAGEWRITE h)
{
    // Original TripLaser.cpp:488-511. The damage clock is relative; the loop is not saved.
    auto* s = g_pServerDE;
    namespace F = save_fields;
    s->WriteToMessageVector(h, &m_vDims);
    s->WriteToMessageVector(h, &m_vColor);
    s->WriteToMessageFloat(h, m_fAlpha);
    s->WriteToMessageFloat(h, m_fDamage);
    s->WriteToMessageFloat(h, m_fDamageTime - s->GetTime());
    F::write_string(h, m_hstrTriggerTarget);
    F::write_string(h, m_hstrTriggerMessage);
    F::write_string(h, m_hstrSound);
    F::write_string(h, m_hstrTouchSound);
    s->WriteToMessageVector(h, &m_vEndPoints[0]);
    s->WriteToMessageVector(h, &m_vEndPoints[1]);
    s->WriteToMessageDWord(h, m_dwFlags);
    s->WriteToMessageByte(h, m_bTriggered);
    s->WriteToMessageByte(h, m_bRemove);
}
void TripLaser::Load(HMESSAGEREAD h)
{
    // Original TripLaser.cpp:523-563, decoded and checked before any member changes. A triggered
    // laser restarts its attached loop, as the original Load does.
    auto* s = g_pServerDE;
    namespace F = save_fields;
    const float now = s->GetTime();
    DVector dims{}, color{}, ends[2]{};
    std::string text[4];
    s->ReadFromMessageVector(h, &dims);
    s->ReadFromMessageVector(h, &color);
    const float alpha = s->ReadFromMessageFloat(h), damage = s->ReadFromMessageFloat(h);
    const float damage_time = s->ReadFromMessageFloat(h);
    bool ok = true;
    for (auto& value : text)
        ok = F::read_string(h, value) && ok;
    s->ReadFromMessageVector(h, &ends[0]);
    s->ReadFromMessageVector(h, &ends[1]);
    const DDWORD flags = s->ReadFromMessageDWord(h);
    bool triggered = false, remove = false;
    ok = F::read_bool(h, triggered) && ok;
    ok = F::read_bool(h, remove) && ok;
    if (!ok)
        return;
    if (!physics_vector(dims) || !physics_vector(color) || !physics_vector(ends[0]) || !physics_vector(ends[1]) ||
        !std::isfinite(alpha) || !F::finite(damage) || !std::isfinite(damage_time))
    {
        s->RejectPayload(h, "trip laser block holds values the port cannot represent");
        return;
    }
    m_vDims = dims;
    m_vColor = color;
    m_fAlpha = alpha;
    m_fDamage = damage;
    m_fDamageTime = F::restored_time(damage_time, now);
    m_hstrTriggerTarget = std::move(text[0]);
    m_hstrTriggerMessage = std::move(text[1]);
    m_hstrSound = std::move(text[2]);
    m_hstrTouchSound = std::move(text[3]);
    m_vEndPoints[0] = ends[0];
    m_vEndPoints[1] = ends[1];
    m_dwFlags = flags;
    m_bTriggered = triggered;
    m_bRemove = remove;
    if (m_bTriggered)
    {
        StopLoop();
        StartLoop();
    }
}
