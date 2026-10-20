// Original Save/Load bodies of the bosses and special enemies (save/load lot 12): Gideon.cpp:811-831,
// EnhancedGideon.cpp:835-855, UndeadGideon.cpp:524-532, AncientOne.cpp:418-434,
// AncientOneTentacle.cpp:569-589, Naga.cpp:514-541, ZealotAI.cpp:863-890, boneleech.cpp:133-150 and
// 495-507 (thief.cpp and TheHandAI.cpp are the same), drudgepriest.cpp:466-474. Each block is written
// before AI_Mgr's; every field is checked before a member changes. See save_load_contracts.md.
#include "AncientOne.h"
#include "BoneLeech.h"
#include "Gideon.h"
#include "LaterEnemies.h"
#include "Naga.h"
#include "PlayerObj.h"
#include "SaveFields.h"
#include "UndeadGideon.h"
#include "ZealotAI.h"
#include "engine/collision.h"
#include <bit>
#include <cmath>

using namespace OpenBlood2;
namespace F = OpenBlood2::save_fields;
namespace
{
    bool clock(float value)
    {
        return std::isfinite(value);
    }
} // namespace

void Gideon::SaveSubclass(HMESSAGEWRITE h)
{
    auto* s = GetServerDE();
    for (float value : {m_fShieldDuration, m_fLastShield, m_fFullHealth, m_fLastPanic}) // Raw absolute clocks.
        s->WriteToMessageFloat(h, value);
}
void Gideon::LoadSubclass(HMESSAGEREAD h)
{
    auto* s = GetServerDE();
    const float duration = s->ReadFromMessageFloat(h), shield = s->ReadFromMessageFloat(h);
    const float full = s->ReadFromMessageFloat(h), panic = s->ReadFromMessageFloat(h);
    if (!F::finite(duration) || duration < 0 || !clock(shield) || !F::finite(full) || full < 0 || !clock(panic))
    {
        s->RejectPayload(h, "Gideon Load: shield, health or panic value rejected");
        return;
    }
    // m_szFireNode ("rr_gun") and m_bCreateHealth are not saved: the constructor and initial update apply.
    m_fShieldDuration = duration;
    m_fLastShield = shield;
    m_fFullHealth = full;
    m_fLastPanic = panic;
}

void UndeadGideon::SaveSubclass(HMESSAGEWRITE h)
{
    GetServerDE()->WriteToMessageFloat(h, m_fFullHealth);
}
void UndeadGideon::LoadSubclass(HMESSAGEREAD h)
{
    auto* s = GetServerDE();
    const float full = s->ReadFromMessageFloat(h);
    if (!F::finite(full) || full < 0)
    {
        s->RejectPayload(h, "UndeadGideon Load: full health rejected");
        return;
    }
    m_fFullHealth = full; // m_bCreateHealth stays the constructor's TRUE.
}

void AncientOne::SaveSubclass(HMESSAGEWRITE h)
{
    auto* s = GetServerDE();
    s->WriteToMessageFloat(h, 0); // m_fLastFire: the constructor's 0, never assigned by the original.
    s->WriteToMessageFloat(h, m_fFullHealth);
}
void AncientOne::LoadSubclass(HMESSAGEREAD h)
{
    auto* s = GetServerDE();
    const float last_fire = s->ReadFromMessageFloat(h), full = s->ReadFromMessageFloat(h);
    if (last_fire != 0 || !F::finite(full) || full < 0)
    {
        s->RejectPayload(h, "AncientOne Load: fire clock or full health rejected");
        return;
    }
    m_fFullHealth = full;
}

void AncientOneTentacle::SaveSubclass(HMESSAGEWRITE h)
{
    auto* s = GetServerDE();
    s->WriteToMessageFloat(h, m_fTimeOfDeath); // Raw absolute; the original leaves it uninitialized until death.
    s->WriteToMessageFloat(h, 0);              // m_fLastIdle: the constructor's 0 (its only writer is commented out).
    s->WriteToMessageFloat(h, m_fScaleBy);
    s->WriteToMessageByte(h, m_bHiding);
}
void AncientOneTentacle::LoadSubclass(HMESSAGEREAD h)
{
    auto* s = GetServerDE();
    const float death = s->ReadFromMessageFloat(h), idle = s->ReadFromMessageFloat(h);
    const float scale = s->ReadFromMessageFloat(h);
    bool hiding = false;
    const bool hidden = F::read_bool(h, hiding);
    if (!hidden || !clock(death) || idle != 0 || !F::finite(scale) || scale <= 0)
    {
        s->RejectPayload(h, "AncientOneTentacle Load: death clock, idle clock or scale rejected");
        return;
    }
    m_fTimeOfDeath = death;
    m_fScaleBy = scale;
    m_bHiding = hiding;
}

void DrudgePriest::SaveSubclass(HMESSAGEWRITE h)
{
    GetServerDE()->WriteToMessageFloat(h, last_spawn); // Raw absolute.
}
void DrudgePriest::LoadSubclass(HMESSAGEREAD h)
{
    auto* s = GetServerDE();
    const float spawn = s->ReadFromMessageFloat(h);
    if (!clock(spawn))
    {
        s->RejectPayload(h, "DrudgePriest Load: spawn clock rejected");
        return;
    }
    last_spawn = spawn; // The ambient loop restarted in initial update, as the original's does.
}

void ZealotAI::SaveSubclass(HMESSAGEWRITE h)
{
    auto* s = GetServerDE();
    s->WriteToMessageByte(h, DBYTE(floating | (start_staff << 1) | (npc_divine << 2)));
    s->WriteToMessageFloat(h, 0); // m_fMaxHeight: never assigned by the original (uninitialized).
}
void ZealotAI::LoadSubclass(HMESSAGEREAD h)
{
    auto* s = GetServerDE();
    const DBYTE values = s->ReadFromMessageByte(h);
    const float height = s->ReadFromMessageFloat(h);
    if (values > 7 || !std::isfinite(height))
    {
        s->RejectPayload(h, "ZealotAI Load: flag bits or height rejected");
        return;
    }
    // The staff effect object is an unsaved BaseClass: a restored cast keeps start_staff with no staff,
    // which the next update clears, as the original's NULL-object calls do nothing.
    floating = values & 1;
    start_staff = (values & 2) != 0;
    npc_divine = (values & 4) != 0;
}

void Naga::SaveSubclass(HMESSAGEWRITE h)
{
    auto* s = GetServerDE();
    DVector color;
    float alpha = 1;
    s->GetObjectColor(m_hObject, color, alpha);
    s->WriteToMessageDWord(h, ceiling_whomps);
    s->WriteToMessageObject(h, spike); // Plain object inside a save: the NULL sentinel (D7).
    s->WriteToMessageFloat(h, full_health);
    s->WriteToMessageFloat(h, alpha);
}
void Naga::LoadSubclass(HMESSAGEREAD h)
{
    auto* s = GetServerDE();
    // Original quirk: m_iCeilingWhomps is written as a DWord and read as a Float, then truncated; a
    // small count reads as a denormal and becomes 0 (a listed re-save rewrite).
    const float whomps = s->ReadFromMessageFloat(h);
    s->ReadFromMessageObject(h); // Requires the sentinel and returns NULL.
    const float full = s->ReadFromMessageFloat(h), alpha = s->ReadFromMessageFloat(h);
    if (!std::isfinite(whomps) || whomps < 0 || whomps >= 4294967296.f || !F::finite(full) || full < 0 ||
        !std::isfinite(alpha) || alpha < 0 || alpha > 1)
    {
        s->RejectPayload(h, "Naga Load: whomp count, health or alpha rejected");
        return;
    }
    DVector color;
    float current;
    if (!s->GetObjectColor(m_hObject, color, current) || !s->SetObjectColor(m_hObject, color, alpha))
    {
        s->RejectPayload(h, "Naga Load: color unavailable");
        return;
    }
    ceiling_whomps = static_cast<unsigned>(whomps);
    // Departure (decision 4): the original then overwrites m_hSpike with the stale plain handle and
    // orphans the spike its initial update just created. The port keeps that new spike.
    full_health = full; // create_health stays the constructor's TRUE: the boss bar is resent.
}

void BoneLeech::SaveSubclass(HMESSAGEWRITE h)
{
    auto* s = GetServerDE();
    // m_bJumping: the port's jump is the FIRE_JUMP command (Metacmd command 10) while animating.
    s->WriteToMessageByte(h, m_bAnimating && m_nCurMetacmd == 10);
    s->WriteToMessageFloat(h, last_damage); // Raw absolute clocks.
    s->WriteToMessageFloat(h, last_detach);
    // OpenBlood2 tail: the replacement detach restores the world flags and dimensions saved at the
    // attach request, and keeps the tracked player linked until the re-attach gate.
    s->WriteToLoadSaveMessageObject(h, track);
    s->WriteToMessageDWord(h, world_flags);
    s->WriteToMessageVector(h, &world_dims);
}
void BoneLeech::LoadSubclass(HMESSAGEREAD h)
{
    auto* s = GetServerDE();
    bool jumping = false;
    const bool flag = F::read_bool(h, jumping);
    const float damage = s->ReadFromMessageFloat(h), detach = s->ReadFromMessageFloat(h);
    HOBJECT tracked = nullptr;
    s->ReadFromLoadSaveMessageObject(h, &tracked);
    const DDWORD flags = s->ReadFromMessageDWord(h);
    DVector dims;
    s->ReadFromMessageVector(h, &dims);
    if (!flag || !clock(damage) || !clock(detach) || tracked == m_hObject || !physics_vector(dims) || dims.x < 0 ||
        dims.y < 0 || dims.z < 0)
    {
        s->RejectPayload(h, "BoneLeech Load: jump flag, clock, tracked player or world body rejected");
        return;
    }
    // The jump flag mirrors AI_Mgr's command state, restored by its own block.
    last_damage = damage;
    last_detach = detach;
    track = tracked; // Its inter-object link comes back with LINK.
    world_flags = flags;
    world_dims = dims;
}
void BoneLeech::AfterLoad(HMESSAGEREAD h)
{
    AI_Mgr::AfterLoad(h);
    const auto enemy = loaded_enemy_attach;
    loaded_enemy_attach = nullptr;
    if (!enemy)
        return;
    // Original MID_LOADOBJECT: an attached creature re-points m_hEnemyAttach to the single player and
    // restarts STATE_EnemyAttach, which replays the attach handshake with the restored client.
    auto* s = GetServerDE();
    if (!dynamic_cast<CPlayerObj*>(s->HandleToObject(enemy)))
    {
        s->RejectPayload(h, "BoneLeech Load: the attached object is not the player");
        return;
    }
    attached = enemy;
    proceed = false; // m_bProceedAttach is not saved: the client acknowledges again.
    if (!SetNewState(StateEnemyAttach))
    {
        s->RejectPayload(h, "BoneLeech Load: attach state unavailable");
        return;
    }
    s->SetObjectFlags(m_hObject, (s->GetObjectFlags(m_hObject) | FLAG_REALLYCLOSE) & ~FLAG_GRAVITY);
}
