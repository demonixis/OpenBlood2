#include "CharacterProfiles.h"
// Selected original PlayerObj HandleDeath/DoDeath/CreateCorpse/CreateGibs.
// Single-player default Chosen profiles; source selections and limits: player_death_contracts.md.
#include "Corpse.h"
#include "Gib.h"
#include "PlayerObj.h"
#include "engine/collision.h"
#include "game/game_settings.h"
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
    DropFlag(); // Source drops before checking the lethal attacker.
    // Original null WhoKilledMeLast guard. Identity comparison needs no live
    // attacker pointer: the attacker may have been removed since the lethal hit.
    if (!m_damage.last_damage.attacker)
    {
        death_error = "Original HandleDeath waits for a non-null lethal attacker";
        return;
    }
    bool unported_power = false;
    for (int i = 0; i < PU_MAXTYPES; ++i)
        unported_power |= m_bPowerupActivated[i] && i != PU_INVISIBLE && i != PU_INVULNERABLE && i != PU_TRIPLEDAMAGE;
    if (!OpenBlood2::character_profile(m_nCharacter) || unported_power)
    {
        death_error = "Player profile or active timed-power death is unavailable";
        return;
    }
    const auto killer = m_damage.last_damage.attacker;
    if (s->IsMultiplayerGame() && m_fDeathTimer == 0 && killer != m_hObject)
    {
        int chance = 100;
        if (m_damage.m_fDeathHitPoints < -5 && !s->RandomInt(0, 99, chance))
        {
            death_error = "Slow-death random service unavailable";
            return;
        }
        if (chance < 10) // Source code says 10%, despite its 25% comment.
        {
            m_fDeathTimer = 15;
            m_fSlowDeathSafeTimer = .5f;
            m_fSlowDeathStayTimer = 5;
            m_bInSlowDeath = true;
            humiliation_slot = -1;
            ++slow_deaths;
            SendWonkyVision(m_fDeathTimer, true);
            auto* attacker = dynamic_cast<CPlayerObj*>(s->HandleToObject(killer));
            if (attacker && attacker->m_hClient && s->GetClientUserData(attacker->m_hClient) == killer)
            {
                int index = 0;
                if (!s->RandomInt(0, 65535, index) ||
                    s->SendToClient(attacker->m_hClient, FinishHimVoice{std::uint16_t(index), attacker->m_nCharacter,
                                                                        m_nCharacter}) != DE_OK)
                    death_error = "Finish-him announcement delivery failed";
            }
        }
        else
            m_fDeathTimer = -1;
        return; // Source confirms ordinary multiplayer death on the next update.
    }
    if (m_bInSlowDeath)
    {
        m_fDeathTimer -= s->GetFrameTime();
        if (m_fDeathTimer <= 0)
            m_bInSlowDeath = false;
        return; // Expiry also waits until the following update before DoDeath.
    }
    DoDeath();
}
void CPlayerObj::DoDeath(bool humiliation)
{
    auto* s = GetServerDE();
    if (!s || m_bDead || !s->IsObjectAlive(m_hObject))
        return;
    m_bDead = DTRUE;
    m_bInSlowDeath = false;
    m_fSlowDeathSafeTimer = m_fSlowDeathStayTimer = 0;
    humiliation_death = humiliation;

    ResetPowerups();
    ++deaths;
    m_InventoryMgr.RemoveDeathItems();
    death_voice_event = m_damage.last_damage.attacker == m_hObject ? 8 : 4; // SUICIDE / DEATH.
    PlayVoiceGroupEventOnClient(death_voice_event, death_voice_event == 8);
    if (s->SendToClient(m_hClient, PlayerDead{}) == DE_OK)
        ++death_messages;
    else
        death_error = "Original SMSG_DEAD delivery failed";
    if (m_bZoomView)
        SetZoomMode(false); // Original weapon-zoom reset after SMSG_DEAD.
    if (s->IsMultiplayerGame())
        UpdateFragStatus();
    if (m_hCurSound)
    {
        s->KillSound(m_hCurSound);
        m_hCurSound = 0;
    }
    StopWeaponSound(); // Original DoDeath releases the per-player weapon loop.
    if (!humiliation)
        s->SetObjectFlags(m_hObject, s->GetObjectFlags(m_hObject) & ~FLAG_VISIBLE);
    move_state = MoveState::Dead;
    m_InventoryMgr.UpdateHandModels();

    if (humiliation)
    {
        humiliation_slot = 3;
        humiliation_animation_pending = true;
        UpdateHumiliationAnimation(); // Contact callbacks defer a changed-size request.
        return;
    }
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

bool CPlayerObj::SendWonkyVision(float seconds, bool no_move)
{
    if (GetServerDE()->SendToClient(m_hClient, WonkyVision{seconds, no_move}) == DE_OK)
        return true;
    death_error = "Wonky-vision delivery failed";
    return false;
}
void CPlayerObj::UpdateFragStatus()
{
    auto* s = GetServerDE();
    auto* killer = dynamic_cast<CPlayerObj*>(s->HandleToObject(m_damage.last_damage.attacker));
    if (!killer || !killer->m_hClient || s->GetClientUserData(killer->m_hClient) != killer->m_hObject)
        return; // Original FindClient: removed killers receive neither a frag nor a draw.
    if (const auto* name = s->GetStringData(killer->m_hstrPlayerName))
    {
        who_killed_me_last = name;
        int delta = killer == this ? -1 : 1;
        if (killer != this && team_game_type(s->net_game_type) && killer->GetTeamID() == GetTeamID())
            delta = s->friendly_fire && s->negative_team_frags ? -1 : 0;
        if (killer->m_Frags + delta < -1000000 || killer->m_Frags + delta > 1000000)
        {
            death_error = "Player frag counter exhausted";
            return;
        }
        if (team_game_type(s->net_game_type) && !s->AddClientTeamFrags(killer->m_hClient, delta))
        {
            death_error = "Team frag counter or membership unavailable";
            return;
        }
        killer->m_Frags += delta;
        if (killer != this)
            if (const auto* victim = s->GetStringData(m_hstrPlayerName))
                killer->who_i_killed_last = victim;
    }
    int index = 0;
    if (!s->RandomInt(0, 65535, index) ||
        s->SendPlayerFragged({m_hObject, killer->m_hObject, killer->m_Frags, std::uint16_t(index)}) != DE_OK)
        death_error = "Player frag broadcast failed";
    else
        ++frag_updates;
}
bool CPlayerObj::UpdateHumiliationAnimation()
{
    auto* s = GetServerDE();
    if (!m_bInSlowDeath && !humiliation_animation_pending)
        return true;
    if (m_bInSlowDeath && move_state == MoveState::Humiliation && !humiliation_animation_pending)
        return true;
    if (humiliation_slot < 0)
    {
        if (!s->RandomInt(0, 2, humiliation_slot))
            return false;
        humiliation_animation_pending = true;
    }
    auto clip = m_Anim_Sound.m_nAnim_HUMILIATION[humiliation_slot];
    std::string name;
    if (!s->GetModelAnimationName(m_hObject, clip, name))
        clip = s->GetModelAnimation(m_hObject); // Same original invalid-ordinal retention as locomotion.
    DVector dimensions;
    if (!s->GetModelAnimationName(m_hObject, clip, name) ||
        s->GetModelAnimUserDims(m_hObject, &dimensions, clip) != DE_OK)
    {
        death_effect_error = "Humiliation animation/dimensions unavailable";
        return false;
    }
    const auto resize = s->ResizeObject(m_hObject, dimensions);
    if (resize == ResizeResult::Blocked || resize == ResizeResult::InTransaction)
    {
        humiliation_animation_pending = true;
        death_effect_error = "Humiliation animation waits for checked dimensions";
        return true;
    }
    if (resize != ResizeResult::Resized)
        return false;
    s->SetModelAnimation(m_hObject, clip);
    if (s->ResetModelAnimation(m_hObject) != DE_OK)
        return false;
    s->SetModelLooping(m_hObject, true);
    if (!m_bDead)
        move_state = MoveState::Humiliation;
    locomotion_clip = name;
    ++animation_changes;
    humiliation_animation_pending = false;
    death_effect_error.clear();
    return true;
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
