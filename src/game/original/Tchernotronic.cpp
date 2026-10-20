// Original ObjectDLL/Tchernotronic.cpp (Monolith). See tchernotronic_contracts.md.
#include "Tchernotronic.h"
#include "PlayerObj.h"
#include "SaveFields.h"
#include "radius_damage.h"
#include "trigger_helpers.h"
#include <cctype>
using namespace OpenBlood2;
namespace
{
    constexpr float remove_delay = .5f; // Source TIME_REMOVE.
    bool same_upper(const char* a, const char* b)
    {
        for (; *a && *b; ++a, ++b)
            if (std::toupper(static_cast<unsigned char>(*a)) != std::toupper(static_cast<unsigned char>(*b)))
                return false;
        return !*a && !*b;
    }
} // namespace
DDWORD Tchernotronic::EngineMessageFn(DDWORD message, void* data, DFLOAT reason)
{
    switch (message)
    {
        case MID_UPDATE:
            if (!Update())
                g_pServerDE->RemoveObject(m_hObject);
            break;
        case MID_PRECREATE:
        {
            GenericProp prop; // The original reads properties for every reason; none exist on a restore.
            auto text = [&](const char* key, std::string& out)
            {
                if (g_pServerDE->GetPropGeneric(key, &prop) == DE_OK && prop.m_String[0])
                    out = prop.m_String;
            };
            if (reason == PRECREATE_WORLDFILE || reason == PRECREATE_STRINGPROP)
            {
                text("DestroyTriggerTarget", m_hstrTriggerTarget);
                text("DestroyTriggerMessage", m_hstrTriggerMessage);
            }
            break;
        }
        case MID_INITIALUPDATE:
            if (reason != INITIALUPDATE_SAVEGAME && g_pServerDE->IsObjectAlive(m_hObject))
                InitialUpdate();
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
DDWORD Tchernotronic::ObjectMessageFn(HOBJECT sender, DDWORD message, HMESSAGEREAD read)
{
    if (message == 1000 /* MID_TRIGGER */)
        if (HSTRING text = g_pServerDE->ReadFromMessageHString(read))
        {
            if (const char* bytes = g_pServerDE->GetStringData(text))
                TriggerMsg(sender, bytes);
            g_pServerDE->FreeString(text);
        }
    return B2BaseClass::ObjectMessageFn(sender, message, read);
}
void Tchernotronic::InitialUpdate()
{
    auto* s = g_pServerDE;
    s->SetObjectUserFlags(m_hObject, s->GetObjectUserFlags(m_hObject) | 1u << 3); // USRFLG_SAVEABLE
    s->SetObjectFlags(m_hObject, FLAG_VISIBLE | FLAG_SOLID);
    m_nRestAnim = s->GetAnimIndex(m_hObject, "static_model");
    m_nMovingAnim = s->GetAnimIndex(m_hObject, "animate");
    s->SetModelLooping(m_hObject, DFALSE);
    // Source ignores the result: an invalid rest index leaves vDims uninitialized. Keep the
    // current dimensions instead of importing that undefined value.
    DVector dims;
    if (s->GetModelAnimUserDims(m_hObject, &dims, m_nRestAnim) != DE_OK && s->GetObjectDims(m_hObject, &dims) != DE_OK)
        dims = {};
    s->SetObjectDims(m_hObject, &dims);
    s->SetBlockingPriority(m_hObject, 100); // BLOCKPRIORITY_NONPUSHABLE
    s->SetNextUpdate(m_hObject, 0);         // Wait to be triggered.
}
bool Tchernotronic::Update()
{
    auto* s = g_pServerDE;
    const float time = s->GetTime();
    if (m_bTimer)
    {
        if (time >= m_fRemoveTime)
        {
            if (!m_hstrTriggerTarget.empty() && !m_hstrTriggerMessage.empty())
            {
                auto target = s->CreateString(m_hstrTriggerTarget.c_str());
                auto text = s->CreateString(m_hstrTriggerMessage.c_str());
                SendTriggerMsgToObjects(this, target, text);
                s->FreeString(target);
                s->FreeString(text);
                ++destroy_messages;
            }
            return false;
        }
    }
    else if (s->GetModelPlaybackState(m_hObject) & MS_PLAYDONE)
    {
        DVector position;
        s->GetObjectPos(m_hObject, &position);
        AddExplosion(position);
        if (!s->IsObjectAlive(m_hObject))
            return true; // Removed by a radius-damage callback; nothing left to schedule.
        m_fRemoveTime = time + remove_delay;
        m_bTimer = true;
    }
    s->SetNextUpdate(m_hObject, .05f);
    return true;
}
void Tchernotronic::TriggerMsg(HOBJECT sender, const char* message)
{
    // See if he's been triggered to start moving: players cannot trigger it.
    if (dynamic_cast<CPlayerObj*>(g_pServerDE->HandleToObject(sender)))
    {
        ++ignored_player_triggers;
        return;
    }
    if (!same_upper(message, "TRIGGER"))
        return;
    ++triggers;
    PlayAnimation(m_nMovingAnim);
    g_pServerDE->SetNextUpdate(m_hObject, .1f);
}
bool Tchernotronic::PlayAnimation(DDWORD animation)
{
    auto* s = g_pServerDE;
    if (animation == DDWORD(-1))
        return false;
    const auto current = s->GetModelAnimation(m_hObject);
    if (current == animation && (s->GetModelPlaybackState(m_hObject) & MS_PLAYDONE))
        return false;
    if (current != animation)
        s->SetModelAnimation(m_hObject, animation);
    return true;
}
void Tchernotronic::AddExplosion(DVector position)
{
    auto* s = g_pServerDE;
    ++explosions;
    // SFX_EXPLOSIONFX_ID with EXP_GRENADE and world up, then the C4 sound, then damage.
    auto& r = explosion;
    r = {};
    r.source = s->GetObjectName(m_hObject);
    r.position = position;
    r.normal = {0, 1, 0};
    r.type = 3;
    r.damage = 100;
    r.radius = 200;
    r.model_event = s->PublishExplosionModels(m_hObject, r.type, position, r.normal, r.model_error);
    r.sprite_event = s->PublishExplosionSprites(m_hObject, r.type, position, r.normal, r.sprite_error);
    r.ring_event = s->PublishExplosionRings(m_hObject, r.type, position, r.normal, r.ring_error);
    r.fragment_event = s->PublishExplosionFragments(m_hObject, r.type, position, r.normal, r.fragment_error);
    r.flame_event = s->PublishExplosionFlames(m_hObject, r.type, position, r.normal, r.flame_error);
    r.light_event = s->PublishExplosionLights(m_hObject, r.type, position, r.normal, r.light_error);
    SoundRequest request;
    r.sound = request.resource = "Sounds\\Weapons\\c4\\explosion_1.wav";
    request.inner_radius = 100; // PlaySoundFromPos: radius * 0.1.
    request.outer_radius = 1000;
    request.priority = 1; // SOUNDPRIORITY_MISC_MEDIUM.
    r.voice = s->PlayDetachedSound(m_hObject, request, position, r.sound_error);
    targets = r.targets = DamageObjectsInRadius(m_hObject, this, position, 200, 100, DamageType::Explode);
    s->RecordSceneExplosion(r);
}
void Tchernotronic::Save(HMESSAGEWRITE h)
{
    // Original Tchernotronic.cpp:434-446; the removal clock is relative.
    auto* s = g_pServerDE;
    save_fields::write_string(h, m_hstrTriggerTarget);
    save_fields::write_string(h, m_hstrTriggerMessage);
    s->WriteToMessageDWord(h, m_nRestAnim);
    s->WriteToMessageDWord(h, m_nMovingAnim);
    s->WriteToMessageByte(h, m_bTimer);
    s->WriteToMessageFloat(h, m_fRemoveTime - s->GetTime());
}
void Tchernotronic::Load(HMESSAGEREAD h)
{
    // Original Tchernotronic.cpp:457-480, decoded and checked before any member changes.
    auto* s = g_pServerDE;
    namespace F = save_fields;
    const float now = s->GetTime();
    std::string target, text;
    bool ok = F::read_string(h, target);
    ok = F::read_string(h, text) && ok;
    const DDWORD rest = s->ReadFromMessageDWord(h), moving = s->ReadFromMessageDWord(h);
    bool timer = false;
    ok = F::read_bool(h, timer) && ok;
    const float remove = s->ReadFromMessageFloat(h);
    if (!ok)
        return;
    if (!std::isfinite(remove))
    {
        s->RejectPayload(h, "Tchernotronic removal clock is not finite");
        return;
    }
    m_hstrTriggerTarget = std::move(target);
    m_hstrTriggerMessage = std::move(text);
    m_nRestAnim = rest;
    m_nMovingAnim = moving;
    m_bTimer = timer;
    m_fRemoveTime = F::restored_time(remove, now);
}
