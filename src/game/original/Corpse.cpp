// Original ObjectDLL/corpse.cpp: selected corpse lifetime, burning and damage.
#include "Corpse.h"
#include "ClientSplatFX.h"
#include "Gib.h"
#include "PlayerObj.h"
#include "SaveFields.h"
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
bool CCorpse::Setup(std::string_view root, bool limbs, HOBJECT fire)
{
    if (!initialized || configured || !measurements.SetSoundRoot(root))
        return false;
    sound_root = root; // Replaces the source's pointer truncated into m_UserData.
    limb_loss = limbs;
    configured = true;
    if (fire)
    {
        auto* s = GetServerDE();
        fire_source = fire;
        burn_start = s->GetTime();
        DVector offset{};
        DRotation rotation;
        if (s->CreateAttachment(m_hObject, fire, "torso", &offset, &rotation, &burn_attachment) != DE_OK)
        {
            error = "Corpse flame attachment failed";
            return false;
        }
        fire_effect = s->PublishObjectParticles(fire, 24, burn_error);
        SoundRequest request;
        request.resource = "sounds/weapons/fireloop.wav";
        request.speaker = m_hObject;
        request.outer_radius = 1000;
        request.inner_radius = 200;
        request.volume = .6f;
        request.loop = true;
        request.reverb_requested = true;
        request.priority = 3;
        std::string failure;
        burn_sound = s->PlayObjectLoopSound(m_hObject, request, failure);
        if (!failure.empty())
            burn_error += failure;
        ObjectCreateStruct state;
        state.m_ObjectType = OT_NORMAL;
        state.m_Flags = FLAG_FORCECLIENTUPDATE;
        if (s->QueryModelNodeTransform(m_hObject, "torso", &state.m_Pos, &state.m_Rotation) !=
            ServerDE::NodeQuery::Found)
        {
            error = "Corpse smoke torso unavailable";
            return false;
        }
        auto* smoke = s->CreateObject(s->GetClass("BaseClass"), &state);
        if (!smoke)
        {
            error = "Corpse smoke source unavailable";
            return false;
        }
        smoke_source = smoke->m_hObject;
    }
    return true;
}
DDWORD CCorpse::EngineMessageFn(DDWORD message, void* data, DFLOAT reason)
{
    auto* s = GetServerDE();
    if (message == MID_PRECREATE)
    {
        auto result = B2BaseClass::EngineMessageFn(message, data, reason);
        auto* state = static_cast<ObjectCreateStruct*>(data);
        // This hidden class is created by AI_Mgr or restored from a save; authored and string
        // corpses require separate validation rather than accepting a partial template.
        if (!result || !state || (reason != PRECREATE_NORMAL && reason != PRECREATE_SAVEGAME) || !state->m_Filename[0])
            return 0;
        state->m_ObjectType = OT_MODEL;
        // Original PostPropRead runs for every reason: a restore also gets these flags back.
        state->m_Flags = FLAG_VISIBLE | FLAG_RAYHIT | FLAG_GRAVITY | FLAG_MODELKEYS | FLAG_REMOVEIFOUTSIDE;
        return result;
    }
    if (message == MID_SAVEOBJECT && data)
        Save(*static_cast<HMESSAGEWRITE*>(data));
    else if (message == MID_LOADOBJECT && data)
        Load(*static_cast<HMESSAGEREAD*>(data));
    // Original INITIALUPDATE_SAVEGAME skips InitialUpdate: no damage Init (the damage block binds its
    // owner from its Load), no hit spheres (Load restores them), and the engine record keeps the
    // body, user flags and schedule.
    else if (message == MID_INITIALUPDATE && reason != INITIALUPDATE_SAVEGAME)
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
    if (dead)
        return false;
    if (!UpdateBurn())
    {
        s->SetNextUpdate(m_hObject, 0);
        return true;
    }
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
                ObjectCreateStruct create;
                create.m_Pos = pool_hit.point;
                const auto publisher = s->CreateObject(s->GetClass("CClientSplatFX"), &create);
                auto* splat = dynamic_cast<CClientSplatFX*>(publisher);
                pool_event = 0;
                if (!splat)
                    pool_error = "Cannot create original CClientSplatFX";
                else
                {
                    splat->Setup(pool_hit.point, pool_hit.normal, .1f, .02f);
                    pool_event = splat->event;
                    pool_error = splat->error;
                }
            }
            else if (pool_result != SegmentResult::Miss && pool_result != SegmentResult::Hit)
                pool_error = "Corpse blood-pool ray unavailable";
        }
        const bool multiplayer = s->IsMultiplayerGame();
        if (multiplayer)
            remove_time = now + 30.f - .1f;
        s->SetNextUpdate(m_hObject, burn_attachment ? .1f : multiplayer ? 30.f : 0.f);
    }
    else
        s->SetNextUpdate(m_hObject, .1f);
    return true;
}
DDWORD CCorpse::ObjectMessageFn(HOBJECT sender, DDWORD message, HMESSAGEREAD read)
{
    if (message == damage_message)
    {
        Damage damage;
        if (!GetServerDE()->ReadDamage(read, damage))
            return 0;
        B2BaseClass::ObjectMessageFn(sender, message, read);
        return GetServerDE()->IsObjectAlive(m_hObject) && !dead && HandleDamage(damage);
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

CCorpse::~CCorpse()
{
    ClearBurn();
}
void CCorpse::ClearBurn()
{
    auto* s = GetServerDE();
    if (!s)
        return;
    if (burn_attachment)
        s->RemoveAttachment(burn_attachment);
    if (fire_source)
        s->RemoveObject(fire_source);
    if (smoke_source)
        s->RemoveObject(smoke_source);
    if (burn_sound)
        s->KillSound(burn_sound);
    burn_attachment = 0;
    fire_source = smoke_source = nullptr;
    burn_sound = 0;
}
bool CCorpse::UpdateBurn()
{
    auto* s = GetServerDE();
    const float now = s->GetTime();
    if (!smoke_start && fire_source && now - burn_start > 5)
    {
        s->RemoveAttachment(burn_attachment);
        burn_attachment = 0;
        s->RemoveObject(fire_source);
        fire_source = nullptr;
        if (burn_sound)
            s->KillSound(burn_sound);
        burn_sound = 0;
        smoke_effect = s->PublishObjectParticles(smoke_source, 21, burn_error);
        DVector offset{};
        DRotation rotation;
        if (s->CreateAttachment(m_hObject, smoke_source, "torso", &offset, &rotation, &burn_attachment) != DE_OK)
        {
            error = "Corpse smoke attachment failed";
            return false;
        }
        smoke_start = now;
    }
    else if (!fire_source && smoke_source && now - smoke_start > 5)
    {
        s->RemoveAttachment(burn_attachment);
        burn_attachment = 0;
        s->RemoveObject(smoke_source);
        smoke_source = nullptr;
    }
    return true;
}
int CCorpse::CalculateHitLimb(DVector direction, DVector position)
{
    auto* s = GetServerDE();
    DVector dims;
    if (!measurements.hit_spheres_valid ||
        s->GetModelAnimUserDims(m_hObject, &dims, s->GetModelAnimation(m_hObject)) != DE_OK)
        return -1;
    const DVector absolute{std::abs(direction.x), std::abs(direction.y), std::abs(direction.z)};
    float length = 0;
    if (absolute.x > absolute.y && absolute.x > absolute.z)
        length = dims.x / absolute.x;
    else if (absolute.y > absolute.x && absolute.y > absolute.z)
        length = dims.y / absolute.y;
    else if (absolute.z > absolute.x && absolute.z > absolute.y)
        length = dims.z / absolute.z;
    const auto segment = vector_scale(direction, length);
    const float squared = vector_dot(segment, segment);
    if (squared == 0)
        return -1; // Original tied/zero direction yields no candidate.
    int node = -1;
    float nearest = 999;
    DBOOL hidden = false;
    DVector pose{};
    DRotation rotation;
    bool have = false;
    for (unsigned i = 0; i < NUM_STD_NODES; ++i)
    {
        const auto status = s->GetModelNodeHideStatus(m_hObject, szNodes[i], &hidden);
        if (status != DE_OK && status != DE_NOTFOUND)
            return -1;
        if (hidden)
            continue;
        const auto found = s->QueryModelNodeTransform(m_hObject, szNodes[i], &pose, &rotation);
        if (found == ServerDE::NodeQuery::Unavailable || (!have && found != ServerDE::NodeQuery::Found))
            return -1;
        have = true;
        const float t = vector_dot(segment, pose) / squared - vector_dot(segment, position) / squared;
        if (t < 0 || t > 1)
            continue;
        const auto delta = vector_subtract(pose, vector_add(position, vector_scale(segment, t)));
        const float distance = std::sqrt(vector_dot(delta, delta));
        if (distance < nearest && distance <= measurements.m_fHitSpheres[i])
        {
            nearest = distance;
            node = int(i);
        }
    }
    return CDestructable::SetProperNode(node);
}
bool CCorpse::HandleDamage(const Damage& hit)
{
    auto* s = GetServerDE();
    damage_error.clear();
    auto direction = vector_scale(hit.direction, -1);
    hit_limb = -1;
    if (limb_loss)
    {
        hit_limb = CalculateHitLimb(direction, hit.position);
        // Preserve source bitwise test, not equality: Fire (1) passes EXPLODE (5).
        if (hit_limb == -1 && !(unsigned(hit.type) & unsigned(DamageType::Explode)))
            return false;
    }
    if (m_damage.IsDead())
    {
        dead = true;
        direction.y -= 1;
        const float length = std::sqrt(vector_dot(direction, direction));
        // SetupClientGibFX reverses the same vector subsequently passed to limbs.
        direction = length > 0 ? vector_scale(direction, -(50 + hit.amount) / length) : DVector{};
        DVector pos, dims;
        s->GetObjectPos(m_hObject, &pos);
        s->GetObjectDims(m_hObject, &dims);
        if (!std::isfinite(m_damage.m_fMass) || m_damage.m_fMass < 0 || m_damage.m_fMass > 2048)
            gib_error = "Corpse gib mass exceeds bounded fragment request";
        else
            gib_events += s->PublishFleshGibs(m_hObject, pos, direction, dims, 1, unsigned(m_damage.m_fMass) / 32,
                                              gib_error) != 0;
        if (limb_loss)
        {
            constexpr int nodes[]{1, 4, 7, 14, 10}, chance[]{100, 80, 80, 60, 60};
            for (int i = 0; i < 5; ++i)
            {
                DBOOL hidden = true;
                if (s->GetModelNodeHideStatus(m_hObject, szNodes[nodes[i]], &hidden) != DE_OK || hidden)
                    continue;
                int roll = 0;
                if (i && !s->RandomInt(0, 99, roll))
                    continue;
                if (roll < chance[i])
                {
                    std::string failure;
                    const auto h = CreateLimb(m_hObject, nodes[i], direction, failure);
                    if (h)
                    {
                        last_limb = h;
                        ++limbs_created;
                    }
                    else if (gib_error.empty())
                        gib_error = failure;
                }
            }
        }
        ClearBurn();
        s->SetNextUpdate(m_hObject, .1f);
        big_gib_voice_roll = -1;
        if (s->RandomInt(0, 99, big_gib_voice_roll) && big_gib_voice_roll < 7)
        {
            ++big_gib_voice_requests;
            // Original postmortem caller has no killer filter.
            if (auto* player = CPlayerObj::LocalVoicePlayer())
                combat_voice_deliveries += player->PlayVoiceGroupEventOnClient(1, true);
        }
    }
    else if (hit_limb >= 0 && hit_limb < int(NUM_ALL_NODES) && limb_loss)
    {
        int roll;
        if (s->RandomInt(0, 99, roll) && roll < 50 && HideLimb(m_hObject, hit_limb, damage_error))
        {
            last_limb = CreateLimb(m_hObject, hit_limb, direction, damage_error);
            limbs_created += last_limb != nullptr;
        }
    }
    return true;
}

namespace
{
    constexpr float corpse_mass = 200, corpse_hit_points = 50; // Original constructor values.
}
void CCorpse::Save(HMESSAGEWRITE h)
{
    auto* s = GetServerDE();
    if (!initialized || !configured || !error.empty() || !measurements.hit_spheres_valid)
    {
        s->RejectPayload(h, "CCorpse Save: the corpse setup did not complete" + (error.empty() ? "" : ": " + error));
        return;
    }
    for (float radius : measurements.m_fHitSpheres)
        s->WriteToMessageFloat(h, radius);
    s->WriteToMessageFloat(h, corpse_mass);
    s->WriteToMessageFloat(h, corpse_hit_points);
    s->WriteToMessageString(h, sound_root.c_str());
    s->WriteToMessageByte(h, limb_loss);
    s->WriteToMessageByte(h, dead); // OpenBlood2 tail: a gibbed corpse still removes itself.
}
void CCorpse::Load(HMESSAGEREAD h)
{
    auto* s = GetServerDE();
    std::array<float, NUM_STD_NODES> spheres{};
    bool ok = true;
    for (auto& radius : spheres)
    {
        radius = s->ReadFromMessageFloat(h);
        ok = ok && std::isfinite(radius) && radius >= 0 && radius <= 1e6f;
    }
    ok = s->ReadFromMessageFloat(h) == corpse_mass && ok;
    ok = s->ReadFromMessageFloat(h) == corpse_hit_points && ok;
    char root[256]; // m_szSoundDir; the original copy into it is unchecked.
    ok = s->ReadFromMessageStringBuffer(h, root, sizeof(root)) == DE_OK && ok;
    bool limbs = true, removing = false;
    ok = save_fields::read_bool(h, limbs) && ok;
    ok = save_fields::read_bool(h, removing) && ok;
    if (!ok || !measurements.SetSoundRoot(root))
    {
        s->RejectPayload(h, "CCorpse Load: hit sphere, constant, sound directory or flag rejected");
        return;
    }
    measurements.m_fHitSpheres = spheres;
    measurements.hit_spheres_valid = true;
    sound_root = root;
    limb_loss = limbs;
    dead = removing;
    initialized = configured = true;
}
