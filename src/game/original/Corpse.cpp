// Original ObjectDLL/corpse.cpp: selected ordinary corpse branch.
#include "Corpse.h"
#include "Sparam.h"
#include "engine/collision.h"
#include "game/game_settings.h"
#include <charconv>
#include <cmath>

using namespace OpenBlood2;
CCorpse::CCorpse() : B2BaseClass(OT_MODEL)
{
    AddAggregate(&m_damage);
}
bool CCorpse::Setup(std::string_view root, bool limbs)
{
    if (!initialized || configured || !measurements.SetSoundRoot(root))
        return false;
    sound_root = root; // Replaces the source's pointer truncated into m_UserData.
    limb_loss = limbs;
    configured = true;
    return true;
}
DDWORD CCorpse::EngineMessageFn(DDWORD message, void* data, DFLOAT reason)
{
    auto* s = GetServerDE();
    if (message == MID_PRECREATE)
    {
        auto result = B2BaseClass::EngineMessageFn(message, data, reason);
        auto* state = static_cast<ObjectCreateStruct*>(data);
        // This hidden class is created by AI_Mgr; authored/string/save corpses
        // require separate validation rather than accepting a partial template.
        if (!result || !state || reason != PRECREATE_NORMAL || !state->m_Filename[0])
            return 0;
        state->m_ObjectType = OT_MODEL;
        state->m_Flags = FLAG_VISIBLE | FLAG_RAYHIT | FLAG_GRAVITY | FLAG_MODELKEYS | FLAG_REMOVEIFOUTSIDE;
        return result;
    }
    if (message == MID_INITIALUPDATE)
    {
        DVector dims;
        if (!m_damage.Init(m_hObject) || s->GetModelAnimUserDims(m_hObject, &dims, 0) != DE_OK ||
            !measurements.GenerateHitSpheres(m_hObject) || !s->ConfigureCharacterBody(m_hObject, 200, 9))
        {
            error = "Corpse model/measurement/body initialization unavailable";
            s->SetNextUpdate(m_hObject, 0);
        }
        else
        {
            m_damage.m_fMass = 200;
            m_damage.apply_damage_physics = false;
            m_damage.SetHitPoints(50);
            m_damage.m_fMaxHitPoints = m_damage.m_fMaxMegaHitPoints = 50;
            m_damage.SetArmorPoints(0);
            m_damage.m_fMaxArmorPoints = m_damage.m_fMaxNecroArmorPoints = 0;
            s->SetObjectUserFlags(m_hObject, s->GetObjectUserFlags(m_hObject) | (100u << 24) | (1u << 3) | (1u << 5));
            initialized = true;
            s->SetNextUpdate(m_hObject, .01f);
        }
    }
    else if (message == MID_UPDATE && !Update())
        s->RemoveObject(m_hObject);
    else if (message == MID_MODELSTRINGKEY)
        OnStringKey(static_cast<ArgList*>(data));
    return B2BaseClass::EngineMessageFn(message, data, reason);
}
bool CCorpse::Update()
{
    auto* s = GetServerDE();
    ++updates;
    if (!initialized || !configured || !error.empty())
    {
        if (error.empty())
            error = "Corpse setup was not completed";
        s->SetNextUpdate(m_hObject, 0);
        return true; // Failed service remains visible; never claims animation completion.
    }
    const auto now = s->GetTime();
    if (remove_time && remove_time <= now)
        return false;
    if (s->GetModelPlaybackState(m_hObject) & MS_PLAYDONE)
    {
        animation_done = true;
        DVector from;
        if (s->GetObjectPos(m_hObject, &from) != DE_OK)
            pool_error = "Corpse blood-pool origin unavailable";
        else
        {
            auto to = from;
            to.y -= 40;
            pool_result = s->IntersectSegment(from, to, m_hObject, pool_hit, false, SegmentObjects::SolidOnly);
            if (pool_result == SegmentResult::Hit && !pool_hit.object)
            {
                pool_hit.point.y += .1f;
                pool_requested = true;
                pool_error = "Original CClientSplatFX is unavailable; blood pool is not rendered";
            }
            else if (pool_result != SegmentResult::Miss && pool_result != SegmentResult::Hit)
                pool_error = "Corpse blood-pool ray unavailable";
        }
        const bool multiplayer = GetGameType() != GAMETYPE_SINGLE;
        if (multiplayer)
            remove_time = now + 30.f - .1f;
        s->SetNextUpdate(m_hObject, multiplayer ? 30.f : 0.f);
    }
    else
        s->SetNextUpdate(m_hObject, .1f);
    return true;
}
DDWORD CCorpse::ObjectMessageFn(HOBJECT sender, DDWORD message, HMESSAGEREAD read)
{
    if (message == damage_message || message == 1000)
    {
        // Do not mutate health through the aggregate while its required corpse
        // hit/limb/gib response is unavailable, including a generic KILL trigger.
        ++damage_refusals;
        damage_error = "Corpse damage/limb/gib services unavailable";
        return 0;
    }
    return B2BaseClass::ObjectMessageFn(sender, message, read);
}
bool CCorpse::OnStringKey(ArgList* args)
{
    if (!args || !args->argv || args->argc < 1 || !args->argv[0] || sound_root.empty())
        return false;
    const std::string_view key(args->argv[0]);
    std::string_view value;
    if (key.size() > 4096 || Sparam_Get(key, "play_sound", value) != SparamStatus::Found)
        return false;
    ++sound_attempts;
    sound_error.clear();
    if (value.empty() || value.size() > 31)
    {
        sound_error = "Invalid corpse sound name";
        return false;
    }
    sound_resource = sound_root + "/" + std::string(value);
    const auto random = Sparam_Get(key, "sound_random", value);
    if (random == SparamStatus::Invalid)
    {
        sound_error = "Invalid corpse sound_random parameter";
        return false;
    }
    if (random == SparamStatus::Found)
    {
        int variant;
        if (!GetServerDE()->RandomInt(1, 3, variant)) // Original B2_LITE NRES(nMax).
            return false;
        sound_resource += std::to_string(variant);
    }
    sound_resource += ".wav";
    float radius = 1000;
    int volume = 100;
    auto number = [&](const char* name, auto& output)
    {
        std::string_view text;
        auto status = Sparam_Get(key, name, text);
        if (status == SparamStatus::Missing)
            return true;
        if (status != SparamStatus::Found || text.empty() || text.size() > 31)
            return false;
        const auto begin = text.find_first_not_of(" \t\r\n");
        const auto end = text.find_last_not_of(" \t\r\n");
        if (begin == text.npos)
            return false;
        text = text.substr(begin, end - begin + 1);
        if (text.front() == '+')
        {
            text.remove_prefix(1);
            if (text.empty() || text.front() == '-')
                return false;
        }
        const auto [last, ec] = std::from_chars(text.data(), text.data() + text.size(), output);
        return ec == std::errc{} && last == text.data() + text.size() && std::isfinite(double(output));
    };
    if (!number("sound_radius", radius) || !number("sound_volume", volume))
    {
        sound_error = "Invalid corpse sound numeric parameter";
        return false;
    }
    SoundRequest request;
    request.resource = sound_resource;
    request.speaker = m_hObject;
    request.outer_radius = radius;
    request.inner_radius = radius * .1f;
    request.volume = float(volume) / 100;
    request.priority = 3; // SOUNDPRIORITY_AI_MEDIUM.
    last_sound = GetServerDE()->PlayCharacterSound(m_hObject, request, false, sound_error);
    sound_starts += last_sound != 0;
    return last_sound != 0; // No GETHANDLE: auto release; repeated keys may overlap.
}
