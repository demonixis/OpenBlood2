// Original AI_Mgr MC_Dead/AI_STATE_Dying/CreateCorpse, ordinary death only.
#include "AI_Mgr.h"
#include "Corpse.h"
#include "engine/collision.h"
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
    // Unlike the unchecked source, a failed transfer retains a visible actor.
    if (!CreateCorpse())
        return false;
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
        !physics_vector(m_vDims) || m_damage.reaction_damage_type == DamageType::Fire)
        return fail("Corpse owner/cache/table unavailable or fire death unsupported");
    std::string model, skin, clip_name;
    auto type = s->GetClass("CCorpse");
    if (!type || !s->GetModelFilenames(m_hObject, model, skin))
        return fail("Corpse class/model filenames unavailable");
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
    const bool configured =
        s->IsObjectAlive(m_hObject) && s->IsObjectAlive(h) && corpse->Setup(m_pAnim_Sound->GetSoundRoot(), m_bCabal);
    if (configured)
    {
        s->SetModelAnimation(h, m_nCorpseType);
        s->SetModelLooping(h, false);
    }
    // A stationary move checks the corpse's actual primary-world body before
    // accepting transfer. Raw death dimensions may overlap a wall/floor even
    // when the live actor fitted. Preserve explicit failure and deferred rollback;
    // never leave a penetrating child to abort an unrelated player's next tick.
    if (!configured || s->GetModelAnimation(h) != m_nCorpseType || s->GetModelLooping(h) ||
        !s->ScaleObject(h, m_vScale) || s->SetObjectDims(h, &dims) != DE_OK ||
        s->TeleportObject(h, &position) != DE_OK || s->MoveObject(h, &position) != DE_OK)
    {
        s->RemoveObject(h); // Deferred rollback; source remains inspectable.
        return fail("Corpse setup failed; incomplete child removed");
    }
    // All supported NPC nodes are visible; per-limb hiding remains unavailable.
    last_corpse = h;
    ++corpses_created;
    report.position = position;
    report.outcome = "Original ordinary corpse initialized; independent animation/lifetime";
    s->RecordSpawn(report);
    return true;
}
