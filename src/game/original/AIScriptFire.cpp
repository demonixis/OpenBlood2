// Selected original AI_Mgr.cpp:4618-4744,4823-4892 (Monolith, 1997/1998).
// Source scripted fire differs from autonomous MC_Fire_* in cadence/phase handling.
#include "AI_Mgr.h"
#include <cmath>

bool AI_Mgr::ScriptFire(ScriptFireMode mode, bool& engaged)
{
    engaged = script_fire_engaged = false;
    script_fire_mode = mode;
    ++script_fire_calls;
    if (!MotionReady())
        return false;
    auto* s = GetServerDE();
    const bool moving = mode != ScriptFireMode::Stand, run = mode == ScriptFireMode::Run;
    const int command = moving ? (run ? 9 : 8) : 7;
    const float time = s->GetTime();
    if (!std::isfinite(time) || !std::isfinite(m_fLoadTimeStart) || !std::isfinite(m_fAttackLoadTime) ||
        m_fAttackLoadTime < 0)
        return MotionFailure("Script fire clock/cadence invalid");
    if (!moving)
    {
        if (!ChooseOpportunityTarget())
            return perception_error.empty() || MotionFailure(perception_error.c_str());
        if (!RefreshTargetPosition() || !MC_FaceTarget())
            return MotionFailure(target_error.c_str());
        --Metacmd; // Source cancels this face increment, but not the later timed-fire face.
        engaged = script_fire_engaged = true;
    }
    if (!m_bAnimating || m_nCurMetacmd != command)
    {
        auto* weapon = m_InventoryMgr.CurrentWeapon();
        const auto type = weapon ? weapon->data.m_nFireType : moving ? TYPE_PISTOL : TYPE_MELEE;
        if (!m_pAnim_Sound || type >= std::size(m_pAnim_Sound->m_nAnim_FIRE_STAND))
            return MotionFailure("Script fire animation table unavailable");
        if (weapon)
            m_fAttackLoadTime = weapon->data.m_fReloadTime;
        if (!std::isfinite(m_fAttackLoadTime) || m_fAttackLoadTime < 0)
            return MotionFailure("Script fire weapon reload invalid");
        const auto clip = !moving ? m_pAnim_Sound->m_nAnim_FIRE_STAND[type]
                          : run   ? m_pAnim_Sound->m_nAnim_FIRE_RUN[type]
                                  : m_pAnim_Sound->m_nAnim_FIRE_WALK[type];
        if (!SetAnimation(clip))
            return MotionFailure("Script fire clip/resize failed"); // No invented crawl recovery.
        s->SetModelLooping(m_hObject, moving);
        m_fLoadTimeStart = m_fTimeStart = 0;
        if (moving && !Move(m_MoveObj.GetForwardVector(), run ? m_fRunSpeed : m_fWalkSpeed))
            return false;
        m_bAnimating = true;
        m_nCurMetacmd = command;
        return true;
    }
    std::string clip;
    if (!s->GetModelAnimationName(m_hObject, s->GetModelAnimation(m_hObject), clip))
        return MotionFailure("Script fire playback unavailable");
    if (moving && !Move(m_MoveObj.GetForwardVector(), run ? m_fRunSpeed : m_fWalkSpeed))
        return false;
    if (time > m_fLoadTimeStart + m_fAttackLoadTime)
    {
        bool target = !moving || ChooseOpportunityTarget();
        if (!target && !perception_error.empty())
            return MotionFailure(perception_error.c_str());
        if (target)
        {
            engaged = script_fire_engaged = true;
            // Moving source fire does not refresh m_vTargetPos after acquisition.
            // Preserve its cached-position aim; MC_FacePos checks the coordinates
            // without claiming a fresh pose for the newly linked identity.
            if (!(moving ? MC_FacePos(m_vTargetPos) : MC_FaceTarget()))
                return MotionFailure(target_error.c_str());
            const auto changes = state_changes, failures = script_failures;
            const bool accepted = Fire();
            if (!s->IsObjectAlive(m_hObject) || script_motion_outcome == MotionOutcome::Failed ||
                state_changes != changes || script_failures != failures)
                return MotionFailure("Script fire owner removed or command reentered during shot");
            if (!accepted && fire_result != FireResult::Blocked)
                return MotionFailure("Script fire required shot service failed");
            if (moving || accepted)
                m_fLoadTimeStart = time; // Moving source consumes cadence even for a blocked shot.
            else
                m_bAnimating = false;
        }
    }
    if (s->GetModelPlaybackState(m_hObject) & MS_PLAYDONE)
    {
        if (moving && !StopVelocity())
            return MotionFailure("Script fire stop failed");
        m_bAnimating = false;
        ++animation_finishes; // Neither script variant advances Metacmd on completion.
    }
    return true;
}
bool AI_Mgr::Script_Fire_Stand(bool& engaged)
{
    engaged = false;
    if (motion_busy)
        return MotionFailure("Reentrant script fire rejected");
    motion_busy = true;
    const bool ok = ScriptFire(ScriptFireMode::Stand, engaged);
    motion_busy = false;
    return ok;
}
bool AI_Mgr::Script_Fire_Walk()
{
    if (motion_busy)
        return MotionFailure("Reentrant script fire rejected");
    motion_busy = true;
    bool engaged = false;
    const bool ok = ScriptFire(ScriptFireMode::Walk, engaged);
    motion_busy = false;
    return ok;
}
bool AI_Mgr::Script_Fire_Run()
{
    if (motion_busy)
        return MotionFailure("Reentrant script fire rejected");
    motion_busy = true;
    bool engaged = false;
    const bool ok = ScriptFire(ScriptFireMode::Run, engaged);
    motion_busy = false;
    return ok;
}
