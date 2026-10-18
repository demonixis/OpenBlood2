#include "CharacterProfiles.h"
// Selected original PlayerObj HandleDeath/DoDeath/CreateCorpse/CreateGibs.
// Single-player default Chosen profiles; source selections and limits: player_death_contracts.md.
#include "Corpse.h"
#include "Gib.h"
#include "PlayerObj.h"
#include "engine/collision.h"
#include <algorithm>
#include <cmath>
#include <cstring>

using namespace OpenBlood2;

void CPlayerObj::HandleDeath()
{
    auto* s = GetServerDE();
    if (m_bDead || !m_damage.IsDead())
        return;
    death_error.clear();
    if (!s || !s->IsObjectAlive(m_hObject) || !m_hClient || s->GetClientUserData(m_hClient) != m_hObject)
    {
        death_error = "Player death requires a checked local client";
        return;
    }
    // Original null WhoKilledMeLast guard. Identity comparison needs no live
    // attacker pointer: the attacker may have been removed since the lethal hit.
    if (!m_damage.last_damage.attacker)
    {
        death_error = "Original HandleDeath waits for a non-null lethal attacker";
        return;
    }
    if (s->GetVarValueFloat(s->GetGameConVar("GAME_GAMETYPE")) != 0 || !OpenBlood2::character_profile(m_nCharacter) ||
        std::any_of(m_bPowerupActivated.begin(), m_bPowerupActivated.end(), [](bool active) { return active; }))
    {
        death_error = "Multiplayer and active timed-power death are unavailable";
        return;
    }
    // DoDeath: these supported players have no active power, zoom, slow-death,
    // parasitic attachment or flag to reset. Do not fabricate those services.
    m_bDead = DTRUE;
    ++deaths;
    m_InventoryMgr.RemoveDeathItems();
    death_voice_event = m_damage.last_damage.attacker == m_hObject ? 8 : 4; // SUICIDE / DEATH.
    PlayVoiceGroupEventOnClient(death_voice_event, death_voice_event == 8);
    if (s->SendToClient(m_hClient, PlayerDead{}) == DE_OK)
        ++death_messages;
    else
        death_error = "Original SMSG_DEAD delivery failed";
    if (m_hCurSound)
    {
        s->KillSound(m_hCurSound);
        m_hCurSound = 0;
    }
    s->SetObjectFlags(m_hObject, s->GetObjectFlags(m_hObject) & ~FLAG_VISIBLE);
    move_state = MoveState::Dead;
    m_InventoryMgr.UpdateHandModels();

    const auto type = m_damage.reaction_damage_type;
    // Preserve source bit test and the literal 50.0 threshold. These differ
    // from NPC equality/.9 and must not be 'fixed' to guessed game behavior.
    if ((static_cast<DBYTE>(type) & static_cast<DBYTE>(DamageType::Explode)) || m_damage.m_fLastDamagePercent >= 50.0f)
        CreateDeathGibs();
    else if (type != DamageType::FreeFall)
    {
        if (HideLimb(m_hObject, m_damage.node_hit, death_effect_error))
            death_limbs +=
                CreateLimb(m_hObject, m_damage.node_hit, m_damage.last_damage.direction, death_effect_error) != nullptr;
        // The source player's separate m_nNodeHit/m_nSideHit are initialized to
        // zero and never updated by its damage handler. Fresh Caleb uses slot1.
        death_slot = 1;
        CreateDeathCorpse();
    }
    // Original death commits even if cosmetic corpse/gib creation fails. Keep
    // the player/client alive and record failed presentation; never retry/spawn twice.
}

bool CPlayerObj::CreateDeathCorpse()
{
    auto* s = GetServerDE();
    SpawnReport report;
    report.source = "CPlayerObj::CreateCorpse";
    report.request = "CCorpse";
    death_placement = {};
    auto fail = [&](const char* why)
    {
        death_effect_error = report.outcome = why;
        s->RecordSpawn(report);
        return false;
    };
    DVector source_dims{}, dims{};
    ObjectCreateStruct state;
    std::string model, skin, clip_name;
    const auto clip = m_Anim_Sound.m_nAnim_DEATH[1];
    if (!s->IsObjectAlive(m_hObject) || !s->GetModelFilenames(m_hObject, model, skin) ||
        model.size() >= sizeof(state.m_Filename) || skin.size() >= sizeof(state.m_SkinName) ||
        s->GetObjectPos(m_hObject, &state.m_Pos) != DE_OK ||
        s->GetObjectRotation(m_hObject, &state.m_Rotation) != DE_OK ||
        s->GetObjectDims(m_hObject, &source_dims) != DE_OK || !s->GetModelAnimationName(m_hObject, clip, clip_name) ||
        s->GetModelAnimUserDims(m_hObject, &dims, clip) != DE_OK)
        return fail("Player corpse model/pose/animation unavailable");
    std::strcpy(state.m_Filename, model.c_str());
    std::strcpy(state.m_SkinName, skin.c_str());
    std::strcpy(state.m_Name, "Player corpse");
    auto* corpse = dynamic_cast<CCorpse*>(s->CreateObject(s->GetClass("CCorpse"), &state));
    if (!corpse)
        return fail("Player corpse creation failed");
    const auto h = corpse->m_hObject;
    report.child = h;
    auto position = state.m_Pos;
    position.y -= (source_dims.y - dims.y) - .001f;
    s->SetModelAnimation(h, clip);
    s->SetModelLooping(h, false);
    // Source player does not pass UserData or Setup: empty sound directory,
    // limb loss enabled, no burn source. Our explicit setup records those defaults.
    if (!s->IsObjectAlive(m_hObject) || !corpse->Setup("", true) || s->GetModelAnimation(h) != clip ||
        s->GetModelLooping(h) || !s->ScaleObject(h, {1, 1, 1}) || s->SetObjectDims(h, &dims) != DE_OK ||
        !s->PlaceNonSolidModel(h, position, state.m_Pos, source_dims, death_placement))
    {
        s->RemoveObject(h);
        return fail("Player corpse setup failed; incomplete child removed");
    }
    corpse->placement = death_placement;
    for (int node : {1, 4, 7, 10, 14})
    {
        DBOOL hidden = false;
        if (s->GetModelNodeHideStatus(m_hObject, szNodes[node], &hidden) == DE_OK && hidden)
            HideLimb(h, node, death_effect_error);
    }
    last_death_corpse = h;
    ++death_corpses;
    report.position = death_placement.candidate;
    report.outcome = "Original player corpse initialized with independent lifetime";
    s->RecordSpawn(report);
    return true;
}

bool CPlayerObj::CreateDeathGibs()
{
    auto* s = GetServerDE();
    auto direction = vector_scale(m_damage.last_damage.direction, -1);
    direction.y -= 1;
    const float length = std::sqrt(vector_dot(direction, direction));
    // SetupClientGibFX mutates the same vector with a second negation before
    // body parts use it. Player speed is 50+damage, not the AI's 50+2*damage.
    direction = length > 0 ? vector_scale(direction, -(50 + m_damage.m_fLastDamageAmount) / length) : DVector{};
    DVector position{}, dims{};
    if (!std::isfinite(m_damage.m_fMass) || m_damage.m_fMass < 0 || m_damage.m_fMass > 2048 ||
        !physics_vector(direction) || s->GetObjectPos(m_hObject, &position) != DE_OK ||
        s->GetObjectDims(m_hObject, &dims) != DE_OK)
    {
        death_effect_error = "Player gib mass/direction/pose invalid";
        return false;
    }
    const auto event = s->PublishFleshGibs(m_hObject, position, direction, dims, 1,
                                           static_cast<unsigned>(m_damage.m_fMass) / 32, death_effect_error);
    death_gibs += event != 0;
    int roll;
    if (!s->RandomInt(0, 100, roll))
        return false; // Source unused first draw.
    constexpr int nodes[]{1, 4, 7, 14, 10}, thresholds[]{80, 70, 50, 70, 50};
    for (int i = 0; i < 5; ++i)
    {
        if (!s->RandomInt(0, 100, roll))
            return false;
        if (roll > thresholds[i])
        {
            std::string error;
            death_limbs += CreateLimb(m_hObject, nodes[i], direction, error) != nullptr;
            if (!error.empty() && death_effect_error.empty())
                death_effect_error = error;
        }
    }
    return event != 0;
}
