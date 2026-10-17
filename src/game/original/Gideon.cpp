// Original ObjectDLL/Gideon.cpp:323-473,583-600,770-808 (Monolith, 1998).
// Typed failure/lifetime adaptations are documented in GIDEON_COMBAT_CONTRACTS.md.
#include "Gideon.h"
#include "engine/collision.h"
#include <cmath>
#include <limits>
using namespace OpenBlood2;

bool Gideon::ComputeState(int value)
{
    state_error.clear();
    const int stim = value == -1 ? ComputeStimuli() : value;
    auto* s = GetServerDE();
    if (!s || !s->IsObjectAlive(m_hObject) || stim < 0 || stim > 4 || !std::isfinite(s->GetTime()) ||
        !std::isfinite(m_fStimuli[0]) || !std::isfinite(m_fStimuli[4]) || !std::isfinite(m_fStimuli[5]) ||
        !std::isfinite(m_fLastShield) || !std::isfinite(m_fShieldDuration) || m_fShieldDuration < 0 ||
        !std::isfinite(m_fLastPanic + 15))
        return StateFailure("Gideon decision inputs or required perception unavailable");
    if (!stim)
    {
        switch (m_nState)
        {
            case StateIdle:
                return SetNewState(StateSpecial1);
            case StateTeleport:
                return SetNewState(StateSearchVisual);
            case StateSearchVisual:
                return SetNewState(StateIdle);
            default:
                return SetNewState(StateTeleport);
        }
    }
    if (m_fStimuli[5] < .25f || m_fStimuli[4] > .75f)
    {
        if (s->GetTime() - m_fLastShield > m_fShieldDuration)
            return SetNewState(StateAttackClose);
        if (m_fStimuli[5] < .05f && m_fLastPanic + 15 >= s->GetTime())
        {
            // Preserve the source's >= (recent panic permits another panic).
            m_fLastPanic = s->GetTime();
            return SetNewState(StateTeleport);
        }
        return SetNewState(StateAttackClose);
    }
    if (m_nState == StateSearchVisual)
        return SetNewState(StateSpecial1);
    return SetNewState(m_fStimuli[0] > .94f ? StateAttackClose : m_fStimuli[0] > .75f ? StateAttackFar : StateSpecial1);
}

bool Gideon::MC_Fire_Stand()
{
    state_error.clear();
    auto* s = GetServerDE();
    const auto* table = AnimSound();
    if (!s || !s->IsObjectAlive(m_hObject) || !table || !std::isfinite(s->GetTime()) ||
        !std::isfinite(m_damage.m_fArmorPoints) || !std::isfinite(m_fLastShield) || !std::isfinite(m_fShieldDuration) ||
        m_fShieldDuration < 0)
        return StateFailure("Gideon fire command owner/table/clock unavailable");
    if (!m_bAnimating || m_nCurMetacmd != 7)
    {
        attack_roll = -1;
        const bool shield = m_damage.m_fArmorPoints <= 25 && s->GetTime() - m_fLastShield > m_fShieldDuration;
        unsigned column = TYPE_RIFLE;
        if (shield)
            selected_weapon = WEAP_GIDEON_SHIELD;
        else
        {
            if (!s->RandomInt(0, 99, attack_roll))
                return StateFailure("Gideon attack chance draw failed");
            if (m_nState == StateAttackFar && attack_roll < 70)
            {
                selected_weapon = WEAP_ZEALOT_ENERGYBLAST;
                column = TYPE_MAGIC; // Source selects MAGIC even though this weapon's fire type is MELEE.
            }
            else if (m_nState != StateAttackFar && attack_roll < 50)
                selected_weapon = WEAP_GIDEON_WIND;
            else
                selected_weapon = WEAP_NAGA_EYEBEAM;
        }
        const auto choice = m_InventoryMgr.ChangeWeapon(selected_weapon);
        if (choice != InventoryResult::Ok && choice != InventoryResult::Current)
            return StateFailure("Gideon selected weapon is unavailable");
        if (shield)
        {
            m_damage.SetArmorPoints(m_damage.m_fArmorPoints + 100);
            m_fLastShield = s->GetTime();
            ++shield_grants;
        }
        // Source ignores bRet. Keep preceding selection/armor effects, but do not
        // claim a running animation when the required engine service fails.
        if (!SetAnimation(table->m_nAnim_FIRE_STAND[column]))
            return false;
        s->SetModelLooping(m_hObject, DFALSE);
        m_bAnimating = true;
        m_nCurMetacmd = 7;
    }
    else
    {
        std::string animation;
        if (!s->GetModelAnimationName(m_hObject, s->GetModelAnimation(m_hObject), animation))
            return StateFailure("Gideon fire playback unavailable");
        if (s->GetModelPlaybackState(m_hObject) & MS_PLAYDONE)
        {
            if (Metacmd == std::numeric_limits<int>::max())
                return StateFailure("Gideon Metacmd overflow");
            // Source always tries the absent Soul Hook and ignores the result.
            // Retain this failed request, not an invented weapon or fallback.
            completion_selection = m_InventoryMgr.ChangeWeapon(WEAP_SOUL_HOOK);
            m_bAnimating = false;
            ++Metacmd;
            ++animation_finishes;
        }
    }
    // Unlike base MC_Fire_Stand, no timed firing: only model keys fire here.
    return true;
}

bool Gideon::MagicPowerup()
{
    ++powerup_calls;
    powerup_effect = 0;
    powerup_error.clear();
    auto* s = GetServerDE();
    if (!s || !s->IsObjectAlive(m_hObject))
    {
        powerup_error = "Gideon powerup requires a live owner";
        return false;
    }
    powerup_effect = s->PublishObjectParticles(m_hObject, 36, powerup_error);
    return powerup_effect != 0;
}

bool Gideon::MC_Extra(std::string_view text)
{
    ++extra_calls;
    ai_key_error.clear();
    auto* s = GetServerDE();
    if (!s || !s->IsObjectAlive(m_hObject) || text.empty() || text.size() > 63 || text.find('\0') != text.npos)
    {
        ai_key_error = "Gideon extra key owner/text invalid (maximum 63 bytes)";
        return false;
    }
    if (text.starts_with("powerup"))
    {
        if (MagicPowerup())
            return true;
        ai_key_error = powerup_error;
        return false;
    }
    m_szFireNode = text;
    if (Fire())
        return true;
    ai_key_error = fire_error;
    return false;
}

bool Gideon::Fire(bool alternate)
{
    ++fire_calls;
    fire_error.clear();
    fire_result = FireResult::Invalid;
    clear_shot = {}; // Boss override does not call CheckClearShot.
    fire_origin = fire_direction = {};
    fire_gun_node = false;
    auto fail = [&](FireResult result, const char* error)
    {
        fire_result = result;
        fire_error = error;
        ++fire_failures;
        return false;
    };
    auto* s = GetServerDE();
    if (!s || !s->IsObjectAlive(m_hObject) || (GetTarget() && !s->IsObjectAlive(GetTarget())))
        return fail(FireResult::Invalid, "Gideon fire owner/target unavailable");
    auto* weapon = m_InventoryMgr.CurrentWeapon();
    if (!weapon)
        return fail(FireResult::Unavailable, "Gideon has no current weapon");
    if (m_szFireNode.empty() || m_szFireNode.size() > 63 || m_szFireNode.find('\0') != m_szFireNode.npos)
        return fail(FireResult::Invalid, "Gideon fire node invalid");
    DRotation rotation;
    fire_gun_node = s->GetModelNodeTransform(m_hObject, m_szFireNode.c_str(), &fire_origin, &rotation);
    if (fire_gun_node)
        fire_direction = vector_subtract(m_vTargetPos, fire_origin);
    else
    {
        if (!m_MoveObj.initialized || !m_MoveObj.sampled || s->GetObjectPos(m_hObject, &fire_origin) != DE_OK)
            return fail(FireResult::Unavailable, "Gideon cached-forward/body fallback unavailable");
        fire_direction = m_MoveObj.GetForwardVector();
    }
    const float length = std::hypot(fire_direction.x, fire_direction.y, fire_direction.z);
    if (!physics_vector(fire_origin) || !physics_vector(fire_direction) || !std::isfinite(length) || length < 1e-6f)
        return fail(FireResult::Invalid, "Gideon aim is invalid or degenerate");
    fire_direction = vector_scale(fire_direction, 1 / length);
    int seed;
    if (!s->RandomInt(0, 255, seed))
        return fail(FireResult::Unavailable, "Gideon shot randomness unavailable");
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
        return fail(fire_result, "Gideon weapon shot service failed");
    return true; // Original ignores the inventory return; dry attempts are accepted.
}
