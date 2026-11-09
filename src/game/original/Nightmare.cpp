// Original ObjectDLL/nightmare.cpp and GameProjectiles.cpp's _ADD_ON boss projectile.
#include "Nightmare.h"
#include "CampaignProjectiles.h"
#include "LightFX.h"
#include "engine/collision.h"
#include <algorithm>
#include <cmath>
#include <numbers>

using namespace OpenBlood2;

bool Nightmare::PublishHealth(float fraction)
{
    const auto result = GetServerDE()->SendBossHealth({fraction});
    return result == DE_OK || result == DE_NOTFOUND || StateFailure("Nightmare boss health delivery failed");
}

DDWORD Nightmare::EngineMessageFn(DDWORD message, void* data, DFLOAT reason)
{
    if (message == MID_UPDATE && RuntimeReady() && full_health_ == 0 && !m_damage.IsDead())
    {
        if (!PublishHealth(1))
        {
            RuntimeFailure(state_error);
            return 0;
        }
        full_health_ = m_damage.m_fHitPoints;
    }
    return AI_Mgr::EngineMessageFn(message, data, reason);
}

bool Nightmare::OnAcceptedDamage()
{
    // Use the base's single typed damage delivery, then publish the ratio from
    // the first update, as in the source's boss-health message.
    return full_health_ == 0 || PublishHealth(std::max(0.f, m_damage.m_fHitPoints / full_health_));
}

bool Nightmare::ComputeState(int)
{
    const auto stimulus = ComputeStimuli();
    if (!perception_error.empty())
        return StateFailure(perception_error.c_str());
    if (stimulus)
        return SetNewState(m_fStimuli[0] > .75f ? StateAttackClose : StateAttackFar);
    return SetNewState(m_nState == StateIdle || m_nState == StateSearchVisual || m_nState == StateSearchSmell
                           ? StateIdle
                           : StateSearchVisual);
}

bool Nightmare::AI_STATE_Idle()
{
    if (ComputeStimuli())
        return ComputeState();
    if (!perception_error.empty())
        return StateFailure(perception_error.c_str());
    switch (Metacmd)
    {
        case 1:
            return MC_Idle();
        case 2:
            return ComputeState();
        default:
            return true;
    }
}

bool Nightmare::MC_Fire_Stand()
{
    auto* server = GetServerDE();
    if (!server || !server->IsObjectAlive(m_hObject) || !AnimSound())
        return StateFailure("Nightmare fire command requires a live owner and animation table");
    if (!m_bAnimating || m_nCurMetacmd != 7)
    {
        const bool close = m_nState == StateAttackClose;
        const auto weapon = !close ? WEAP_NIGHTMARE_FIREBALLS : shockwave_ ? WEAP_BEHEMOTH_SHOCKWAVE : WEAP_NIGHTMARE_BITE;
        const auto changed = m_InventoryMgr.ChangeWeapon(weapon);
        if (changed != InventoryResult::Ok && changed != InventoryResult::Current)
            return StateFailure("Nightmare attack weapon unavailable");
        if (!SetAnimation(AnimSound()->m_nAnim_FIRE_STAND[!close ? 2 : shockwave_ ? 5 : 4]))
            return animation_blocked ? SetNewState(StateCrawlUnder) : false;
        server->SetModelLooping(m_hObject, DFALSE);
        m_bAnimating = true;
        m_nCurMetacmd = 7;
    }
    else if (server->GetModelPlaybackState(m_hObject) & MS_PLAYDONE)
    {
        m_bAnimating = false;
        ++Metacmd;
    }
    return true;
}

bool Nightmare::AI_STATE_AttackClose()
{
    if (!GetTarget())
        return SetNewState(StateIdle);
    switch (Metacmd)
    {
        case 1:
            return MC_FaceTarget();
        case 2:
        case 3:
        {
            const bool shockwave = Metacmd == 2;
            if (shockwave)
            {
                const auto delta = vector_subtract(m_MoveObj.GetPos(), m_vTargetPos);
                m_fStimuli[0] = std::sqrt(vector_dot(delta, delta));
            }
            shockwave_ = shockwave;
            if (m_fStimuli[0] < (shockwave ? 600.f : 250.f) || (shockwave && m_nCurMetacmd == 7))
                return MC_Fire_Stand();
            if (m_fStimuli[0] >= 900.f)
                return SetNewState(StateAttackFar);
            if (!MC_FaceTarget())
                return false;
            --Metacmd;
            return MC_Walk();
        }
        case 4:
            return ComputeState();
        default:
            return true;
    }
}

bool Nightmare::AI_STATE_AttackFar()
{
    if (!GetTarget())
        return SetNewState(StateIdle);
    const auto delta = vector_subtract(m_MoveObj.GetPos(), m_vTargetPos);
    m_fStimuli[0] = std::sqrt(vector_dot(delta, delta));
    if (m_fStimuli[0] < 400 && !SetNewState(StateAttackClose))
        return false;
    switch (Metacmd)
    {
        case 1:
            return MC_FaceTarget();
        case 2:
            if (m_fStimuli[0] <= 1500 || m_nCurMetacmd == 7)
                return MC_Fire_Stand();
            [[fallthrough]];
        case 4:
        {
            const auto changed = m_InventoryMgr.ChangeWeapon(WEAP_NIGHTMARE_BITE);
            if ((changed != InventoryResult::Ok && changed != InventoryResult::Current) || !MC_FaceTarget())
                return false;
            --Metacmd;
            return MC_Run();
        }
        case 3:
            return MC_Taunt_Bold();
        case 5:
            return ComputeState();
        default:
            return true;
    }
}

CNightmareFireball::CNightmareFireball()
{
    filename = "Sprites\\lleechproj.spr";
    skin = nullptr;
    m_fLifeTime = 6.5f;
    m_damage.m_bGodMode = true;
}

CNightmareFireball::~CNightmareFireball()
{
    if (g_pServerDE && flight_sound)
        g_pServerDE->KillSound(flight_sound);
}

DDWORD CNightmareFireball::EngineMessageFn(DDWORD message, void* data, DFLOAT reason)
{
    if (message == MID_PRECREATE && !IsAddonBuild())
        return 0;
    if (message == MID_LINKBROKEN && data == tracked_target)
        tracked_target = nullptr;
    return CProjectile::EngineMessageFn(message, data, reason);
}

bool CNightmareFireball::InitialUpdate()
{
    auto* server = g_pServerDE;
    float green = 0, trail_scale = 0, arc = 0;
    constexpr float max_arc = std::numbers::pi_v<float> / 4;
    if (!server->RandomFloat(.25f, 1, green) || !server->RandomFloat(.25f, 2, trail_scale) ||
        !CProjectile::InitialUpdate() || !server->ScaleObject(m_hObject, {.3f, .3f, .3f}))
        return false;
    server->SetObjectUserFlags(m_hObject, server->GetObjectUserFlags(m_hObject) | 1u);
    server->PublishLightEffect(m_hObject, native_light_settings({1, green, 0}, 100));
    trail = server->PublishObjectParticles(m_hObject, 3, trail_error, trail_scale);
    // The original InitUpdate yaw is overwritten by the weapon's subsequent Setup alignment.
    // Retain that RNG draw before the first movement update.
    return server->RandomFloat(-max_arc, max_arc, arc);
}

bool CNightmareFireball::AcquireTarget(DVector position, const DRotation& rotation)
{
    auto* server = g_pServerDE;
    const auto end = vector_add(position, vector_scale(rotate_vector(rotation, {0, 0, 1}), 3000));
    SegmentHit hit;
    // Setup establishes both the owner to ignore and the final firing alignment.
    // On a ray miss the original reads an undefined impact point. Keep flying straight.
    const auto result = server->IntersectSegment(position, end, m_hFiredFrom, hit, false, SegmentObjects::SolidOnly,
                                                 SegmentFilter::IgnoreVolumes);
    acquired_ = true;
    if (result == SegmentResult::Miss)
        return true;
    if (result != SegmentResult::Hit)
        return false;
    hit_location_ = hit.point;
    hit_location_valid_ = true;
    if (hit.object && dynamic_cast<CBaseCharacter*>(server->HandleToObject(hit.object)))
    {
        if (server->CreateInterObjectLink(m_hObject, hit.object) != DE_OK)
            return false;
        tracked_target = hit.object;
    }
    return true;
}

bool CNightmareFireball::Update()
{
    auto* server = g_pServerDE;
    server->SetNextUpdate(m_hObject, .001f);
    if (!configured)
        return Fail("Nightmare fireball never received Setup");
    if (!flight_sound)
    {
        SoundRequest sound;
        sound.resource = "Sounds\\Weapons\\napalm\\projectile.wav";
        sound.inner_radius = 75;
        sound.outer_radius = 750;
        sound.priority = 1;
        sound.space = SoundSpace::Spatial;
        sound.speaker = m_hObject;
        sound.loop = true;
        flight_sound = server->PlayObjectLoopSound(m_hObject, sound, sound_error);
    }
    if (m_bExplode || m_damage.IsDead())
    {
        Explode();
        return false;
    }
    DVector position, velocity;
    DRotation rotation;
    if (server->GetObjectPos(m_hObject, &position) != DE_OK || server->GetObjectRotation(m_hObject, &rotation) != DE_OK ||
        server->GetVelocity(m_hObject, &velocity) != DE_OK || (!acquired_ && !AcquireTarget(position, rotation)))
        return Fail("Nightmare fireball tracking state unavailable");
    // The source overrides the base timer. Use its authored lifetime only when
    // the acquisition ray misses, so a projectile fired into empty space is bounded.
    if (!hit_location_valid_)
        return server->GetTime() <= m_fStartTime + m_fLifeTime;
    DVector target = hit_location_;
    if (tracked_target && server->GetObjectPos(tracked_target, &target) != DE_OK)
        return Fail("Nightmare fireball tracked character unavailable");
    const auto delta = vector_subtract(target, position);
    const float distance = std::sqrt(vector_dot(delta, delta));
    if (distance < 1e-6f)
        return true;
    const float amount = tracked_target ? .075f + .175f * std::max(0.f, 1 - distance / 750.f) : .125f;
    DRotation desired, blended;
    if (!align_rotation(delta, rotate_vector(rotation, {0, 1, 0}), desired) ||
        server->InterpolateRotation(blended, rotation, desired, amount) != DE_OK ||
        server->SetObjectRotation(m_hObject, &blended) != DE_OK)
        return Fail("Nightmare fireball steering failed");
    velocity = vector_scale(rotate_vector(blended, {0, 0, 1}), std::sqrt(vector_dot(velocity, velocity)));
    return server->SetVelocity(m_hObject, &velocity) == DE_OK || Fail("Nightmare fireball velocity rejected");
}

void CNightmareFireball::AddExplosion(DVector point, DVector normal)
{
    PublishCampaignExplosion(m_hObject, point, normal, 16, "Sounds\\Weapons\\napalm\\impact.wav", 2000);
}
