// Original AI_Mgr state/animation selections. See AI_STATE_CONTRACTS.md.
#include "AI_Mgr.h"
#include <cmath>
#include <limits>

bool AI_Mgr::StateFailure(const char* error)
{
    ++state_failures;
    state_error = error;
    return false;
}
bool AI_Mgr::StateStrToInt(std::string_view input, int& state)
{
    if (input.size() > 31 || input.find('\0') != input.npos)
        return false;
    std::string name(input);
    for (auto& c : name)
        if (c >= 'a' && c <= 'z')
            c -= 'a' - 'A';
    state = StateInactive;
    // Source compares only FOUR characters for SEARCH and accepts suffixes.
    constexpr std::pair<std::string_view, int> names[]{
        {"INACTIVE", StateInactive}, {"IDLE", StateIdle},         {"SEAR", StateSearchVisual},
        {"ATTACK", StateAttackFar},  {"HIDE", StateEscapeHide},   {"GUARD", StateGuard},
        {"SCRIPT", StateScript},     {"SPECIAL1", StateSpecial1}, {"SPECIAL2", StateSpecial2},
        {"SPECIAL3", StateSpecial3}, {"SPECIAL4", StateSpecial4}, {"SPECIAL5", StateSpecial5},
        {"SPECIAL6", StateSpecial6}, {"SPECIAL7", StateSpecial7}, {"SPECIAL8", StateSpecial8},
        {"SPECIAL9", StateSpecial9}, {"PASSIVE", StatePassive}};
    for (const auto& [prefix, value] : names)
        if (name.starts_with(prefix))
        {
            state = value;
            break;
        }
    return true;
}
bool AI_Mgr::StopVelocity()
{
    auto* server = GetServerDE();
    DVector velocity;
    if (!server || !server->IsObjectAlive(m_hObject) || server->GetVelocity(m_hObject, &velocity) != DE_OK)
        return StateFailure("StopVelocity requires a live owner and velocity");
    velocity.x = velocity.z = 0;
    if (m_nTrapped)
        velocity = {}; // Original Move(vVel, MATH_EPSILON) trapped branch.
    if (server->SetVelocity(m_hObject, &velocity) != DE_OK)
        return StateFailure("StopVelocity service rejected the request");
    return true;
}
bool AI_Mgr::SetNewState(int state)
{
    state_error.clear();
    requested_state = state;
    auto* server = GetServerDE();
    if (!server || !server->IsObjectAlive(m_hObject) ||
        !((state >= 0 && state <= 24) || (state >= 100 && state <= 108) || state == StateInactive))
        return StateFailure("Invalid AI state or owner");
    const auto time = server->GetTime();
    if (!std::isfinite(time) || !std::isfinite(m_fWaitForIdleTime) || !std::isfinite(time + 120.f))
        return StateFailure("Invalid AI idle deadline");
    // Preflight the only fallible mutation before committing state bookkeeping.
    if (!StopVelocity())
        return false;
    if (m_nState != StateWalkAround && m_nState != StateRunAround && m_nState != StateJumpOver &&
        m_nState != StateCrawlUnder && m_nState != StateSearchVisual && m_nState != StateSearchSmell)
        m_nLastState = m_nState;
    if (state == StateIdle && m_bHasTargeted)
        m_nState = time > m_fWaitForIdleTime ? StateIdle : StateSearchVisual;
    else
    {
        m_nState = state;
        m_fWaitForIdleTime = time + 120.f;
    }
    Metacmd = 1;
    m_nCurMetacmd = 999;
    m_bAnimating = false;
    ++state_changes;
    return true;
}
bool AI_Mgr::SetAnimation(DDWORD clip)
{
    using OpenBlood2::ResizeResult;
    state_error.clear();
    requested_animation = clip;
    animation_blocked = false;
    auto* server = GetServerDE();
    std::string name;
    if (!server || !server->IsObjectAlive(m_hObject) || !server->GetModelAnimationName(m_hObject, clip, name))
        return StateFailure("AI animation model/clip unavailable");
    if (clip == m_nCurAnimation)
    {
        if (server->ResetModelAnimation(m_hObject) != DE_OK)
            return StateFailure("AI animation reset failed");
    }
    else
    {
        DVector dimensions = m_vDims;
        DVector authored;
        if (server->GetModelAnimUserDims(m_hObject, &authored, clip) == DE_OK)
            dimensions = {authored.x * m_vScale.x, authored.y * m_vScale.y, authored.z * m_vScale.z};
        // Explicit replacement for the old SetObjectDims2: existing atomic,
        // foot-anchored clearance, no partial pushing or fabricated successful resize.
        const auto result = server->ResizeObject(m_hObject, dimensions);
        animation_blocked = result == ResizeResult::Blocked;
        if (result != ResizeResult::Resized)
            return StateFailure(result == ResizeResult::Blocked ? "AI animation resize blocked"
                                                                : "AI animation resize unavailable or invalid");
        server->SetModelAnimation(m_hObject, clip); // Validated above, no callbacks.
        m_vDims = dimensions;
        m_nCurAnimation = clip;
    }
    ++animation_starts;
    return true;
}
bool AI_Mgr::AnimationCommand(DDWORD clip, int command)
{
    auto* server = GetServerDE();
    state_error.clear();
    if (!server || !server->IsObjectAlive(m_hObject))
        return StateFailure("AI animation command requires a live owner");
    if (!m_bAnimating || m_nCurMetacmd != command)
    {
        if (!SetAnimation(clip))
            return false;
        server->SetModelLooping(m_hObject, DFALSE);
        m_bAnimating = true;
        m_nCurMetacmd = command;
    }
    else
    {
        std::string name;
        if (!server->GetModelAnimationName(m_hObject, server->GetModelAnimation(m_hObject), name))
            return StateFailure("AI animation playback unavailable");
        if (server->GetModelPlaybackState(m_hObject) & MS_PLAYDONE)
        {
            if (Metacmd == std::numeric_limits<int>::max())
                return StateFailure("AI Metacmd overflow");
            m_bAnimating = false;
            ++Metacmd;
            ++animation_finishes;
        }
    }
    return true;
}
bool AI_Mgr::MC_Idle()
{
    DDWORD clip = 0;
    if (!m_bAnimating || m_nCurMetacmd != 0)
    {
        auto* server = GetServerDE();
        if (!server || !server->IsObjectAlive(m_hObject) || !m_pAnim_Sound ||
            !server->RandomInt(0, 3, animation_choice))
            return StateFailure("AI idle table/owner/random service unavailable");
        clip = m_pAnim_Sound->m_nAnim_IDLE[animation_choice];
    }
    return AnimationCommand(clip, 0);
}
bool AI_Mgr::MC_Talk()
{
    DDWORD clip = 0;
    if (!m_bAnimating || m_nCurMetacmd != 0)
    {
        auto* server = GetServerDE();
        if (!server || !server->IsObjectAlive(m_hObject) || !m_pAnim_Sound ||
            !server->RandomInt(0, 4, animation_choice))
            return StateFailure("AI talk table/owner/random service unavailable");
        clip = m_pAnim_Sound->m_nAnim_TALK[animation_choice];
    }
    // Original Talk deliberately shares MC_IDLE, including cross-command continuation.
    return AnimationCommand(clip, 0);
}
bool AI_Mgr::MC_Special(int index)
{
    DDWORD clip = 0;
    if (!m_bAnimating || m_nCurMetacmd != 25)
    {
        if (!m_pAnim_Sound || index < 0 || index >= 9)
            return StateFailure("AI special animation table/index unavailable");
        animation_choice = index;
        clip = m_pAnim_Sound->m_nAnim_SPECIAL[index];
    }
    return AnimationCommand(clip, 25);
}
bool AI_Mgr::ComputeState(int)
{
    return StateFailure("Subclass ComputeState is not integrated");
}
bool AI_Mgr::AI_STATE_Special(int)
{
    return StateFailure("Subclass special action is not integrated");
}
bool AI_Mgr::AI_STATE_Idle()
{
    const int stim = ComputeStimuli();
    if (stim < 0)
        return StateFailure("Idle perception failed; no state selected");
    if (stim == 3)
        return SetNewState(StateSearchSmell);
    if (stim > 0)
    {
        GetServerDE()->SetNextUpdate(m_hObject, .001f);
        return ComputeState(stim);
    }
    GetServerDE()->SetNextUpdate(m_hObject, .1f);
    switch (Metacmd)
    {
        case 1:
            if (m_bCabal)
                return MC_BestWeapon();
            ++Metacmd;
            return true;
        case 2:
            return MC_Idle();
        case 3:
            return ComputeState();
        default:
            return true; // Source switch has no default action.
    }
}
bool AI_Mgr::AI_STATE_Passive()
{
    if (!GetServerDE() || !GetServerDE()->IsObjectAlive(m_hObject))
        return StateFailure("AI state owner unavailable");
    if (m_damage.GetLastDamager())
        return ComputeState();
    GetServerDE()->SetNextUpdate(m_hObject, .1f);
    switch (Metacmd)
    {
        case 1:
            if (m_bCabal)
                return MC_BestWeapon();
            ++Metacmd;
            return true;
        case 2:
            return MC_Idle();
        case 3:
            Metacmd = 1;
            return true;
        default:
            return true;
    }
}
bool AI_Mgr::DispatchState()
{
    state_error.clear();
    auto* server = GetServerDE();
    if (!server || !server->IsObjectAlive(m_hObject))
        return StateFailure("AI state owner unavailable");
    ++state_dispatches;
    dispatched_state = m_nState;
    if (Metacmd <= 0)
        Metacmd = 1;
    switch (m_nState)
    {
        case StateDodge:
            return AI_STATE_Dodge();
        case StateTeleport:
            return AI_STATE_Teleport();
        case StateDying:
            return AI_STATE_Dying();
        case StateRecoil:
            return AI_STATE_Recoil();
        case StateEscapeRun:
            return AI_STATE_Escape_RunAway();
        case StateEscapeHide:
            return AI_STATE_Escape_Hide();
        case StateGuard:
            return AI_STATE_GuardLocation();
        case StateJumpOver:
            return AI_STATE_JumpOverObj();
        case StateCrawlUnder:
            return AI_STATE_CrawlUnderObj();
        case StateWalkAround:
            return AI_STATE_WalkAroundObj();
        case StateRunAround:
            return AI_STATE_RunAroundObj();
        case StateWalkTo:
            return AI_STATE_WalkToPos();
        case StateRunTo:
            return AI_STATE_RunToPos();
        case StateSearchVisual:
            return AI_STATE_SearchVisualTarget();
        case StateSearchSmell:
            return AI_STATE_SearchSmellTarget();
        default:
            break;
    }
    if (m_nState == StateAttackClose)
        return AI_STATE_AttackClose();
    if (m_nState == StateAttackFar)
        return AI_STATE_AttackFar();
    if (m_nState == StateIdle)
        return AI_STATE_Idle();
    if (m_nState == StatePassive)
        return AI_STATE_Passive();
    if (m_nState >= StateSpecial1 && m_nState <= StateSpecial8)
        return AI_STATE_Special(m_nState - StateSpecial1 + 1);
    if (m_nState == StateInactive)
        return true;
    return StateFailure("Selected AI action is not integrated");
}

bool AI_Mgr::AI_STATE_Teleport()
{
    return StateFailure("Subclass teleport is not integrated");
}
bool AI_Mgr::AI_STATE_AttackClose()
{
    return StateFailure("Subclass close attack is not integrated");
}
bool AI_Mgr::AI_STATE_AttackFar()
{
    return StateFailure("Subclass far attack is not integrated");
}

bool AI_Mgr::AI_STATE_Dodge()
{
    return StateFailure("Subclass dodge action is not integrated");
}
