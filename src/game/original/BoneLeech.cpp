// Original ObjectDLL/boneleech.cpp (Monolith), selected state/motion callbacks.
#include "BoneLeech.h"
#include "PlayerObj.h"
#include "engine/collision.h"
#include <cmath>
using namespace OpenBlood2;

bool BoneLeech::ComputeState(int value)
{
    const int stimulus = value == -1 ? ComputeStimuli() : value;
    if (stimulus < 0)
        return StateFailure("BoneLeech stimulus unavailable");
    if (!stimulus)
        return SetNewState(m_nState == StateIdle || m_nState == StateSearchVisual || m_nState == StateSearchSmell
                               ? StateIdle
                               : StateSearchSmell);
    // Source tests the argument, not the result of ComputeStimuli.
    return SetNewState(value == 3 ? StateSearchSmell : StateAttackClose);
}
bool BoneLeech::AI_STATE_AttackClose()
{
    if (!m_MoveObj.initialized || !m_MoveObj.sampled || !physics_vector(m_vTargetPos))
        return StateFailure("BoneLeech movement cache unavailable");
    if (!GetTarget() && m_nCurMetacmd != 10 /* MC_FIRE_JUMP */)
        return npc_type == NPCType::Thief ? ComputeState() : SetNewState(StateSearchVisual);
    switch (Metacmd)
    {
        case 1:
            return MC_FaceTarget();
        case 2:
        {
            const auto delta = vector_subtract(m_MoveObj.GetPos(), m_vTargetPos);
            if (std::sqrt(vector_dot(delta, delta)) <= 100 || m_nCurMetacmd == 10)
                return FireJump();
            if (!MC_FaceTarget())
                return false;
            --Metacmd;
            bool ledge;
            if (!IsLedge(m_MoveObj.GetForwardVector(), ledge))
                return StateFailure(locomotion_error.c_str());
            return ledge ? MC_Idle() : MC_Run();
        }
        case 3:
            return ComputeState();
        default:
            return true;
    }
}
bool BoneLeech::FireJump()
{
    auto* s = GetServerDE();
    if (!AnimSound() || !s->IsObjectAlive(m_hObject) || !m_MoveObj.initialized)
        return StateFailure("BoneLeech jump owner/table/cache unavailable");
    if (!m_bAnimating || m_nCurMetacmd != 10)
    {
        if (!SetAnimation(AnimSound()->m_nAnim_JUMP[4]))
            return false;
        const auto forward = m_MoveObj.GetForwardVector();
        DVector velocity{forward.x * m_fRunSpeed * 2, m_MoveObj.GetUpVector().y * m_fJumpSpeed,
                         forward.z * m_fRunSpeed * 2};
        if (!Move(velocity, 1e-11f))
            return StateFailure(script_motion_error.c_str());
        s->SetModelLooping(m_hObject, DFALSE);
        m_bAnimating = true;
        m_nCurMetacmd = 10;
        ++jumps;
    }
    else
    {
        MovementState support;
        if (!s->GetMovementState(m_hObject, support))
            return StateFailure("BoneLeech jump support unavailable");
        if (support.standing)
        {
            // Preserve the source's reversed IsKindOf operands on the standing path.
            if (support.ground_object &&
                s->IsKindOf(s->GetClass("CPlayerObj"), s->GetObjectClass(support.ground_object)))
            {
                return RequestAttach(support.ground_object);
            }
            if (!StopVelocity())
                return false;
            m_bAnimating = false;
            ++Metacmd;
        }
    }
    return true;
}
DDWORD BoneLeech::EngineMessageFn(DDWORD message, void* data, DFLOAT reason)
{
    if (message == MID_LINKBROKEN && data == track)
    {
        if (attached)
            Detach();
        track = nullptr;
    }
    if (message == MID_TOUCHNOTIFY && data && data == GetTarget() && m_nCurMetacmd == 10)
    {
        auto* s = GetServerDE();
        if (s->IsObjectAlive(static_cast<HOBJECT>(data)) &&
            s->IsKindOf(s->GetObjectClass(static_cast<HOBJECT>(data)), s->GetClass("CPlayerObj")))
        {
            RequestAttach(static_cast<HOBJECT>(data));
        }
        return 0; // Original early return for a touched target during FireJump.
    }
    return AI_Mgr::EngineMessageFn(message, data, reason);
}

bool BoneLeech::RequestAttach(HOBJECT player)
{
    auto* s = GetServerDE();
    if (m_nState == StateEnemyAttach || !s->IsObjectAlive(player))
        return false;
    auto* owner = dynamic_cast<CPlayerObj*>(s->HandleToObject(player));
    if (!owner || owner->m_damage.IsDead() || !owner->GetClient())
        return false;
    // Cancellation/owner removal can arrive before the first attach update.
    world_flags = s->GetObjectFlags(m_hObject);
    if (s->GetObjectDims(m_hObject, &world_dims) != DE_OK)
        return RuntimeFailure("BoneLeech world dimensions unavailable");
    if (s->CreateInterObjectLink(m_hObject, player) != DE_OK || !SetNewState(StateEnemyAttach))
        return RuntimeFailure("BoneLeech attach owner/state unavailable");
    // A camera-bound creature must keep servicing the handshake and damage even off-screen.
    if (!s->SetDeactivationTime(m_hObject, 0))
        return RuntimeFailure("BoneLeech attachment wake failed");
    s->SetObjectState(m_hObject, OBJSTATE_ACTIVE);
    m_bSetShutdown = true;
    attached = track = player;
    attachment_requested = true;
    proceed = false;
    return true;
}
bool BoneLeech::Respond(HOBJECT player, bool accept)
{
    if (player != attached || m_nState != StateEnemyAttach)
        return false;
    if (!accept)
        return Detach();
    if (Metacmd != 2)
        return false;
    proceed = true;
    return true;
}
bool BoneLeech::Detach()
{
    auto* s = GetServerDE();
    if (attached && s->IsObjectAlive(attached))
        if (auto* p = dynamic_cast<CPlayerObj*>(s->HandleToObject(attached)))
            s->SendToClient(p->GetClient(), CreatureAttach{m_hObject, attached, false});
    attached = nullptr;
    proceed = false;
    Metacmd = 4;
    return true;
}
bool BoneLeech::AI_STATE_EnemyAttach()
{
    auto* s = GetServerDE();
    auto* p = s->IsObjectAlive(attached) ? dynamic_cast<CPlayerObj*>(s->HandleToObject(attached)) : nullptr;
    if ((!p || p->m_damage.IsDead()) && Metacmd < 4)
        Detach();
    switch (Metacmd)
    {
        case 1:
        {
            if (s->SetModelFilenames(m_hObject,
                                     npc_type == NPCType::TheHand ? "models\\enemies\\TheHand_pv.abc"
                                     : npc_type == NPCType::Thief ? "models\\enemies\\Thief_pv.abc"
                                                                  : "models\\enemies\\boneleech_pv.abc",
                                     npc_type == NPCType::TheHand ? "skins\\enemies\\TheHand_pv.dtx"
                                     : npc_type == NPCType::Thief ? "skins\\enemies\\Thief_pv.dtx"
                                                                  : "skins\\enemies\\boneleech_pv.dtx") != DE_OK)
                return RuntimeFailure("BoneLeech view model unavailable");
            s->SetObjectFlags(m_hObject,
                              (world_flags | FLAG_REALLYCLOSE) & ~(FLAG_GRAVITY | FLAG_SOLID | FLAG_TOUCH_NOTIFY));
            DVector zero{};
            s->SetVelocity(m_hObject, &zero);
            s->SetAcceleration(m_hObject, &zero);
            ++Metacmd;
            if (s->SendToClient(p->GetClient(), CreatureAttach{m_hObject, attached}) != DE_OK)
                return RuntimeFailure("BoneLeech attachment message rejected");
            return true;
        }
        case 2:
            if (proceed)
                ++Metacmd;
            return true;
        case 3:
            if (s->GetTime() - last_damage >= 1)
            {
                int amount = 5, sound = 1;
                if (!s->RandomInt(npc_type == NPCType::Thief ? 7 : 5, npc_type == NPCType::Thief ? 15 : 10, amount))
                    return RuntimeFailure("BoneLeech damage random service unavailable");
                DVector position;
                s->GetObjectPos(m_hObject, &position);
                const Damage damage{m_MoveObj.GetForwardVector(), position,
                                    float(amount) * m_InventoryMgr.damage_multiplier, DamageType::Melee, m_hObject};
                const auto msg = s->StartMessageToObject(this, attached, damage_message);
                if (!msg)
                    return RuntimeFailure("BoneLeech damage message unavailable");
                const auto written = s->WriteDamage(msg, damage);
                const auto sent = s->EndMessage(msg);
                if (written != DE_OK || sent != DE_OK)
                    return RuntimeFailure("BoneLeech damage delivery failed");
                if (npc_type != NPCType::TheHand && !s->RandomInt(1, npc_type == NPCType::Thief ? 3 : 6, sound))
                    return RuntimeFailure("BoneLeech sound random service unavailable");
                if (s->IsObjectAlive(attached))
                    AnimSound()->PlaySound(attached,
                                           npc_type == NPCType::TheHand
                                               ? std::string("choke.wav")
                                               : std::string(npc_type == NPCType::Thief ? "th_bite_" : "bl_burrow_") +
                                                     std::to_string(sound) + ".wav",
                                           200, 100);
                last_damage = s->GetTime();
                ++damage_ticks;
            }
            return true;
        case 4:
        {
            // Restore the original model before resuming any cached world-animation indices.
            if (s->SetModelFilenames(m_hObject,
                                     npc_type == NPCType::TheHand ? "models\\enemies\\TheHand.abc"
                                     : npc_type == NPCType::Thief ? "models\\enemies\\Thief.abc"
                                                                  : "models\\enemies\\boneleech.abc",
                                     npc_type == NPCType::TheHand ? "skins\\enemies\\TheHand.dtx"
                                     : npc_type == NPCType::Thief ? "skins\\enemies\\Thief.dtx"
                                                                  : "skins\\enemies\\boneleech.dtx") != DE_OK)
                return RuntimeFailure("BoneLeech world model restore failed");
            s->SetObjectFlags(m_hObject, world_flags & ~(FLAG_REALLYCLOSE | FLAG_SOLID | FLAG_GRAVITY));
            if (world_dims.x > 0)
                s->SetObjectDims(m_hObject, &world_dims);
            if (s->IsObjectAlive(track))
            {
                DVector pos, dims, previous;
                DRotation rot;
                s->GetObjectPos(track, &pos);
                s->GetObjectDims(track, &dims);
                s->GetObjectPos(m_hObject, &previous);
                s->GetObjectRotation(track, &rot);
                // Source places at the target center. Our solid-box solver cannot leave two
                // bodies overlapping: try bounded nearby positions without crossing a wall.
                const float radius = std::sqrt(dims.x * dims.x + dims.z * dims.z) +
                                     std::sqrt(world_dims.x * world_dims.x + world_dims.z * world_dims.z) + 2;
                bool placed = false;
                for (const auto offset : {DVector{0, 0, -radius}, DVector{radius, 0, 0}, DVector{-radius, 0, 0},
                                          DVector{0, 0, radius}, DVector{0, dims.y + world_dims.y + 2, 0}})
                {
                    auto candidate = vector_add(pos, rotate_vector(rot, offset));
                    SegmentHit hit;
                    const auto query = s->IntersectSegment(pos, candidate, track, hit);
                    if (query != SegmentResult::Miss)
                        continue;
                    if (s->TeleportObject(m_hObject, &candidate) == DE_OK &&
                        s->ResizeObject(m_hObject, world_dims) == ResizeResult::Resized)
                    {
                        placed = true;
                        break;
                    }
                    s->TeleportObject(m_hObject, &previous);
                }
                if (!placed)
                    return true; // Remain nonsolid and retry; never block the player's body.
                s->SetObjectRotation(m_hObject, &rot);
                s->GetObjectPos(m_hObject, &pos);
                if (!MC_FacePos(vector_add(pos, rotate_vector(rot, {0, 0, -100}))))
                    return false;
            }
            s->SetObjectFlags(m_hObject, world_flags);
            m_nCurAnimation = INVALID_ANI;
            m_bAnimating = false;
            last_detach = s->GetTime();
            Metacmd = 5;
            return true;
        }
        default:
            if (s->GetTime() - last_detach > 4)
            {
                if (track && track != GetTarget())
                    s->BreakInterObjectLink(m_hObject, track);
                track = nullptr;
                s->SetDeactivationTime(m_hObject, 3);
                s->SetObjectState(m_hObject, OBJSTATE_ACTIVE);
                return ComputeState();
            }
            return true;
    }
}
DDWORD BoneLeech::ObjectMessageFn(HOBJECT sender, DDWORD message, HMESSAGEREAD read)
{
    if (message == damage_message && attached)
        return 0; // Original attached leech is immune; USE releases it.
    return AI_Mgr::ObjectMessageFn(sender, message, read);
}
