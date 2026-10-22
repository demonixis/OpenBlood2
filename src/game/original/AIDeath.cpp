// Original AI_Mgr MC_Dead/AI_STATE_Dying/CreateCorpse, ordinary death only.
#include "AI_Mgr.h"
#include "Corpse.h"
#include "Gib.h"
#include "engine/collision.h"
#include <algorithm>
#include <cmath>
#include <cstring>

bool AI_Mgr::MC_Dead()
{
    auto* s = GetServerDE();
    death_error.clear();
    if (!s || !s->IsObjectAlive(m_hObject) || !m_pAnim_Sound)
        return StateFailure("Death owner/animation table unavailable");
    if (!m_bAnimating || m_nCurMetacmd != 20)
    {
        if (m_hCurSound)
        {
            s->KillSound(m_hCurSound);
            m_hCurSound = 0;
        }
        death_slot = -1;
        if (!(m_dwFlags & 0x400u))
        {
            if (m_damage.side_hit != 0 && m_damage.side_hit != 6)
                return StateFailure("Death side invalid");
            constexpr int nodes[]{1, 2, 4, 7, 10, 14};
            for (int i = 0; i < 6; ++i)
                if (nodes[i] == m_damage.node_hit)
                    death_slot = i + m_damage.side_hit;
        }
        else
        {
            int roll;
            if (!s->RandomInt(0, 99, roll))
                return StateFailure("Nightmare death RNG unavailable");
            death_slot = roll < 50 ? 0 : 1;
        }
        if (death_slot >= 0)
            m_nCorpseType = m_pAnim_Sound->m_nAnim_DEATH[death_slot];
        m_bAnimating = true;
        m_nCurMetacmd = 20;
        ++death_starts; // Deliberately no NPC animation playback or Metacmd increment.
        return true;
    }
    // Original control flow: CreateCorpse is void and the actor is hidden and removed
    // regardless. A failed transfer stays recorded (death_error, spawn report, counter)
    // instead of suspending a solid, visible actor that can block the player.
    if (!CreateCorpse())
        ++corpse_failures;
    s->SetObjectFlags(m_hObject, s->GetObjectFlags(m_hObject) & ~FLAG_VISIBLE);
    m_bRemoveMe = true;
    return true;
}
bool AI_Mgr::AI_STATE_Dying()
{
    if (Metacmd == 1)
        return MC_Dead();
    if (Metacmd == 2)
        m_nState = StateInactive; // Direct source assignment, no transition side effects.
    return true;
}
bool AI_Mgr::CreateCorpse()
{
    using namespace OpenBlood2;
    auto* s = GetServerDE();
    SpawnReport report;
    corpse_placement = {};
    report.source = "AI_Mgr::CreateCorpse";
    report.request = "CCorpse";
    auto fail = [&](const char* why)
    {
        death_error = why;
        report.outcome = why;
        if (s)
            s->RecordSpawn(report);
        return StateFailure(why);
    };
    if (!s || !s->IsObjectAlive(m_hObject) || !m_pAnim_Sound || !m_MoveObj.initialized || !m_MoveObj.sampled ||
        !physics_vector(m_vDims))
        return fail("Corpse owner/cache/table unavailable");
    std::string model, skin, clip_name;
    auto type = s->GetClass("CCorpse");
    if (!type || !s->GetModelFilenames(m_hObject, model, skin))
        return fail("Corpse class/model filenames unavailable");
    if (m_damage.reaction_damage_type == DamageType::Fire)
    {
        skin = "skins\\enemies\\burned.dtx";
        if (!s->RandomInt(0, 11, death_slot))
            return fail("Burned corpse RNG unavailable");
        m_nCorpseType = m_pAnim_Sound->m_nAnim_DEATH[death_slot];
    }
    if (m_nCorpseType < m_pAnim_Sound->m_nAnim_DEATH[0])
    {
        int slot;
        if (!s->RandomInt(0, 11, slot))
            return fail("Corpse fallback RNG unavailable");
        m_nCorpseType = m_pAnim_Sound->m_nAnim_DEATH[slot];
        death_slot = slot;
    }
    DVector dims;
    if (!s->GetModelAnimationName(m_hObject, m_nCorpseType, clip_name) ||
        s->GetModelAnimUserDims(m_hObject, &dims, m_nCorpseType) != DE_OK || !physics_vector(dims) || dims.x <= 0 ||
        dims.y <= 0 || dims.z <= 0)
        return fail("Corpse animation/dimensions unavailable");
    ObjectCreateStruct state;
    if (model.size() >= sizeof(state.m_Filename) || skin.size() >= sizeof(state.m_SkinName))
        return fail("Corpse filenames exceed original buffer");
    std::memcpy(state.m_Filename, model.c_str(), model.size() + 1);
    std::memcpy(state.m_SkinName, skin.c_str(), skin.size() + 1);
    std::strcpy(state.m_Name, "AI corpse");
    state.m_Pos = m_MoveObj.GetPos(); // Original cached transform; not a fresh pose sample.
    state.m_Rotation = m_MoveObj.GetRotation();
    auto* corpse = dynamic_cast<CCorpse*>(s->CreateObject(type, &state));
    if (!corpse)
        return fail("Corpse creation failed");
    const auto h = corpse->m_hObject;
    report.child = h;
    auto position = state.m_Pos;
    position.y -= (m_vDims.y - dims.y) - .001f;
    const bool configured = s->IsObjectAlive(m_hObject) && s->IsObjectAlive(h) &&
                            corpse->Setup(m_pAnim_Sound->GetSoundRoot(), m_bCabal, fire_source);
    if (configured)
        fire_source = nullptr; // Ownership transfers once, including failure rollback.
    if (configured)
    {
        s->SetModelAnimation(h, m_nCorpseType);
        s->SetModelLooping(h, false);
    }
    // Keep the original cached anchor, raw death dimensions and requested foot
    // position. The replacement engine fits this new nonsolid body locally without
    // changing dimensions, crossing a wall or accepting a penetrating source.
    bool placed = configured && s->GetModelAnimation(h) == m_nCorpseType && !s->GetModelLooping(h) &&
                  s->ScaleObject(h, m_vScale) && s->SetObjectDims(h, &dims) == DE_OK &&
                  s->PlaceNonSolidModel(h, position, state.m_Pos, m_vDims, corpse_placement);
    if (!placed && configured && corpse_placement.result == BodyPlacementResult::Blocked)
    {
        // Replacement fallback: the raw death box cannot fit (corridor narrower than the
        // death clip, or beyond the bounded search). Clamp the nonsolid corpse's physical
        // box to the clear living box at the same feet; it is contained in that box, so the
        // fit is guaranteed whenever the anchor was clear. The model and clip are unchanged.
        corpse_fallback = corpse_placement;
        DVector contained{std::min(dims.x, m_vDims.x), std::min(dims.y, m_vDims.y), std::min(dims.z, m_vDims.z)};
        auto fallback = state.m_Pos;
        fallback.y -= (m_vDims.y - contained.y) - .001f;
        placed = s->SetObjectDims(h, &contained) == DE_OK &&
                 s->PlaceNonSolidModel(h, fallback, state.m_Pos, m_vDims, corpse_placement);
        contained_corpses += placed;
    }
    if (!placed)
    {
        report.position =
            corpse_placement.result == BodyPlacementResult::Unqueried ? position : corpse_placement.candidate;
        s->RemoveObject(h); // Deferred rollback; source remains inspectable.
        return fail("Corpse setup failed; incomplete child removed");
    }
    corpse->placement = corpse_placement;
    for (int node : {1, 4, 7, 10, 14})
    {
        DBOOL hidden = false;
        if (s->GetModelNodeHideStatus(m_hObject, szNodes[node], &hidden) == DE_OK && hidden)
            HideLimb(h, node, death_error);
    }
    last_corpse = h;
    ++corpses_created;
    report.position = corpse_placement.candidate;
    report.outcome = "Original ordinary corpse initialized; independent animation/lifetime";
    s->RecordSpawn(report);
    return true;
}

bool AI_Mgr::StartBurnDeath()
{
    auto* s = GetServerDE();
    if (fire_source && s->IsObjectAlive(fire_source))
        return true;
    ObjectCreateStruct state;
    state.m_ObjectType = OT_NORMAL;
    state.m_Flags = FLAG_FORCECLIENTUPDATE;
    if (s->QueryModelNodeTransform(m_hObject, "torso", &state.m_Pos, &state.m_Rotation) != ServerDE::NodeQuery::Found)
    {
        damage_reaction_error = "Burning death torso unavailable";
        return false;
    }
    std::strcpy(state.m_Name, "Corpse fire source");
    auto* object = s->CreateObject(s->GetClass("BaseClass"), &state);
    if (!object)
    {
        damage_reaction_error = "Burning death source creation failed";
        return false;
    }
    fire_source = object->m_hObject;
    return true;
}
bool AI_Mgr::CreateDeathGibs()
{
    gib_error.clear();
    if (!std::isfinite(m_damage.m_fMass) || m_damage.m_fMass < 0 || m_damage.m_fMass > 2048)
    {
        gib_error = "AI gib mass exceeds bounded fragment request";
        return false;
    }
    // Source ((int)GetMass()) >> 5 gibs, launched from the last damage.
    return CreateGibs(m_damage.last_damage.direction, static_cast<unsigned>(m_damage.m_fMass) / 32,
                      m_damage.m_fLastDamageAmount);
}
bool AI_Mgr::CreateGibs(DVector direction, unsigned count, float damage)
{
    using namespace OpenBlood2;
    auto* s = GetServerDE();
    gib_error.clear();
    if (!physics_vector(direction) || !std::isfinite(damage) || count > 64)
    {
        gib_error = "AI gib direction, damage or count is outside the bounded request";
        return false;
    }
    direction.y += 2;
    const float length = std::sqrt(vector_dot(direction, direction));
    // CreateGibs negates, then SetupClientGibFX negates its IN/OUT argument again.
    direction = length > 0 ? vector_scale(direction, (50 + 2 * damage) / length) : DVector{};
    const float scale = m_fAIMass > 0 && m_fAIMass < 250 ? m_fAIMass / 250 : 1;
    const auto event = s->PublishFleshGibs(m_hObject, m_MoveObj.GetPos(), direction, m_vDims, scale, count, gib_error);
    gib_events += event != 0;
    if (m_bCabal)
    {
        int roll;
        if (!s->RandomInt(0, 100, roll))
            return false; // Source's unused initial draw.
        constexpr int nodes[]{1, 4, 7, 14, 10}, thresholds[]{80, 70, 50, 70, 50};
        for (int i = 0; i < 5; ++i)
        {
            if (!s->RandomInt(0, 100, roll))
                return false;
            if (roll > thresholds[i])
            {
                std::string error;
                if (CreateLimb(m_hObject, nodes[i], direction, error))
                    ++limbs_created;
                else if (gib_error.empty())
                    gib_error = error;
            }
        }
    }
    return event != 0;
}
