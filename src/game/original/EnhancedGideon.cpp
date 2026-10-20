// Original ObjectDLL/EnhancedGideon.cpp:857-1002. Shared script dispatch retains
// its source no-fire/gravity ordering; common combat lives in Gideon.cpp.
#include "EnhancedGideon.h"
#include <cmath>

bool EnhancedGideon::MC_Walk()
{
    return Locomote(false, false);
}
bool EnhancedGideon::MC_Run()
{
    return Locomote(true, false);
}
bool EnhancedGideon::ScriptLocomotion(bool run)
{
    return Locomote(run, true);
}
bool EnhancedGideon::Locomote(bool run, bool script)
{
    state_error.clear();
    auto* server = GetServerDE();
    const auto* table = AnimSound();
    if (!server || !server->IsObjectAlive(m_hObject) || !table || !m_MoveObj.initialized || !m_MoveObj.sampled ||
        !std::isfinite(m_fWalkSpeed) || !std::isfinite(m_fRunSpeed) || m_fWalkSpeed < 0 || m_fRunSpeed < 0 ||
        m_fWalkSpeed > 1e7f || m_fRunSpeed > 1e7f || !std::isfinite(server->GetTime()))
        return StateFailure("EnhancedGideon locomotion owner/table/cache/speed unavailable");
    const int command = run ? 2 : 1;
    if (!m_bAnimating || m_nCurMetacmd != command)
    {
        auto flags = server->GetObjectFlags(m_hObject);
        if (!run && (script || m_nState == StateScript))
            server->SetObjectFlags(m_hObject, flags | FLAG_GRAVITY);
        else if (run && !script && m_nState == StateScript)
            server->SetObjectFlags(m_hObject, flags & ~FLAG_GRAVITY);
        m_fTimeStart = server->GetTime();
        if (!SetAnimation(run ? table->m_nAnim_RUN[TYPE_MELEE] : table->m_nAnim_WALK[TYPE_MELEE]))
            return false;
        server->SetModelLooping(m_hObject, DTRUE);
        // Both run variants initially use WALK speed. Only initial walking
        // applies the injured-leg divisor; later commands use full speed.
        if (!Move(m_MoveObj.GetForwardVector(), !run && m_nInjuredLeg ? m_fWalkSpeed / 3 : m_fWalkSpeed))
            return false;
        m_bAnimating = true;
        m_nCurMetacmd = command;
    }
    else
    {
        std::string name;
        if (!server->GetModelAnimationName(m_hObject, server->GetModelAnimation(m_hObject), name))
            return StateFailure("EnhancedGideon locomotion playback unavailable");
        if (!Move(m_MoveObj.GetForwardVector(), run ? m_fRunSpeed : m_fWalkSpeed))
            return false;
        if (server->GetModelPlaybackState(m_hObject) & MS_PLAYDONE)
        {
            if (!StopVelocity())
                return false;
            m_bAnimating = false;
            ++animation_finishes; // Unlike ordinary MC_Walk/Run, no Metacmd increment.
        }
    }
    return true;
}
