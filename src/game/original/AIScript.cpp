// Selected AI_Mgr.cpp / AIScriptList.h commands and state dispatch.
// See ai_script_contracts.md: preparation does not emulate the missing AI_Update.
#include "AI_Mgr.h"
#include "engine/model_commands.h"
#include <charconv>
#include <cmath>
#include <limits>

namespace
{
    bool same(std::string_view a, std::string_view b)
    {
        if (a.size() != b.size())
            return false;
        for (std::size_t i = 0; i < a.size(); ++i)
        {
            auto upper = [](char c) { return c >= 'a' && c <= 'z' ? char(c - 'a' + 'A') : c; };
            if (upper(a[i]) != upper(b[i]))
                return false;
        }
        return true;
    }
    // Defined atoi prefix semantics, including nonnumbers -> 0 and decimal
    // truncation. Reject overflow instead of invoking the source's undefined behavior.
    bool integer_prefix(std::string_view s, int& value)
    {
        while (!s.empty() && (s.front() == ' ' || (s.front() >= '\t' && s.front() <= '\r')))
            s.remove_prefix(1);
        bool negative = false;
        if (!s.empty() && (s.front() == '+' || s.front() == '-'))
        {
            negative = s.front() == '-';
            s.remove_prefix(1);
        }
        const auto digits = s.find_first_not_of("0123456789");
        s = s.substr(0, digits);
        value = 0;
        if (s.empty())
            return true;
        std::uint64_t number = 0;
        const auto [end, ec] = std::from_chars(s.data(), s.data() + s.size(), number);
        const auto limit = std::uint64_t(std::numeric_limits<int>::max()) + unsigned(negative);
        if (ec != std::errc{} || end != s.data() + s.size() || number > limit)
            return false;
        value = static_cast<int>(negative ? -static_cast<std::int64_t>(number) : static_cast<std::int64_t>(number));
        return true;
    }
    struct Busy
    {
        bool& flag;
        explicit Busy(bool& f) : flag(f)
        {
            flag = true;
        }
        ~Busy()
        {
            flag = false;
        }
    };
} // namespace
bool AI_Mgr::ParseScript(std::string_view text, DDWORD previous, ScriptProgram& out, std::string& error)
{
    error.clear();
    auto fail = [&](const char* why)
    {
        error = why;
        return false;
    };
    std::vector<std::vector<std::string>> words;
    if (text.empty() || text.size() > 1279 || text.find('\0') != text.npos ||
        !OpenBlood2::model_commands(text, words) || words.empty() || !same(words.front().front(), "SCRIPT"))
        return fail("Invalid bounded SCRIPT text");
    ScriptProgram parsed;
    parsed.flags = previous & ~2u;
    if (words.front().size() > 1)
    {
        const auto& argument = words.front()[1];
        if (same(argument, "INTERRUPTABLE"))
            parsed.flags |= 2u;
        else
        {
            int flags;
            if (!integer_prefix(argument, flags))
                return fail("Script flags overflow");
            parsed.flags = static_cast<DDWORD>(flags);
        }
    }
    constexpr std::string_view names[]{
        "DONE", "SETMOVEMENT",   "FOLLOWPATH",           "PLAYSOUND",   "SETSTATE", "TARGET",
        "WAIT", "PLAYANIMATION", "PLAYANIMATIONLOOPING", "MOVETOOBJECT"};
    for (std::size_t i = 1; i < words.size(); ++i)
    {
        if (words[i].size() != 2 || words[i][0].size() > 49 || words[i][1].size() > 49)
            return fail("Script instructions require a name and one argument, at most 49 bytes each");
        ScriptInstruction instruction;
        instruction.name = words[i][0];
        instruction.argument = words[i][1];
        for (std::size_t n = 1; n < std::size(names); ++n)
            if (same(instruction.name, names[n]))
            {
                instruction.command = static_cast<ScriptCommand>(n);
                break;
            }
        // Preflight unsafe conversions before any state mutation.
        int wait;
        if (instruction.command == ScriptCommand::Wait && !integer_prefix(instruction.argument, wait))
            return fail("Script wait overflow");
        parsed.commands.push_back(std::move(instruction));
    }
    out = std::move(parsed);
    return true;
}
bool AI_Mgr::ScriptFailure(std::string_view why)
{
    ++script_failures;
    script_error = why;
    script_outcome = ScriptOutcome::Failed;
    return false;
}
bool AI_Mgr::PrepareScript(std::string_view text)
{
    if (script_busy)
        return ScriptFailure("Reentrant script preparation rejected");
    ScriptProgram parsed;
    std::string error;
    if (!ParseScript(text, m_dwScriptFlags, parsed, error))
        return ScriptFailure(error);
    if (!SetNewState(StateScript))
        return ScriptFailure(state_error);
    script_commands = std::move(parsed.commands);
    m_dwScriptFlags = parsed.flags;
    script_index = 0;
    script_current = {};
    m_bUpdateScriptCmd = true;
    script_wait_end = 0;
    script_error.clear();
    script_outcome = ScriptOutcome::Ready;
    return true;
}
bool AI_Mgr::FinishScript()
{
    m_dwScriptFlags = 0;
    if (!ComputeState())
        return ScriptFailure(state_error.empty() ? "Script exit decision unavailable" : state_error);
    return script_outcome != ScriptOutcome::Failed; // Virtual decisions may reenter; preserve their failure latch.
}
bool AI_Mgr::UpdateScriptCommand(PathMgr& paths)
{
    m_bUpdateScriptCmd = false;
    script_current = {};
    if (script_commands.empty())
        return true;
    if (script_index >= script_commands.size())
        return ScriptFailure("Invalid script command cursor");
    script_current = script_commands[script_index]; // Own it across removal and callbacks.
    if (m_dwScriptFlags & 1u)
        script_index = script_index + 1 < script_commands.size() ? script_index + 1 : 0;
    else
    {
        script_index = 0;
        script_commands.erase(script_commands.begin());
    }
    ++script_selections;
    auto* server = GetServerDE();
    const auto& arg = script_current.argument;
    switch (script_current.command)
    {
        case ScriptCommand::SetMovement:
            return SetSetMovementCmd(arg) || ScriptFailure(script_motion_error);
        case ScriptCommand::FollowPath:
            return SetFollowPathCmd(paths, arg) || ScriptFailure(script_motion_error);
        case ScriptCommand::MoveToObject:
            return SetMoveToObjectCmd(arg) || ScriptFailure(script_motion_error);
        case ScriptCommand::SetState:
        {
            int state;
            if (!StateStrToInt(arg, state) || !SetNewState(state))
                return ScriptFailure("Script state selection failed");
            return true;
        }
        case ScriptCommand::Target:
            if (!SetTarget(arg) || !SetNewState(StateAttackFar))
                return ScriptFailure("Script target/state selection failed");
            return true;
        case ScriptCommand::Wait:
        {
            int seconds;
            if (!integer_prefix(arg, seconds) || !std::isfinite(server->GetTime()) ||
                !std::isfinite(script_wait_end = server->GetTime() + seconds))
                return ScriptFailure("Invalid script wait deadline");
            return true;
        }
        case ScriptCommand::PlayAnimation:
        case ScriptCommand::PlayAnimationLooping:
        {
            const auto clip = server->GetAnimIndex(m_hObject, arg.c_str());
            if (clip == UINT32_MAX || !SetAnimation(clip))
                return ScriptFailure("Script animation unavailable or resize blocked");
            server->SetModelLooping(m_hObject, script_current.command == ScriptCommand::PlayAnimationLooping);
            return true;
        }
        case ScriptCommand::PlaySound:
        {
            OpenBlood2::SoundRequest request;
            request.resource = arg;
            request.speaker = m_hObject;
            request.outer_radius = 5000;
            request.inner_radius = 500;
            request.priority = 2;
            std::string error;
            if (!server->PlayCharacterSound(m_hObject, request, false, error))
                return ScriptFailure(error);
            ++script_sounds;
            return true;
        }
        case ScriptCommand::Done:
            return FinishScript();
    }
    return ScriptFailure("Invalid script command type");
}
bool AI_Mgr::AI_STATE_Script(PathMgr& paths)
{
    return RunScriptState(paths, nullptr);
}
bool AI_Mgr::RunScriptState(PathMgr& paths, bool* suppress_fire)
{
    if (script_busy)
        return ScriptFailure("Reentrant script update rejected");
    if (script_outcome == ScriptOutcome::Failed)
        return false;
    auto* server = GetServerDE();
    if (!server || !server->IsObjectAlive(m_hObject) || script_outcome == ScriptOutcome::None)
        return ScriptFailure("Script requires a live owner and a prepared program");
    if (m_nState != StateScript)
    {
        if (script_outcome != ScriptOutcome::Completed)
            script_outcome = ScriptOutcome::StateChanged;
        return true; // The enclosing AI_Update would dispatch the new state.
    }
    Busy busy(script_busy);
    if (suppress_fire)
        *suppress_fire = true;
    ++script_updates;
    script_outcome = ScriptOutcome::Running;
    if (m_bUpdateScriptCmd && !UpdateScriptCommand(paths))
        return false;
    if (script_outcome == ScriptOutcome::Failed || !server->IsObjectAlive(m_hObject))
        return ScriptFailure("Script owner removed or command failed during selection");
    // EnhancedGideon restores gravity after selecting the command, before dispatch.
    if (suppress_fire)
        server->SetObjectFlags(m_hObject, server->GetObjectFlags(m_hObject) | FLAG_GRAVITY);
    switch (script_current.command)
    {
        case ScriptCommand::SetMovement:
        case ScriptCommand::PlaySound:
        case ScriptCommand::SetState:
        case ScriptCommand::Target:
            m_bUpdateScriptCmd = true;
            break;
        case ScriptCommand::FollowPath:
            if (!UpdateFollowPathCmd())
                return ScriptFailure(script_motion_error);
            break;
        case ScriptCommand::MoveToObject:
            if (!UpdateMoveToObjectCmd())
                return ScriptFailure(script_motion_error);
            break;
        case ScriptCommand::Wait:
            m_bUpdateScriptCmd = server->GetTime() >= script_wait_end;
            break;
        case ScriptCommand::PlayAnimation:
        {
            std::string name;
            if (!server->GetModelAnimationName(m_hObject, m_nCurAnimation, name))
                return ScriptFailure("Script animation model removed");
            m_bUpdateScriptCmd = (server->GetModelPlaybackState(m_hObject) & MS_PLAYDONE) != 0;
            break;
        }
        case ScriptCommand::PlayAnimationLooping: // Intentionally absent in the original switch.
        case ScriptCommand::Done:
            if (suppress_fire)
                *suppress_fire = false;
            if (!FinishScript())
                return false;
            script_outcome = ScriptOutcome::Completed;
            break;
    }
    if (script_outcome == ScriptOutcome::Failed)
        return false; // A nested callback failure must never be overwritten/replayed.
    if (!server->IsObjectAlive(m_hObject))
        return ScriptFailure("Script owner removed during command dispatch");
    if (m_nState != StateScript && script_outcome != ScriptOutcome::Completed)
        script_outcome = ScriptOutcome::StateChanged;
    return true;
}
