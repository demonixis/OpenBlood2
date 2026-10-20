// Original AI_Mgr.cpp:6759-6938 Save/Load with AIScriptList.h:52-82,211-241 (AISCRIPTCMD and
// CAIScriptList) and PathList.h:167-201 with PathListData.cpp:85-115, then the port's checked tail.
// Every field is decoded and checked before a member changes. See save_load_contracts.md (lot 11).
#include "AI_Mgr.h"
#include "SaveFields.h"
#include "SmellHint.h"
#include "engine/collision.h"
#include <cmath>

using namespace OpenBlood2;
namespace F = OpenBlood2::save_fields;
namespace
{
    // AISCRIPTCMD::args is char[MAX_AI_ARGS_LENGTH = 50]; ParseScript keeps names and arguments
    // to 49 bytes. A SCRIPT message is at most 1,279 bytes, so far fewer commands than this.
    constexpr std::size_t max_args = 49, max_script_commands = 1024;
    constexpr const char* command_names[]{
        "DONE", "SETMOVEMENT",   "FOLLOWPATH",           "PLAYSOUND",   "SETSTATE", "TARGET",
        "WAIT", "PLAYANIMATION", "PLAYANIMATIONLOOPING", "MOVETOOBJECT"};
    bool valid_state(int state)
    {
        return (state >= 0 && state <= 24) || (state >= 100 && state <= 108) || state == AI_Mgr::StateInactive;
    }
    bool range(float value)
    {
        return std::isfinite(value) && value >= 0 && value <= 1e7f;
    }
    // m_nNumItems and m_nScriptCmdIndex are written as (DFLOAT); exact up to 2^24.
    bool float_count(float value, std::size_t limit, std::size_t& out)
    {
        if (!std::isfinite(value) || value < 0 || value > float(limit) || value != std::floor(value))
            return false;
        out = static_cast<std::size_t>(value);
        return true;
    }
    void write_command(HMESSAGEWRITE h, const AI_Mgr::ScriptInstruction& command)
    {
        g_pServerDE->WriteToMessageByte(h, static_cast<DBYTE>(command.command));
        F::write_string(h, command.argument); // Empty args are the original NULL HSTRING.
    }
    bool read_command(HMESSAGEREAD h, AI_Mgr::ScriptInstruction& out)
    {
        const DBYTE command = g_pServerDE->ReadFromMessageByte(h);
        std::string argument;
        const bool text = F::read_string(h, argument, max_args);
        if (!text || command >= std::size(command_names))
            return false;
        // Unknown names were saved as the original DONE mapping; the canonical name replaces them.
        out = {static_cast<AI_Mgr::ScriptCommand>(command), command_names[command], std::move(argument)};
        return true;
    }
} // namespace

void AI_Mgr::Save(HMESSAGEWRITE h, DDWORD)
{
    auto* s = GetServerDE();
    if (runtime_busy || script_busy || motion_busy || navigation_busy || locomotion_busy || trigger_busy)
    {
        s->RejectPayload(h, "AI save during an AI callback");
        return;
    }
    const float now = s->GetTime();
    const auto vector = [&](const DVector& v) { s->WriteToMessageVector(h, &v); };
    const auto real = [&](float v) { s->WriteToMessageFloat(h, v); };
    const auto dword = [&](DDWORD v) { s->WriteToMessageDWord(h, v); };
    const auto byte = [&](unsigned v) { s->WriteToMessageByte(h, static_cast<DBYTE>(v)); };
    // A pending or removed identity is written as the NULL sentinel, like any target outside the list.
    s->WriteToLoadSaveMessageObject(h, m_hTarget);
    vector(m_vTargetPos);
    s->WriteToLoadSaveMessageObject(h, m_hTrackObject);
    vector(m_vTrackObjPos);
    vector(m_vGuardPos);
    vector(m_vDestPos);
    dword(static_cast<DDWORD>(m_nState));
    dword(static_cast<DDWORD>(m_nLastState));
    for (float stimulus : m_fStimuli)
        real(stimulus);
    dword(static_cast<DDWORD>(m_nDodgeFlags));
    F::write_string(h, spot_target);
    F::write_string(h, spot_message);
    F::write_string(h, trigger_relay_target);
    for (float value : {m_fHearingDist, m_fSmellingDist, m_fSensingDist, m_fSeeingDist, m_fWalkSpeed, m_fRunSpeed,
                        m_fJumpSpeed, m_fRollSpeed})
        real(value);
    real(m_fTimeStart - now); // Relative, as the original.
    real(m_fLoadTimeStart - now);
    dword(m_nCorpseType);
    real(m_fAttackLoadTime);
    byte(m_bAnimating);
    byte(0); // AI_Mgr::m_bJumping: only the original MC_Fire_Jump sets it, and no port caller has one.
    dword(static_cast<DDWORD>(Metacmd));
    dword(static_cast<DDWORD>(m_nCurMetacmd));
    dword(m_dwFlags);
    dword(block_flags);
    byte(m_bSetShutdown);
    real(m_fLastUpdate); // Raw absolute; the original Load adds GetTime() to it.
    dword(static_cast<DDWORD>(m_nInjuredLeg));
    s->WriteToLoadSaveMessageObject(h, SavedEnemyAttach());
    vector(m_vScale);
    real(last_ledge_check); // Raw absolute.
    dword(m_nCurAnimation);
    vector(m_vDims);
    byte(0); // m_bStartFire: constructor FALSE, never assigned by the original.
    real(static_cast<float>(script_index));
    real(script_wait_end); // Raw absolute, valid because a full restore keeps the clock.
    dword(m_dwScriptFlags);
    byte(m_eScriptMovement);
    byte(m_bUpdateScriptCmd);
    write_command(h, script_current);
    real(static_cast<float>(script_commands.size()));
    for (const auto& command : script_commands)
        write_command(h, command);
    real(static_cast<float>(script_path.size()));
    byte(0); // m_bDeleteElements: the constructor's Init(DFALSE).
    for (const auto& point : script_path)
    {
        vector(point.position);
        s->WriteToMessageString(h, point.name.c_str());
        F::write_nullable(h, point.action_target ? point.action_target->c_str() : nullptr);
        F::write_nullable(h, point.action_message ? point.action_message->c_str() : nullptr);
    }
    // OpenBlood2 tail: port state the original keeps in links or loses (save_load_contracts lot 11).
    const auto attacker = m_damage.LastDamagerIdentity();
    s->WriteToLoadSaveMessageObject(h, attacker);
    s->WriteToLoadSaveMessageObject(h, smell_cursor);
    s->WriteToLoadSaveMessageObject(h, searched_ammo_target);
    byte(m_bRemoveMe);
    byte(target_position_valid);
    byte(tracked_position_valid);
    byte(navigation_plan_failed);
    byte(static_cast<unsigned>(script_outcome));
    byte(static_cast<unsigned>(script_motion_command));
    byte(runtime_failed);
    F::write_string(h, runtime_failed ? runtime_error : std::string());
}

void AI_Mgr::Load(HMESSAGEREAD h, DDWORD)
{
    auto* s = GetServerDE();
    const float now = s->GetTime();
    std::string why;
    auto check = [&](bool condition, const char* reason)
    {
        if (!condition && why.empty())
            why = reason;
    };
    const auto vector = [&]()
    {
        DVector v;
        s->ReadFromMessageVector(h, &v);
        return v;
    };
    const auto real = [&]() { return s->ReadFromMessageFloat(h); };
    const auto dword = [&]() { return s->ReadFromMessageDWord(h); };
    const auto boolean = [&](bool& out) { check(F::read_bool(h, out), "boolean is not 0 or 1"); };

    HOBJECT target = nullptr, track = nullptr, attach = nullptr;
    s->ReadFromLoadSaveMessageObject(h, &target);
    const auto target_pos = vector();
    s->ReadFromLoadSaveMessageObject(h, &track);
    const auto track_pos = vector(), guard = vector(), dest = vector();
    const int state = static_cast<int>(dword()), last_state = static_cast<int>(dword());
    std::array<float, 6> stimuli;
    for (auto& value : stimuli)
        value = real();
    const int dodge = static_cast<int>(dword());
    std::string spot, message, relay;
    check(F::read_string(h, spot) && F::read_string(h, message) && F::read_string(h, relay), "trigger string");
    float hearing = real(), smelling = real(), sensing = real(), seeing = real();
    float walk = real(), run = real(), jump = real(), roll = real();
    const float time_start = real(), load_time_start = real();
    const DDWORD corpse = dword();
    const float attack_load = real();
    bool animating = false, jumping = false, shutdown = false, start_fire = false, update_command = false;
    boolean(animating);
    boolean(jumping);
    const int metacmd = static_cast<int>(dword()), cur_metacmd = static_cast<int>(dword());
    const DDWORD flags = dword(), blocks = dword();
    boolean(shutdown);
    const float last_update = real();
    const int injured = static_cast<int>(dword());
    s->ReadFromLoadSaveMessageObject(h, &attach);
    const auto scale = vector();
    const float ledge = real();
    const DDWORD animation = dword();
    const auto dims = vector();
    boolean(start_fire);
    std::size_t index = 0;
    check(float_count(real(), max_script_commands, index), "script command index");
    const float wait_end = real();
    const DDWORD script_flags = dword();
    const DBYTE movement = s->ReadFromMessageByte(h);
    boolean(update_command);
    ScriptInstruction current;
    check(read_command(h, current), "current script command");
    // A valid count keeps the reader aligned even after an earlier field was refused, so the
    // refusal names the first rejected field.
    std::size_t count = 0;
    const bool counted = float_count(real(), max_script_commands, count);
    check(counted, "script command count");
    std::vector<ScriptInstruction> commands(count);
    for (auto& command : commands)
        check(read_command(h, command), "script command");
    std::size_t points = 0;
    const bool listed = counted && float_count(real(), PathMgr::max_points, points);
    check(listed, "path point count");
    check(s->ReadFromMessageByte(h) == 0, "path list ownership flag");
    PathList path(points);
    for (auto& point : path)
    {
        point.position = vector();
        std::optional<std::string> name;
        check(F::read_nullable(h, name, 100) && name, "path point name");
        point.name = name.value_or("");
        check(F::read_nullable(h, point.action_target, 100) && F::read_nullable(h, point.action_message, 1279),
              "path point action");
        check(physics_vector(point.position), "path point position");
    }
    // OpenBlood2 tail.
    HOBJECT attacker = nullptr, smell = nullptr, ammo = nullptr;
    s->ReadFromLoadSaveMessageObject(h, &attacker);
    s->ReadFromLoadSaveMessageObject(h, &smell);
    s->ReadFromLoadSaveMessageObject(h, &ammo);
    bool remove = false, target_valid = false, track_valid = false, plan_failed = false, failed = false;
    boolean(remove);
    boolean(target_valid);
    boolean(track_valid);
    boolean(plan_failed);
    const DBYTE outcome = s->ReadFromMessageByte(h), motion = s->ReadFromMessageByte(h);
    boolean(failed);
    std::string failure;
    check(F::read_string(h, failure, 4096), "runtime error text");

    check(target != m_hObject && track != m_hObject && attacker != m_hObject && smell != m_hObject, "self reference");
    check(physics_vector(target_pos) && physics_vector(track_pos) && physics_vector(guard) && physics_vector(dest),
          "position vector");
    check(valid_state(state) && valid_state(last_state), "AI state");
    for (float value : stimuli)
        check(std::isfinite(value), "stimulus");
    for (float value : {hearing, smelling, sensing, seeing, walk, run, jump, roll})
        check(range(value), "distance or speed");
    check(std::isfinite(time_start) && std::isfinite(load_time_start) && std::isfinite(last_update) &&
              std::isfinite(ledge) && std::isfinite(wait_end) && range(attack_load),
          "clock");
    check(!jumping && !start_fire, "AI_Mgr jump or start-fire flag has no port state");
    check(!attach || AcceptsEnemyAttach(), "enemy attachment for a class that never attaches");
    check(physics_vector(scale) && scale.x > 0 && scale.y > 0 && scale.z > 0, "scale");
    check(physics_vector(dims) && dims.x >= 0 && dims.y >= 0 && dims.z >= 0, "dimensions");
    check(movement == SM_WALK || movement == SM_RUN, "script movement");
    check(index == 0 || index < count, "script command index past the list");
    check(outcome <= static_cast<DBYTE>(ScriptOutcome::Failed) &&
              motion <= static_cast<DBYTE>(MotionCommand::MoveToObject),
          "script outcome or motion command");
    check(!smell || dynamic_cast<SmellHint*>(s->HandleToObject(smell)), "smell cursor is not a SmellHint");
    if (!why.empty())
    {
        s->RejectPayload(h, "AI_Mgr Load: " + why);
        return;
    }
    // The cursor's link is the only fallible change; take it first. A SmellHint is always-load, so a
    // listed one never exists today and the cursor reads NULL, as in the original.
    if (smell && s->CreateInterObjectLink(m_hObject, smell) != DE_OK)
    {
        s->RejectPayload(h, "AI_Mgr Load: smell cursor link failed");
        return;
    }
    // The target's inter-object link comes back with the LINK section.
    m_hTarget = target;
    m_vTargetPos = target_pos;
    m_hTrackObject = track;
    m_vTrackObjPos = track_pos;
    m_vGuardPos = guard;
    m_vDestPos = dest;
    m_nState = state;
    m_nLastState = last_state;
    m_fStimuli = stimuli;
    m_nDodgeFlags = dodge;
    spot_target = std::move(spot);
    spot_message = std::move(message);
    trigger_relay_target = std::move(relay);
    m_fHearingDist = hearing;
    m_fSmellingDist = smelling;
    m_fSensingDist = sensing;
    m_fSeeingDist = seeing;
    m_fWalkSpeed = walk;
    m_fRunSpeed = run;
    m_fJumpSpeed = jump;
    m_fRollSpeed = roll;
    m_fTimeStart = F::restored_time(time_start, now);
    m_fLoadTimeStart = F::restored_time(load_time_start, now);
    m_nCorpseType = corpse;
    m_fAttackLoadTime = attack_load;
    m_bAnimating = animating;
    Metacmd = metacmd;
    m_nCurMetacmd = cur_metacmd;
    m_dwFlags = flags;
    block_flags = blocks;
    m_bSetShutdown = shutdown;
    m_fLastUpdate = last_update + now; // Original quirk (AI_Mgr.cpp:6912): a listed re-save rewrite.
    m_nInjuredLeg = injured;
    loaded_enemy_attach = attach;
    m_vScale = scale;
    // Departure (decision 4): the original adds GetTime() here too, which suppresses every ledge
    // check for about the saved game time. The raw deadline is valid under the restored clock.
    last_ledge_check = ledge;
    m_nCurAnimation = animation;
    m_vDims = dims;
    script_index = index;
    script_wait_end = wait_end;
    m_dwScriptFlags = script_flags;
    m_eScriptMovement = static_cast<AIScriptMovement>(movement);
    m_bUpdateScriptCmd = update_command;
    script_current = std::move(current);
    script_commands = std::move(commands); // The original appends to the list initial update left empty.
    script_path = std::move(path);
    loaded_attacker = attacker;
    smell_cursor = smell;
    searched_ammo_target = ammo;
    m_bRemoveMe = remove;
    target_position_valid = target_valid;
    tracked_position_valid = track_valid;
    navigation_plan_failed = plan_failed;
    script_outcome = static_cast<ScriptOutcome>(outcome);
    script_motion_command = static_cast<MotionCommand>(motion);
    runtime_failed = failed;
    runtime_error = std::move(failure);
    ++restored_loads;
}

void AI_Mgr::AfterLoad(HMESSAGEREAD)
{
    // The damage block's Load cleared the attacker; the port's tail brings the memory back.
    if (loaded_attacker)
        m_damage.RestoreLastDamager(loaded_attacker);
    loaded_attacker = nullptr;
}
