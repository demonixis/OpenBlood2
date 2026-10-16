// Selected original Destructable hit localization and AI_Mgr damage/recoil.
// See AI_HIT_CONTRACTS.md for explicit service failures and safety adaptations.
#include "AI_Mgr.h"
#include "engine/collision.h"
#include <cmath>
#include <limits>
#include <numbers>

int CDestructable::SetProperNode(int node)
{
    if (node < 0 || node > 17)
        return -1;
    if (node <= 1)
        return 1;
    if (node <= 3)
        return 2;
    if (node <= 6)
        return 4;
    if (node <= 9)
        return 7;
    if (node <= 13)
        return 10;
    return 14;
}
bool CDestructable::LocalizeHit(const OpenBlood2::Damage& damage, const CAnim_Sound& table)
{
    using namespace OpenBlood2;
    auto* s = BaseClass::GetServerDE();
    ++hit_queries;
    hit = {};
    hit.from = damage.position;
    auto fail = [&](const char* why)
    {
        hit.error = why;
        ++hit_failures;
        return false;
    };
    DVector dims{};
    DRotation rotation;
    if (!s || !s->IsObjectAlive(m_hObject) || !m_pInventoryMgr || !table.hit_spheres_valid ||
        !physics_vector(damage.direction) || !physics_vector(damage.position) ||
        s->GetModelAnimUserDims(m_hObject, &dims, s->GetModelAnimation(m_hObject)) != DE_OK || !physics_vector(dims) ||
        dims.x < 0 || dims.y < 0 || dims.z < 0 || s->GetObjectRotation(m_hObject, &rotation) != DE_OK)
        return fail("NPC hit owner/model/table/dimensions unavailable");
    const auto d = damage.direction;
    const DVector absolute{std::abs(d.x), std::abs(d.y), std::abs(d.z)};
    float length = 0;
    if (absolute.x > absolute.y && absolute.x > absolute.z)
        length = dims.x / absolute.x;
    else if (absolute.y > absolute.x && absolute.y > absolute.z)
        length = dims.y / absolute.y;
    else if (absolute.z > absolute.x && absolute.z > absolute.y)
        length = dims.z / absolute.z;
    hit.segment = vector_scale(d, length);
    const auto squared = vector_dot(hit.segment, hit.segment);
    if (!physics_vector(hit.segment) || !std::isfinite(squared))
        return fail("NPC hit segment invalid");
    DVector node{};
    DRotation node_rotation;
    DBOOL hidden = DFALSE; // Source retains this value when a name is absent.
    bool have_pose = false;
    for (unsigned i = 0; i < NUM_STD_NODES; ++i)
    {
        if (!std::isfinite(table.m_fHitSpheres[i]) || table.m_fHitSpheres[i] < 0)
            return fail("NPC hit sphere invalid");
        const auto visibility = s->GetModelNodeHideStatus(m_hObject, szNodes[i], &hidden);
        if (visibility != DE_OK && visibility != DE_NOTFOUND)
            return fail("NPC hit node visibility unavailable");
        if (hidden)
            continue;
        // Missing later names preserve the preceding transform, as the source's
        // ignored GetModelNodeTransform result does. Pose errors never do.
        const auto result = s->QueryModelNodeTransform(m_hObject, szNodes[i], &node, &node_rotation);
        if (result == ServerDE::NodeQuery::Unavailable || (!have_pose && result != ServerDE::NodeQuery::Found))
            return fail("NPC hit node pose unavailable");
        have_pose = true;
        hit.missing_nodes += result == ServerDE::NodeQuery::Missing;
        if (squared == 0)
            continue; // Source zero/tied direction yields NaN: no candidate.
        const auto inverse = 1.f / squared;
        const auto t = vector_dot(hit.segment, node) * inverse - vector_dot(hit.segment, damage.position) * inverse;
        if (t < 0 || t > 1)
            continue;
        const auto delta = vector_subtract(node, vector_add(damage.position, vector_scale(hit.segment, t)));
        const auto distance = std::sqrt(vector_dot(delta, delta));
        if (distance < hit.distance && distance <= table.m_fHitSpheres[i])
        {
            hit.raw_node = static_cast<int>(i);
            hit.distance = distance;
        }
    }
    if (hit.raw_node == -1)
    {
        // DAMAGE_TYPE_NORMAL is zero. The original !(type & NORMAL) is always true.
        hit.random_fallback = true;
        if (!s->RandomInt(0, NUM_ALL_NODES - 3, hit.raw_node))
            return fail("NPC hit RNG unavailable");
    }
    hit.node = SetProperNode(hit.raw_node);
    const auto forward = rotate_vector(rotation, {0, 0, 1});
    const float pi = std::numbers::pi_v<float>;
    float incoming = std::atan2(-d.x, -d.z), facing = std::atan2(forward.x, forward.z);
    if (incoming < 0)
        incoming += 2 * pi;
    if (facing < 0)
        facing += 2 * pi;
    const auto angle = facing - incoming; // Intentionally NOT wrapped again.
    hit.side = angle <= pi / 2 || angle >= 3 * pi / 2 ? 0 : 6;
    hit.multiplier = hit.node == 1 ? 1.5f : hit.node == 2 ? 1.f : .5f;
    hit.valid = true;
    return true;
}
bool AI_Mgr::HandleDamage()
{
    ++damage_reactions;
    damage_reaction_error.clear();
    if (m_nState == StateDying || m_bRemoveMe || m_damage.node_hit == -1)
        return true;
    const int node = m_damage.node_hit;
    if ((m_dwFlags & 0x4) && !m_nInjuredLeg && (node == 10 || node == 14) &&
        m_damage.m_fHitPoints < m_damage.m_fMaxHitPoints * .5f)
        m_nInjuredLeg = node;
    if (m_damage.m_fHitPoints <= 0)
    {
        if (m_bCabal)
            m_InventoryMgr.DropCurrentWeapon(); // Source ignores refusal (no held weapon/busy/melee).
        auto* s = GetServerDE();
        OpenBlood2::MovementState support;
        if (!s->GetMovementState(m_hObject, support))
        {
            damage_reaction_error = "NPC death support query unavailable";
            return false;
        }
        if (support.ground_object && !(s->GetObjectFlags(support.ground_object) & FLAG_SOLID))
            m_dwFlags |= 0x40u; // FLAG_ALWAYSGIB; primary world has a real polygon, not a fabricated HOBJECT.
        const auto type = m_damage.reaction_damage_type;
        if (type == OpenBlood2::DamageType::Fire && !(m_dwFlags & 0x40u))
        {
            damage_reaction_error = "NPC fire death requires original burning services";
            return false;
        }
        if (!(m_dwFlags & 0x800u) &&
            (type == OpenBlood2::DamageType::Explode || (m_dwFlags & 0x40u) || m_damage.m_fLastDamagePercent >= .9f))
        {
            damage_reaction_error = "NPC gib death requires original gib services";
            return false;
        }
        if (!s->RandomInt(0, 99, kill_voice_roll))
        {
            damage_reaction_error = "NPC kill voice draw unavailable";
            return false;
        }
        const auto attacker = m_damage.GetLastDamager();
        if (kill_voice_roll < 6 && attacker && s->IsKindOf(s->GetObjectClass(attacker), s->GetClass("CPlayerObj")))
            ++kill_voice_requests; // Optional VME_KILL request; voice-event playback remains unavailable.
    }
    if (m_nState == StateScript && !(m_dwScriptFlags & (1u << 1)))
        return true;
    if ((m_dwFlags & 0x20) || (m_damage.m_fLastDamageAmount >= m_damage.m_fHitPoints * .25f &&
                               m_nState != StateTeleport && m_nState != StateDodge))
        return SetNewState(StateRecoil);
    if (m_nState != StateAttackClose && m_nState != StateAttackFar)
        return ComputeState();
    return true;
}
bool AI_Mgr::MC_Recoil()
{
    auto* s = GetServerDE();
    std::string name;
    if (!s || !s->IsObjectAlive(m_hObject) || !m_pAnim_Sound ||
        !s->GetModelAnimationName(m_hObject, s->GetModelAnimation(m_hObject), name))
        return StateFailure("AI recoil owner/model/table unavailable");
    if (!m_bAnimating || m_nCurMetacmd != 19)
    {
        if (m_damage.side_hit != 0 && m_damage.side_hit != 6)
            return StateFailure("AI recoil side invalid");
        recoil_node = m_damage.node_hit;
        if (m_damage.reaction_damage_type == OpenBlood2::DamageType::Fire &&
            !s->RandomInt(0, NUM_ALL_NODES - 3, recoil_node))
            return StateFailure("AI fire recoil RNG unavailable");
        recoil_slot = -1;
        constexpr int nodes[]{1, 2, 4, 7, 10, 14};
        for (int i = 0; i < 6; ++i)
            if (recoil_node == nodes[i])
                recoil_slot = i + m_damage.side_hit;
        if (recoil_slot >= 0 && !SetAnimation(m_pAnim_Sound->m_nAnim_RECOIL[recoil_slot]))
            return false;
        // Raw fire draws are intentionally not mapped through SetProperNode.
        // Unmatched draws keep the current animation and still make it nonlooping.
        s->SetModelLooping(m_hObject, DFALSE);
        m_bAnimating = true;
        m_nCurMetacmd = 19;
        ++recoil_starts;
    }
    else if (s->GetModelPlaybackState(m_hObject) & MS_PLAYDONE)
    {
        if (Metacmd == std::numeric_limits<int>::max())
            return StateFailure("AI Metacmd overflow");
        m_bAnimating = false;
        ++Metacmd;
        ++animation_finishes;
    }
    return true;
}
bool AI_Mgr::AI_STATE_Recoil()
{
    if (Metacmd == 1)
        return MC_Recoil();
    if (Metacmd == 2)
        return ComputeState();
    return true;
}
