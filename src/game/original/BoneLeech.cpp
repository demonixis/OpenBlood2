// Original ObjectDLL/boneleech.cpp (Monolith), selected state/motion callbacks.
#include "BoneLeech.h"
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
        return SetNewState(StateSearchVisual);
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
                attachment_requested = true;
                return RuntimeFailure("BoneLeech client attachment handshake is not integrated");
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
    if (message == MID_TOUCHNOTIFY && data && data == GetTarget() && m_nCurMetacmd == 10)
    {
        auto* s = GetServerDE();
        if (s->IsObjectAlive(static_cast<HOBJECT>(data)) &&
            s->IsKindOf(s->GetObjectClass(static_cast<HOBJECT>(data)), s->GetClass("CPlayerObj")))
        {
            attachment_requested = true;
            RuntimeFailure("BoneLeech client attachment handshake is not integrated");
        }
        return 0; // Original early return for a touched target during FireJump.
    }
    return AI_Mgr::EngineMessageFn(message, data, reason);
}
