// Selected original AI_Mgr.cpp:3204-3539,4469-4528,5612-5670.
// See AI_FIRE_CONTRACTS.md and the Naga/energy/wind continuations for supported aiming branches.
#include "AI_Mgr.h"
#include "engine/collision.h"
#include <cmath>
#include <limits>
using namespace OpenBlood2;
namespace
{
    bool normalize(DVector& v)
    {
        const auto length = std::sqrt(vector_dot(v, v));
        if (!physics_vector(v) || !std::isfinite(length) || length < 1e-6f)
            return false;
        v = vector_scale(v, 1 / length);
        return true;
    }
} // namespace
const char* OpenBlood2::ai_clear_outcome_name(AIClearOutcome value)
{
    switch (value)
    {
        case AIClearOutcome::None:
            return "Not queried";
        case AIClearOutcome::Clear:
            return "Clear (initial miss)";
        case AIClearOutcome::Target:
            return "Target hit";
        case AIClearOutcome::Head:
            return "Head retry hit";
        case AIClearOutcome::Blocked:
            return "Blocked";
        case AIClearOutcome::Invalid:
            return "Invalid query/identity";
        case AIClearOutcome::Unavailable:
            return "Collision unavailable";
    }
    return "Unknown";
}
bool AI_Mgr::CheckClearShot(DVector origin, DVector& direction, bool alternate)
{
    clear_shot = {};
    clear_shot.origin = origin;
    auto* server = GetServerDE();
    auto* weapon = m_InventoryMgr.CurrentWeapon();
    DVector forward = direction;
    if (!server || !server->IsObjectAlive(m_hObject) || (m_hTarget && !server->IsObjectAlive(m_hTarget)) || !weapon ||
        !physics_vector(origin) || !normalize(forward))
    {
        clear_shot.outcome = AIClearOutcome::Invalid;
        return false;
    }
    const float range = alternate ? weapon->data.m_fAltRange : weapon->data.m_fRange;
    auto query = [&](DVector ray, SegmentHit& hit, SegmentResult& result)
    {
        ++clear_shot.rays;
        // Original IGNORE_LIQUID | IGNORE_GLASS | IGNORE_DESTRUCTABLE.
        result = server->IntersectSegment(origin, vector_add(origin, vector_scale(ray, range)), m_hObject, hit, false,
                                          SegmentObjects::SolidOrRayHit, SegmentFilter::AIShot);
        if (result == SegmentResult::Invalid)
            clear_shot.outcome = AIClearOutcome::Invalid;
        else if (result == SegmentResult::Unavailable)
            clear_shot.outcome = AIClearOutcome::Unavailable;
        else
            clear_shot.outcome = AIClearOutcome::Blocked;
        return result == SegmentResult::Hit;
    };
    clear_shot.direction = forward;
    if (!query(forward, clear_shot.first_hit, clear_shot.first_result))
    {
        if (clear_shot.first_result != SegmentResult::Miss)
            return false;
        clear_shot.outcome = AIClearOutcome::Clear;
        return true; // Unlike CanSee, an initial miss is clear.
    }
    if (m_hTarget && clear_shot.first_hit.object == m_hTarget)
    {
        clear_shot.outcome = AIClearOutcome::Target;
        return true;
    }
    DRotation rotation;
    if (m_hTarget && server->GetModelNodeTransform(m_hTarget, "head", &clear_shot.head, &rotation))
    {
        // Original mutates the cached target position even if this retry fails.
        m_vTargetPos = clear_shot.head;
        target_position_valid = true;
        clear_shot.retried_head = true;
        forward = vector_subtract(m_vTargetPos, origin);
        if (!normalize(forward))
        {
            clear_shot.outcome = AIClearOutcome::Invalid;
            return false;
        }
        if (query(forward, clear_shot.head_hit, clear_shot.head_result) && clear_shot.head_hit.object == m_hTarget)
        {
            direction = forward;
            clear_shot.direction = forward;
            clear_shot.outcome = AIClearOutcome::Head;
            return true;
        }
    }
    return false; // A miss on the HEAD retry is not clear.
}
bool AI_Mgr::Fire(bool alternate)
{
    ++fire_calls;
    fire_error.clear();
    fire_result = FireResult::Invalid;
    clear_shot = {};
    fire_origin = fire_direction = {};
    fire_gun_node = false;
    auto fail = [&](FireResult result, const char* error)
    {
        fire_result = result;
        fire_error = error;
        ++fire_failures;
        return false;
    };
    auto* server = GetServerDE();
    if (!server || !server->IsObjectAlive(m_hObject) || (m_hTarget && !server->IsObjectAlive(m_hTarget)))
        return fail(FireResult::Invalid, "AI fire owner/target unavailable");
    auto* weapon = m_InventoryMgr.CurrentWeapon();
    if (!weapon)
        return fail(FireResult::Unavailable, "AI has no current weapon");
    if (weapon->data.m_nType != WEAP_BERETTA && weapon->data.m_nType != WEAP_SUBMACHINEGUN &&
        weapon->data.m_nType != WEAP_FLAREGUN && weapon->data.m_nType != WEAP_MELEE &&
        weapon->data.m_nType != WEAP_NAGA_EYEBEAM && weapon->data.m_nType != WEAP_ZEALOT_ENERGYBLAST &&
        weapon->data.m_nType != WEAP_GIDEON_WIND && weapon->data.m_nType != WEAP_GIDEON_SHIELD)
        return fail(FireResult::Unavailable, "AI weapon-specific aiming is not integrated");
    DRotation rotation;
    const bool naga = weapon->data.m_nType == WEAP_NAGA_EYEBEAM;
    const bool energy = weapon->data.m_nType == WEAP_ZEALOT_ENERGYBLAST;
    fire_gun_node = server->GetModelNodeTransform(m_hObject,
                                                  energy ? "r_hand_extra2"
                                                  : naga ? "head_gun"
                                                         : "r_gun",
                                                  &fire_origin, &rotation);
    if (energy && !fire_gun_node)
        return fail(FireResult::Unavailable, "Energy blast r_hand_extra2 node unavailable");
    if (naga && !fire_gun_node)
        return fail(FireResult::Unavailable, "Naga beam head_gun node unavailable");
    if (fire_gun_node)
        fire_direction = vector_subtract(m_vTargetPos, fire_origin);
    else
    {
        if (server->GetObjectPos(m_hObject, &fire_origin) != DE_OK ||
            server->GetObjectRotation(m_hObject, &rotation) != DE_OK)
            return fail(FireResult::Unavailable, "AI body/forward fallback unavailable");
        fire_direction = rotate_vector({0, 0, 1}, rotation);
    }
    if (!normalize(fire_direction))
        return fail(FireResult::Invalid, "AI aim direction is degenerate or invalid");
    if (!energy && !CheckClearShot(fire_origin, fire_direction, alternate))
    {
        if (clear_shot.outcome == AIClearOutcome::Invalid || clear_shot.outcome == AIClearOutcome::Unavailable)
            return fail(FireResult::Unavailable, "AI clearance query failed");
        ++fire_blocked;
        fire_result = FireResult::Blocked;
        return false;
    }
    // Adapt the original server RNG to the existing owned typed shot transport.
    // This is deterministic in this engine, not retail CRT random-bit parity.
    int seed;
    if (!server->RandomInt(0, 255, seed))
        return fail(FireResult::Unavailable, "AI shot randomness unavailable");
    WeaponFire request;
    request.position = fire_origin;
    request.direction = fire_direction;
    request.weapon = weapon->data.m_nType;
    request.alternate = alternate;
    request.seed = static_cast<DBYTE>(seed);
    fire_result = m_InventoryMgr.FireCurrentWeapon(request);
    if (fire_result == FireResult::Fired)
        ++fire_shots;
    else if (fire_result == FireResult::Dry)
        ++fire_dry;
    else
        return fail(fire_result, "AI weapon shot service failed");
    // Original Fire discards the inventory return. Keep dry acceptance distinct
    // from actual shots, but never accept an unavailable/invalid engine service.
    return true;
}
bool AI_Mgr::MC_BestWeapon()
{
    state_error.clear();
    auto* server = GetServerDE();
    DVector position;
    if (!server || !server->IsObjectAlive(m_hObject) || server->GetObjectPos(m_hObject, &position) != DE_OK ||
        !physics_vector(m_vTargetPos))
        return StateFailure("AI best-weapon position/owner unavailable");
    weapon_distance =
        std::sqrt(vector_dot(vector_subtract(position, m_vTargetPos), vector_subtract(position, m_vTargetPos)));
    CWeapon* candidate = nullptr;
    if (!m_InventoryMgr.FindBestWeapon(weapon_distance, candidate))
        return StateFailure("AI best-weapon inventory query failed");
    best_weapon = candidate ? candidate->data.m_nType : 0;
    auto animate = [&](CWeapon* weapon)
    {
        const auto type = weapon ? weapon->data.m_nFireType : TYPE_PISTOL;
        // Original table has only THREE columns; TYPE_MELEE is four.
        if (!m_pAnim_Sound || type >= std::size(m_pAnim_Sound->m_nAnim_SWITCH_WEAPON))
            return StateFailure("AI weapon-switch animation column unavailable");
        return SetAnimation(m_pAnim_Sound->m_nAnim_SWITCH_WEAPON[type]);
    };
    std::string animation;
    if (m_bAnimating && !server->GetModelAnimationName(m_hObject, server->GetModelAnimation(m_hObject), animation))
        return StateFailure("AI weapon-switch playback unavailable");
    if (!m_bAnimating)
    {
        if (!candidate)
        {
            auto* current = m_InventoryMgr.CurrentWeapon();
            if (current && current->data.m_nType <= WEAP_MELEE &&
                m_InventoryMgr.GetAmmoCount(current->data.m_nAmmoType) <= 0)
                return SetNewState(StateFindAmmo);
            if (Metacmd == std::numeric_limits<int>::max())
                return StateFailure("AI Metacmd overflow");
            ++Metacmd;
        }
        else
        {
            if (!animate(m_InventoryMgr.CurrentWeapon()))
                return false;
            server->SetModelLooping(m_hObject, DFALSE);
            m_bAnimating = true;
        }
    }
    else if (server->GetModelPlaybackState(m_hObject) & MS_PLAYDONE)
    {
        if (candidate)
        {
            // Validate the destination clip before changing the inventory.
            if (!animate(candidate))
                return false;
            if (m_InventoryMgr.ChangeWeapon(candidate->data.m_nType) != InventoryResult::Ok ||
                !candidate->ShowHandModel(DFALSE))
                return StateFailure("AI best-weapon selection failed");
        }
        else
            m_bAnimating = false;
    }
    return true;
}
bool AI_Mgr::MC_Fire_Stand()
{
    state_error.clear();
    auto* server = GetServerDE();
    if (!server || !server->IsObjectAlive(m_hObject))
        return StateFailure("AI fire command requires a live owner");
    if (!m_bAnimating || m_nCurMetacmd != 7)
    {
        auto* weapon = m_InventoryMgr.CurrentWeapon();
        const auto type = weapon ? weapon->data.m_nFireType : TYPE_MELEE;
        if (!m_pAnim_Sound || type >= std::size(m_pAnim_Sound->m_nAnim_FIRE_STAND))
            return StateFailure("AI standing-fire animation table unavailable");
        if (weapon)
            m_fAttackLoadTime = weapon->data.m_fReloadTime;
        m_fLoadTimeStart = m_fTimeStart = 0;
        if (!SetAnimation(m_pAnim_Sound->m_nAnim_FIRE_STAND[type]))
        {
            // Only actual blocked clearance selects the source crawl recovery.
            // Missing model/clip/services must stay failures, not valid actions.
            if (animation_blocked)
            {
                if (!SetNewState(StateCrawlUnder))
                    return false;
                server->SetNextUpdate(m_hObject, .1f);
                return true;
            }
            return false;
        }
        server->SetModelLooping(m_hObject, DFALSE);
        m_bAnimating = true;
        m_nCurMetacmd = 7;
        server->SetNextUpdate(m_hObject, .1f);
        return true;
    }
    if (!std::isfinite(m_fLoadTimeStart) || !std::isfinite(m_fAttackLoadTime) || m_fAttackLoadTime < 0)
        return StateFailure("AI fire cadence invalid");
    std::string animation;
    if (!server->GetModelAnimationName(m_hObject, server->GetModelAnimation(m_hObject), animation))
        return StateFailure("AI standing-fire playback unavailable");
    const bool done = (server->GetModelPlaybackState(m_hObject) & MS_PLAYDONE) != 0;
    // Source can increment twice (blocked fire AND completed clip).
    if (Metacmd > std::numeric_limits<int>::max() - 2)
        return StateFailure("AI Metacmd overflow");
    if (server->GetTime() > m_fLoadTimeStart + m_fAttackLoadTime)
    {
        if (Fire())
            m_fLoadTimeStart = server->GetTime();
        else if (fire_result == FireResult::Blocked || !m_InventoryMgr.CurrentWeapon())
        {
            m_bAnimating = false;
            ++Metacmd;
        }
        else
            return StateFailure("AI fire command required shot service failed");
    }
    if (done)
    {
        m_bAnimating = false;
        ++Metacmd;
        ++animation_finishes;
    }
    return true;
}
