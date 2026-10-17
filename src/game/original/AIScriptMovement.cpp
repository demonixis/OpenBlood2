// Selected original AI_Mgr.cpp:1397-1654,3139-3157,4109-4167,4244-4274.
// Typed, checked command helpers; not the full SCRIPT interpreter or AI_Update.
#include "AI_Mgr.h"
#include "engine/collision.h"
#include <cmath>
#include <limits>
using namespace OpenBlood2;
namespace
{
    struct Busy
    {
        bool& value;
        explicit Busy(bool& v) : value(v)
        {
            value = true;
        }
        ~Busy()
        {
            value = false;
        }
    };
    bool equal(std::string_view a, std::string_view b)
    {
        if (a.size() != b.size())
            return false;
        for (std::size_t i = 0; i < a.size(); ++i)
        {
            const auto upper = [](char c) { return c >= 'a' && c <= 'z' ? char(c - 'a' + 'A') : c; };
            if (upper(a[i]) != upper(b[i]))
                return false;
        }
        return true;
    }
    bool text_valid(std::string_view text, std::size_t max)
    {
        return text.size() <= max && text.find('\0') == text.npos;
    }
} // namespace
bool AI_Mgr::MotionFailure(const char* error)
{
    script_motion_error = error;
    script_motion_outcome = MotionOutcome::Failed;
    ++script_motion_failures;
    return false;
}
bool AI_Mgr::MotionReady()
{
    auto* server = GetServerDE();
    if (!server || !server->IsObjectAlive(m_hObject) || !m_MoveObj.initialized || !m_MoveObj.sampled)
        return MotionFailure("Script movement requires a live owner and initialized sampled CMovement");
    if ((m_eScriptMovement != SM_WALK && m_eScriptMovement != SM_RUN) || !std::isfinite(m_fWalkSpeed) ||
        !std::isfinite(m_fRunSpeed) || m_fWalkSpeed < 0 || m_fRunSpeed < 0 || m_fWalkSpeed > 1e7f || m_fRunSpeed > 1e7f)
        return MotionFailure("Invalid original script movement mode/speeds");
    return true;
}
bool AI_Mgr::Move(DVector direction, float speed)
{
    auto* server = GetServerDE();
    if (!server || !server->IsObjectAlive(m_hObject) || !physics_vector(direction) || !std::isfinite(speed) ||
        std::abs(speed) > 1e7f)
        return MotionFailure("Invalid AI velocity request");
    // Original epsilon sentinel copies the supplied velocity; direction is never normalized.
    auto velocity = m_nTrapped ? DVector{} : speed == 1e-11f ? direction : vector_scale(direction, speed);
    if (!physics_vector(velocity) || server->SetVelocity(m_hObject, &velocity) != DE_OK)
        return MotionFailure("AI velocity service rejected request");
    script_motion_velocity = velocity;
    ++velocity_commands;
    return true;
}
bool AI_Mgr::ScriptLocomotion(bool run)
{
    if (!MotionReady())
        return false;
    auto* server = GetServerDE();
    const int command = run ? 2 : 1;
    if (!m_bAnimating || m_nCurMetacmd != command)
    {
        auto* weapon = m_InventoryMgr.CurrentWeapon();
        const auto type = weapon ? weapon->data.m_nFireType : TYPE_MELEE;
        if (!m_pAnim_Sound || type >= std::size(m_pAnim_Sound->m_nAnim_WALK))
            return MotionFailure("Script locomotion animation table unavailable");
        auto clip = run ? m_pAnim_Sound->m_nAnim_RUN[type] : m_pAnim_Sound->m_nAnim_WALK[type];
        // Source injury clip choice requires a weapon, even though any injury slows the first walk command.
        if (!run && weapon && (m_nInjuredLeg == 10 || m_nInjuredLeg == 14))
        {
            const bool pistol = type == TYPE_PISTOL || type == TYPE_MELEE;
            clip = m_nInjuredLeg == 10 ? (pistol ? m_pAnim_Sound->m_nAnim_WALK_INJURED_LLEG_PISTOL
                                                 : m_pAnim_Sound->m_nAnim_WALK_INJURED_LLEG_RIFLE)
                                       : (pistol ? m_pAnim_Sound->m_nAnim_WALK_INJURED_RLEG_PISTOL
                                                 : m_pAnim_Sound->m_nAnim_WALK_INJURED_RLEG_RIFLE);
        }
        const auto time = server->GetTime();
        if (!std::isfinite(time))
            return MotionFailure("Script locomotion clock unavailable");
        if (!SetAnimation(clip))
            return MotionFailure("Script locomotion clip/resize failed");
        // Unlike MC_Walk/Run, source Script_Walk/Run does not choose crawl recovery.
        // It ignored resize failure; the typed port explicitly fails instead.
        server->SetModelLooping(m_hObject, DTRUE);
        if (!Move(m_MoveObj.GetForwardVector(), run ? m_fRunSpeed : m_nInjuredLeg ? m_fWalkSpeed / 3 : m_fWalkSpeed))
            return false;
        m_fTimeStart = time;
        m_bAnimating = true;
        m_nCurMetacmd = command;
    }
    else
    {
        std::string clip;
        if (!server->GetModelAnimationName(m_hObject, server->GetModelAnimation(m_hObject), clip))
            return MotionFailure("Script locomotion playback unavailable");
        if (!Move(m_MoveObj.GetForwardVector(), run ? m_fRunSpeed : m_fWalkSpeed))
            return false;
        if (server->GetModelPlaybackState(m_hObject) & MS_PLAYDONE)
        {
            if (!StopVelocity())
                return MotionFailure("Script locomotion stop failed");
            m_bAnimating = false;
            ++animation_finishes; // Script variant does not advance Metacmd.
        }
    }
    return true;
}
bool AI_Mgr::Script_Walk()
{
    if (motion_busy)
        return MotionFailure("Reentrant script walk rejected");
    return ScriptLocomotion(false);
}
bool AI_Mgr::Script_Run()
{
    if (motion_busy)
        return MotionFailure("Reentrant script run rejected");
    return ScriptLocomotion(true);
}
bool AI_Mgr::SetSetMovementCmd(std::string_view name)
{
    if (motion_busy)
        return MotionFailure("Reentrant script movement selection rejected");
    if (!text_valid(name, 255))
        return MotionFailure("Invalid script movement text");
    if (equal(name, "WALK"))
        m_eScriptMovement = SM_WALK;
    else if (equal(name, "RUN"))
        m_eScriptMovement = SM_RUN;
    return true; // Source ignores an unknown mode, without trimming.
}
bool AI_Mgr::SetFollowPathCmd(PathMgr& paths, std::string_view name)
{
    if (motion_busy)
        return MotionFailure("Reentrant FOLLOWPATH selection rejected");
    if (!MotionReady())
        return false;
    if (!text_valid(name, 100) || name.empty())
        return MotionFailure("Invalid FOLLOWPATH request");
    script_motion_name = std::string(name);
    script_path.clear();
    script_motion_error.clear();
    script_motion_command = MotionCommand::FollowPath;
    m_bUpdateScriptCmd = false;
    Metacmd = 1;
    std::string error;
    if (!paths.GetPath(script_motion_name, script_path, error))
        return MotionFailure(error.c_str());
    script_motion_outcome = MotionOutcome::Selected;
    return true;
}
bool AI_Mgr::SetMoveToObjectCmd(std::string_view name)
{
    if (motion_busy)
        return MotionFailure("Reentrant MOVETOOBJECT selection rejected");
    if (!MotionReady())
        return false;
    if (!text_valid(name, 100) || name.empty())
        return MotionFailure("Invalid MOVETOOBJECT request");
    script_motion_name = std::string(name);
    script_motion_error.clear();
    auto* server = GetServerDE();
    auto* list = server->FindNamedObjects(script_motion_name.c_str());
    if (!list)
        return MotionFailure("MOVETOOBJECT name lookup unavailable");
    const auto target = list->m_pFirstLink ? list->m_pFirstLink->m_hObject : nullptr;
    server->RelinquishList(list);
    if (target && !SetNewTarget(target))
        return MotionFailure("MOVETOOBJECT target link failed");
    script_motion_command = MotionCommand::MoveToObject;
    m_bUpdateScriptCmd = !target;
    Metacmd = 1;
    script_motion_outcome = target ? MotionOutcome::Selected : MotionOutcome::Completed;
    return true; // No match completes, preserving the previous linked target as in source.
}
bool AI_Mgr::FaceScriptPoint(DVector point)
{
    DVector current;
    if (GetServerDE()->GetObjectPos(m_hObject, &current) != DE_OK)
        return MotionFailure("Script facing owner unavailable");
    if (vector_dot(vector_subtract(point, current), vector_subtract(point, current)) == 0)
    {
        // Defined zero-direction adaptation: keep heading and the source command increment.
        if (Metacmd == std::numeric_limits<int>::max())
            return MotionFailure("Script facing command overflow");
        ++Metacmd;
        return true;
    }
    return MC_FacePos(point) || MotionFailure("Script facing failed");
}
bool AI_Mgr::DeliverPathAction(const PathListData& point)
{
    if (!point.action_message)
        return true;
    auto* server = GetServerDE();
    if (!text_valid(*point.action_message, 1279) || (point.action_target && !text_valid(*point.action_target, 100)))
        return MotionFailure("Path action text exceeds typed message limits");
    std::vector<HOBJECT> targets;
    if (point.action_target)
    {
        if (point.action_target->empty())
            return true; // Source named helper ignores an empty target.
        auto* list = server->FindNamedObjects(point.action_target->c_str());
        if (!list)
            return MotionFailure("Path action target lookup unavailable");
        for (auto* link = list->m_pFirstLink; link; link = link->m_pNext)
        {
            if (targets.size() == 65536)
            {
                server->RelinquishList(list);
                return MotionFailure("Path action target limit");
            }
            targets.push_back(link->m_hObject);
        }
        server->RelinquishList(list);
    }
    else
        targets.push_back(m_hObject);
    for (auto target : targets)
    {
        if (!server->IsObjectAlive(m_hObject))
            return MotionFailure("Path owner removed during action");
        if (!server->IsObjectAlive(target))
            continue;
        if (point.action_target && equal(*point.action_message, "REMOVE"))
        {
            server->SetObjectUserFlags(target, 0);
            server->RemoveObject(target);
            ++path_removals;
        }
        else
        {
            auto string = server->CreateString(point.action_message->c_str());
            auto message = server->StartMessageToObject(this, target, 1000);
            if (!string || !message || server->WriteToMessageHString(message, string) != DE_OK)
            {
                if (string)
                    server->FreeString(string);
                if (message)
                    server->EndHMessageWrite(message);
                return MotionFailure("Path action message allocation failed");
            }
            server->FreeString(string);
            if (server->EndMessage(message) != DE_OK)
                return MotionFailure("Path action delivery failed");
            ++path_deliveries; // Transport success, not recipient action acceptance.
        }
    }
    if (!server->IsObjectAlive(m_hObject))
        return MotionFailure("Path owner removed during action");
    return true;
}
bool AI_Mgr::UpdateFollowPathCmd()
{
    if (motion_busy)
        return MotionFailure("Reentrant FOLLOWPATH update rejected");
    if (script_motion_outcome == MotionOutcome::Failed)
        return false;
    if (!MotionReady())
        return false;
    if (script_motion_command != MotionCommand::FollowPath)
        return MotionFailure("FOLLOWPATH is not selected");
    if (m_bUpdateScriptCmd)
        return true;
    Busy guard(motion_busy);
    ++script_motion_updates;
    if (script_path.empty())
    {
        m_bUpdateScriptCmd = true;
        script_motion_outcome = MotionOutcome::Completed;
        return true;
    }
    const auto point = script_path.front(); // Own action data across arbitrary trigger callbacks.
    const auto pos = m_MoveObj.GetPos();
    auto target = point.position;
    target.y = pos.y;
    script_motion_destination = target;
    const auto dt = GetServerDE()->GetFrameTime();
    const auto speed = m_eScriptMovement == SM_RUN ? m_fRunSpeed : m_fWalkSpeed;
    if (!physics_vector(target) || !std::isfinite(dt) || dt < 0 || dt > .5f)
        return MotionFailure("Invalid FOLLOWPATH position/frame delta");
    if (!FaceScriptPoint(target))
        return false;
    const auto delta = vector_subtract(target, pos);
    script_motion_distance = std::hypot(delta.x, delta.y, delta.z);
    script_motion_threshold = dt * speed * 2;
    if (script_motion_distance <= script_motion_threshold)
    {
        if (!m_nTrapped && GetServerDE()->MoveObject(m_hObject, &target) != DE_OK)
            return MotionFailure("FOLLOWPATH arrival move failed");
        if (!GetServerDE()->IsObjectAlive(m_hObject))
            return MotionFailure("FOLLOWPATH owner removed during arrival");
        if (script_motion_outcome == MotionOutcome::Failed)
            return false; // A touch callback may have rejected a recursive command.
        if (!DeliverPathAction(point))
            return false;
        script_path.erase(script_path.begin());
        ++path_arrivals;
        if (script_motion_outcome == MotionOutcome::Failed)
            return false;
        script_motion_outcome = MotionOutcome::Arrived;
    }
    else
    {
        bool engaged = false;
        if ((m_dwScriptFlags & (1u << 2)) && !ScriptFire(ScriptFireMode::Stand, engaged))
            return false;
        if (!engaged && !ScriptLocomotion(m_eScriptMovement == SM_RUN))
            return false;
        script_motion_outcome = MotionOutcome::Moving;
    }
    return true;
}
bool AI_Mgr::UpdateMoveToObjectCmd()
{
    if (motion_busy)
        return MotionFailure("Reentrant MOVETOOBJECT update rejected");
    if (script_motion_outcome == MotionOutcome::Failed)
        return false;
    if (!MotionReady())
        return false;
    if (script_motion_command != MotionCommand::MoveToObject)
        return MotionFailure("MOVETOOBJECT is not selected");
    if (m_bUpdateScriptCmd)
        return true;
    Busy guard(motion_busy);
    ++script_motion_updates;
    auto* server = GetServerDE();
    if (!m_hTarget)
    {
        m_bUpdateScriptCmd = true;
        script_motion_outcome = MotionOutcome::Completed;
        return true;
    }
    DVector target;
    if (!server->IsObjectAlive(m_hTarget) || server->GetObjectPos(m_hTarget, &target) != DE_OK)
        return MotionFailure("MOVETOOBJECT target is pending or unavailable");
    const auto pos = m_MoveObj.GetPos();
    target.y = pos.y;
    script_motion_destination = target;
    switch (Metacmd)
    {
        case 1:
            if (!m_MoveObj.CalculatePath(target))
                return MotionFailure("MOVETOOBJECT path calculation failed; partial path not followed");
            ++Metacmd;
            break;
        case 2:
        {
            DVector point;
            if (!m_MoveObj.GetNextPathPoint(point))
                return MotionFailure("MOVETOOBJECT route unexpectedly empty");
            if (!FaceScriptPoint(point))
                return false;
            break;
        }
        case 3:
        {
            DVector point;
            if (!m_MoveObj.GetNextPathPoint(point))
                return MotionFailure("MOVETOOBJECT route unexpectedly empty");
            const float speed = m_eScriptMovement == SM_RUN ? m_fRunSpeed : m_fWalkSpeed, dt = server->GetFrameTime();
            if (!std::isfinite(dt) || dt < 0 || dt > .5f)
                return MotionFailure("Invalid MOVETOOBJECT frame delta");
            const auto d = vector_subtract(point, pos);
            script_motion_distance = std::hypot(d.x, d.y, d.z);
            script_motion_threshold = dt * speed * 2;
            if (script_motion_distance <= script_motion_threshold)
            {
                if (!m_MoveObj.MoveToNextPathPoint())
                    ++Metacmd;
                else
                    Metacmd = 2;
                ++path_arrivals;
            }
            else
            {
                if (m_dwScriptFlags & (1u << 2))
                {
                    bool engaged = false;
                    if (!ScriptFire(m_eScriptMovement == SM_RUN ? ScriptFireMode::Run : ScriptFireMode::Walk, engaged))
                        return false;
                }
                else if (!ScriptLocomotion(m_eScriptMovement == SM_RUN))
                    return false;
                Metacmd = 2;
            }
            break;
        }
        case 4:
            if (!m_nTrapped && server->MoveObject(m_hObject, &target) != DE_OK)
                return MotionFailure("MOVETOOBJECT final move failed");
            if (!server->IsObjectAlive(m_hObject))
                return MotionFailure("MOVETOOBJECT owner removed during final move");
            if (script_motion_outcome == MotionOutcome::Failed)
                return false;
            m_bUpdateScriptCmd = true;
            script_motion_outcome = MotionOutcome::Completed;
            return true;
        default:
            return MotionFailure("MOVETOOBJECT command index invalid");
    }
    script_motion_outcome = MotionOutcome::Moving;
    return true;
}
