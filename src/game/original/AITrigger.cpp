// Original AI_Mgr.cpp:987-1263, selected command/hearing branches. See AI_TRIGGER_CONTRACTS.md.
#include "AI_Mgr.h"
#include "engine/collision.h"
#include "engine/model_commands.h"
#include <charconv>
#include <cmath>

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
    template <class T> bool number(std::string_view text, T& value)
    {
        if (text.starts_with('+'))
        {
            text.remove_prefix(1);
            if (text.starts_with('-'))
                return false;
        }
        if (text.empty() || text.starts_with('+'))
            return false;
        const auto [end, ec] = std::from_chars(text.data(), text.data() + text.size(), value);
        return ec == std::errc{} && end == text.data() + text.size() && std::isfinite(double(value));
    }
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
} // namespace
bool AI_Mgr::TriggerFailure(const char* error)
{
    ++trigger_failures;
    trigger_outcome = TriggerOutcome::Failed;
    trigger_error = error;
    return false;
}
bool AI_Mgr::PlayAnimation(std::string_view name)
{
    auto* server = GetServerDE();
    if (!server || !server->IsObjectAlive(m_hObject) || name.empty() || name.size() > 255 ||
        name.find('\0') != name.npos)
        return TriggerFailure("Invalid AI animation request");
    const auto clip = server->GetAnimIndex(m_hObject, std::string(name).c_str());
    // Validate/select before changing looping; missing/blocked clips are explicit failures.
    if (clip == UINT32_MAX || !SetAnimation(clip))
        return TriggerFailure("Requested AI animation unavailable or blocked");
    server->SetModelLooping(m_hObject, DFALSE);
    return true;
}
bool AI_Mgr::HandleTrigger(HOBJECT sender, std::string_view text)
{
    auto* server = GetServerDE();
    ++trigger_calls;
    if (trigger_busy)
        return TriggerFailure("Reentrant AI trigger rejected");
    trigger_error.clear();
    trigger_outcome = TriggerOutcome::None;
    trigger_text.assign(text.substr(0, 1279));
    if (!server || !server->IsObjectAlive(m_hObject) || text.empty() || text.size() > 1279 ||
        text.find('\0') != text.npos)
        return TriggerFailure("AI trigger owner or bounded text unavailable");
    Busy busy(trigger_busy);
    if (same(text, "TRIGGER") && !trigger_relay_target.empty())
    {
        if (trigger_relay_target.size() > 100 || trigger_relay_target.find('\0') != trigger_relay_target.npos)
            return TriggerFailure("AI relay target is invalid");
        auto* list = server->FindNamedObjects(trigger_relay_target.c_str());
        if (!list)
            return TriggerFailure("AI relay lookup failed");
        std::vector<HOBJECT> targets;
        for (auto* link = list->m_pFirstLink; link; link = link->m_pNext)
            targets.push_back(link->m_hObject);
        server->RelinquishList(list);
        for (auto target : targets)
        {
            if (!server->IsObjectAlive(target))
                continue;
            if (!server->IsObjectAlive(m_hObject))
                return TriggerFailure("AI relay owner removed during delivery");
            auto string = server->CreateString(trigger_text.c_str());
            auto message = server->StartMessageToObject(this, target, 1000 /* MID_TRIGGER */);
            if (!string || !message || server->WriteToMessageHString(message, string) != DE_OK)
            {
                if (string)
                    server->FreeString(string);
                if (message)
                    server->EndHMessageWrite(message);
                return TriggerFailure("AI relay message allocation failed");
            }
            server->FreeString(string);
            if (server->EndMessage(message) != DE_OK)
                return TriggerFailure("AI relay delivery failed");
            ++trigger_relays;
            if (trigger_outcome == TriggerOutcome::Failed)
                return false;
        }
        trigger_outcome = TriggerOutcome::Relay;
        return true; // Zero name matches is the source's valid empty delivery.
    }
    std::vector<std::vector<std::string>> commands;
    if (!OpenBlood2::model_commands(text, commands) || commands.empty())
        return TriggerFailure("Malformed AI trigger command list");
    for (const auto& args : commands)
    {
        if (!server->IsObjectAlive(m_hObject))
            return TriggerFailure("AI removed during command dispatch");
        ++trigger_commands;
        const auto& command = args.front();
        if (same(command, "SOUND_TRIGGER"))
        {
            int kind;
            if (args.size() < 2 || !number(args[1], kind))
                return TriggerFailure("Invalid sound-trigger identifier");
            ++hearing_events;
            heard_sound = kind;
            heard_sender = sender;
            heard_distance = 0;
            heard_position = {};
            trigger_outcome = TriggerOutcome::Ignored;
            auto ignore = [&] { ++hearing_ignored; };
            if (sender == m_hObject)
            {
                ignore();
                continue;
            }
            if (m_nState == StateScript && !(m_dwScriptFlags & (1u << 1)))
            {
                ignore();
                return true;
            } // Original early return skips later commands.
            if (kind == 0 || kind == 2)
            {
                if (m_nState != StateIdle)
                {
                    ignore();
                    continue;
                }
                DVector origin, source;
                if (!server->IsObjectAlive(sender) || server->GetObjectPos(sender, &source) != DE_OK ||
                    server->GetObjectPos(m_hObject, &origin) != DE_OK || !std::isfinite(m_fHearingDist) ||
                    m_fHearingDist < 0)
                    return TriggerFailure("Hearing requires live positions and a finite nonnegative range");
                const auto delta = OpenBlood2::vector_subtract(source, origin);
                heard_position = source; // Gunfire/player sounds use sender position, not message coordinates.
                heard_distance = std::hypot(delta.x, delta.y, delta.z);
                if (!std::isfinite(heard_distance))
                    return TriggerFailure("Hearing distance overflow");
                if (heard_distance > m_fHearingDist)
                {
                    ignore();
                    continue;
                }
                // Preserve the source's reversed IsKindOf argument order, even for subclass callers.
                if (kind == 0 && !server->IsKindOf(server->GetClass("CPlayerObj"), server->GetObjectClass(sender)))
                {
                    ignore();
                    continue;
                }
                if (kind == 0 || heard_distance < m_fHearingDist / 2)
                {
                    m_vTargetPos = source;
                    target_position_valid = true; // Cached location only; no target/link acquisition.
                    if (!SetNewState(StateSearchVisual))
                        return TriggerFailure(state_error.c_str());
                    trigger_outcome = TriggerOutcome::Heard;
                }
                else
                {
                    if (!MC_FacePos(source) || !SetNewState(StateIdle))
                        return TriggerFailure("Hearing face/state service failed");
                    trigger_outcome = TriggerOutcome::Turned;
                }
                ++hearing_reactions;
            }
            else if (kind == 1)
            {
                if (!(m_dwFlags & 8u) || m_nState == StateDodge || m_nLastState == StateDodge ||
                    m_nLastState == StatePassive)
                {
                    ignore();
                    continue;
                }
                DVector point;
                if (args.size() < 5 || !number(args[2], point.x) || !number(args[3], point.y) ||
                    !number(args[4], point.z) || !OpenBlood2::physics_vector(point))
                    return TriggerFailure("Invalid gun-impact coordinates");
                m_vTrackObjPos = heard_position = point; // Source does not apply hearing distance to impacts.
                tracked_position_valid = false;          // This is an impact location, not a tracked-object sample.
                if (!SetNewState(StateDodge))
                    return TriggerFailure(state_error.c_str());
                trigger_outcome = TriggerOutcome::Heard;
                ++hearing_reactions;
            }
            else
                ignore();
        }
        else if (same(command, "SETSTATE"))
        {
            int state;
            if (args.size() < 2 || !StateStrToInt(args[1], state) || !SetNewState(state))
                return TriggerFailure("AI SETSTATE request failed");
            trigger_outcome = TriggerOutcome::State;
            return true;
        }
        else if (same(command, "TARGET"))
        {
            if (args.size() < 2 || !SetTarget(args[1]) || !SetNewState(StateAttackFar))
                return TriggerFailure("AI TARGET lookup, facing or transition failed");
            trigger_outcome = TriggerOutcome::Target;
            return true;
        }
        else if (same(command, "PLAYANIMATION"))
        {
            if (args.size() < 2)
                return TriggerFailure("AI PLAYANIMATION requires a clip name");
            if (!PlayAnimation(args[1]))
                return false;
            trigger_outcome = TriggerOutcome::Animation;
            return true;
        }
        else if (same(command, "PLAYSOUND"))
        {
            int radius = 5000;
            if (args.size() < 2 || args[1].empty() || args[1].size() > 255 ||
                (args.size() > 2 && !number(args[2], radius)))
                return TriggerFailure("Invalid AI PLAYSOUND parameters");
            if (radius <= 0)
                radius = 1000;
            OpenBlood2::SoundRequest request;
            request.resource = args[1];
            request.speaker = m_hObject;
            request.outer_radius = radius;
            request.inner_radius = radius * .1f;
            request.priority = 2; // SOUNDPRIORITY_MISC_HIGH
            std::string error;
            if (!server->PlayCharacterSound(m_hObject, request, false, error))
                return TriggerFailure(error.c_str());
            trigger_outcome = TriggerOutcome::Sound;
            return true;
        }
        else if (same(command, "SCRIPT"))
        {
            if (npc_type == NPCType::Diagnostic)
                return TriggerFailure("SCRIPT requires an initialized production AI_Update driver");
            if (!RuntimeReady() || !PrepareScript(text) || !AI_Update())
                return TriggerFailure("SCRIPT preparation or forced AI_Update failed");
            return true;
        }
        else
            return TriggerFailure("Unknown AI trigger command");
    }
    return true; // Typed acceptance, unlike the source's false even after a successful hearing action.
}
DDWORD AI_Mgr::ObjectMessageFn(HOBJECT sender, DDWORD message, HMESSAGEREAD read)
{
    if (message == 1000 /* MID_TRIGGER */)
    {
        auto* server = GetServerDE();
        auto string = server->ReadFromMessageHString(read);
        const char* data = string ? server->GetStringData(string) : nullptr;
        const std::string text = data ? data : "";
        if (string)
            server->FreeString(string); // Own the text across nested delivery/removal.
        HandleTrigger(sender, text);
    }
    return CBaseCharacter::ObjectMessageFn(sender, message, read);
}
