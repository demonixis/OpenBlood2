#pragma once
#include "PathMgr.h"
#include "PlayerObj.h"
#include <memory>

namespace OpenBlood2
{
    enum class AIFindList
    {
        Enemies,
        Smells,
        Exits,
        Ammo
    };
    enum AIFindFlags : unsigned
    {
        FindAvoidTarget = 1,
        FindVisible = 2,
        FindFaceObject = 4,
        FindSpecific = 8
    };
    struct AIFindReport
    {
        AIFindList list = AIFindList::Enemies;
        unsigned flags = 0, visited = 0, eligible = 0, rays = 0;
        float range = 0, distance = 0;
        DVector origin{};
        HOBJECT selected = nullptr;
        bool valid = false, origin_head = false;
        std::string error;
    };
    enum class AIKeyOutcome
    {
        None,
        Played,
        ChanceSkipped,
        VoiceBusy,
        CameraBlocked,
        ScriptBlocked,
        NoTarget,
        Unavailable,
        Invalid,
        PlaybackFailed,
        FireAttempt,
        FireBlocked
    };
    const char* ai_key_outcome_name(AIKeyOutcome);
    enum class AISightOutcome
    {
        None,
        Body,
        Head,
        Blocked,
        Miss,
        Invalid,
        Unavailable
    };
    const char* ai_sight_outcome_name(AISightOutcome);
    struct AISight
    {
        AISightOutcome outcome = AISightOutcome::None;
        HOBJECT target = nullptr;
        DVector origin{}, body{}, head{};
        SegmentHit body_hit, head_hit;
        SegmentResult body_result = SegmentResult::Miss, head_result = SegmentResult::Miss;
        bool origin_head = false, retried_head = false;
        unsigned rays = 0;
    };
    enum class AIClearOutcome
    {
        None,
        Clear,
        Target,
        Head,
        Blocked,
        Invalid,
        Unavailable
    };
    const char* ai_clear_outcome_name(AIClearOutcome);
    struct AIClearShot
    {
        AIClearOutcome outcome = AIClearOutcome::None;
        DVector origin{}, direction{}, head{};
        SegmentHit first_hit, head_hit;
        SegmentResult first_result = SegmentResult::Miss, head_result = SegmentResult::Miss;
        unsigned rays = 0;
        bool retried_head = false;
    };
} // namespace OpenBlood2

// Selective original AI_Mgr::InitStatics/OnStringKey port. No production AI class
// registration or complete AI_Update or MC_Extra implementation yet.
// Perception commands: AI_PERCEPTION_CONTRACTS.md; no automatic AI update loop.
// Selected CanSee and explicit target/facing commands are documented in
// AI_SIGHT_CONTRACTS.md and AI_TARGET_CONTRACTS.md.
class AI_Mgr : public CBaseCharacter
{
  public:
    AI_Mgr();
    ~AI_Mgr() override;
    bool InsertMe();
    static void InsertPlayer(CPlayerObj&);
    static void RemovePlayer(CPlayerObj&);
    static DLink m_CabalHead, m_MonsterHead;
    static unsigned m_dwNumCabal, m_dwNumMonster;
    bool m_bCabal = true;
    float m_fSeeingDist = 0, m_fSmellingDist = 0, m_fSensingDist = 0;
    std::array<float, 6> m_fStimuli{}; // sight, sound, smell, sense, threat, health
    HOBJECT FindObjectInRadius(OpenBlood2::AIFindList, float, unsigned flags = 0, HCLASS specific = nullptr);
    int ComputeStimuli(); // Original 0/1/3/4 results; -1 means a required query failed.
    bool ChooseOpportunityTarget();
    OpenBlood2::AIFindReport find;
    std::array<OpenBlood2::AIFindReport, 3> stimulus_queries;
    std::uint64_t perception_calls = 0, perception_failures = 0;
    int stimulus = 0;
    bool from_damage = false;
    std::string perception_error;
    // Original numeric state IDs. Selection does not imply an implemented action.
    enum State : int
    {
        StateAttackClose = 0,
        StateAttackFar = 1,
        StateSearchVisual = 2,
        StateEscapeHide = 3,
        StateEscapeRun = 4,
        StateIdle = 5,
        StateGuard = 6,
        StateFindHealth = 7,
        StateFindAmmo = 8,
        StateHide = 9,
        StateDodge = 10,
        StateTeleport = 11,
        StateEnemyAttach = 12,
        StateAssistAlly = 13,
        StateSearchSmell = 14,
        StateSpecial1 = 15,
        StateSpecial2 = 16,
        StateSpecial3 = 17,
        StateSpecial4 = 18,
        StateSpecial5 = 19,
        StateSpecial6 = 20,
        StateSpecial7 = 21,
        StateSpecial8 = 22,
        StateSpecial9 = 23,
        StatePassive = 24,
        StateDying = 100,
        StateScript = 101,
        StateRecoil = 102,
        StateJumpOver = 103,
        StateCrawlUnder = 104,
        StateWalkAround = 105,
        StateRunAround = 106,
        StateWalkTo = 107,
        StateRunTo = 108,
        StateInactive = 999
    };
    static bool StateStrToInt(std::string_view, int&);
    bool SetNewState(int);
    bool StopVelocity();
    bool SetAnimation(DDWORD);
    bool MC_Idle();
    bool MC_BestWeapon();
    bool MC_Fire_Stand();
    bool CheckClearShot(DVector origin, DVector& direction, bool alternate = false);
    bool Fire(bool alternate = false); // Source acceptance includes dry attempts; inspect fire_result.
    OpenBlood2::AIClearShot clear_shot;
    FireResult fire_result = FireResult::Invalid;
    DVector fire_origin{}, fire_direction{};
    bool fire_gun_node = false;
    std::uint64_t fire_calls = 0, fire_shots = 0, fire_dry = 0, fire_blocked = 0, fire_failures = 0;
    std::string fire_error;
    float weapon_distance = 0, m_fAttackLoadTime = 0, m_fLoadTimeStart = 0, m_fTimeStart = 0;
    DBYTE best_weapon = 0;

    bool MC_Talk();
    bool MC_Special(int);
    // Only the original state-dispatch portion, not AI_Update/deactivation/retention.
    bool DispatchState();
    virtual bool ComputeState(int stimulus = -1);
    virtual bool AI_STATE_Idle();
    bool AI_STATE_Passive();
    virtual bool AI_STATE_Special(int); // Consolidates the eight original virtual handlers.
    int m_nLastState = StateIdle, m_nCurMetacmd = 1, m_nTrapped = 0;
    DDWORD m_nCurAnimation = 999;
    bool m_bAnimating = false, m_bHasTargeted = false, animation_blocked = false;
    float m_fWaitForIdleTime = 0;
    DVector m_vDims{1, 1, 1}, m_vScale{1, 1, 1};
    std::string m_szAIState = "IDLE";
    std::uint64_t state_changes = 0, state_dispatches = 0, state_failures = 0;
    std::uint64_t animation_starts = 0, animation_finishes = 0;
    int requested_state = StateIdle, dispatched_state = StateIdle, animation_choice = -1;
    DDWORD requested_animation = UINT32_MAX;
    std::string state_error;

    enum AIScriptMovement
    {
        SM_WALK = 0,
        SM_RUN = 1
    };
    enum class MotionCommand
    {
        None,
        FollowPath,
        MoveToObject
    };
    enum class MotionOutcome
    {
        None,
        Selected,
        Moving,
        Arrived,
        Completed,
        Failed
    };
    bool Move(DVector direction, float speed);
    bool Script_Walk();
    bool Script_Run();
    bool SetSetMovementCmd(std::string_view);
    bool SetFollowPathCmd(PathMgr&, std::string_view);
    bool UpdateFollowPathCmd();
    bool SetMoveToObjectCmd(std::string_view);
    bool UpdateMoveToObjectCmd();
    std::span<const PathListData> ScriptPath() const
    {
        return script_path;
    }
    AIScriptMovement m_eScriptMovement = SM_WALK;
    float m_fWalkSpeed = 5, m_fRunSpeed = 10;
    int m_nInjuredLeg = 0;
    bool m_bUpdateScriptCmd = false;
    MotionCommand script_motion_command = MotionCommand::None;
    MotionOutcome script_motion_outcome = MotionOutcome::None;
    std::uint64_t script_motion_updates = 0, script_motion_failures = 0, velocity_commands = 0;
    std::uint64_t path_arrivals = 0, path_deliveries = 0, path_removals = 0;
    DVector script_motion_destination{}, script_motion_velocity{};
    float script_motion_distance = 0, script_motion_threshold = 0;
    std::string script_motion_name, script_motion_error;

    enum class ScriptCommand
    {
        Done,
        SetMovement,
        FollowPath,
        PlaySound,
        SetState,
        Target,
        Wait,
        PlayAnimation,
        PlayAnimationLooping,
        MoveToObject
    };
    struct ScriptInstruction
    {
        ScriptCommand command = ScriptCommand::Done;
        std::string name, argument; // Unknown names retain the original DONE mapping.
    };
    struct ScriptProgram
    {
        DDWORD flags = 0;
        std::vector<ScriptInstruction> commands;
    };
    enum class ScriptOutcome
    {
        None,
        Ready,
        Running,
        Completed,
        StateChanged,
        Failed
    };
    static bool ParseScript(std::string_view, DDWORD previous_flags, ScriptProgram&, std::string& error);
    // Preparation is separate from the original trigger's forced AI_Update,
    // which is not integrated. Explicit diagnostic callers drive this state.
    bool PrepareScript(std::string_view);
    bool AI_STATE_Script(PathMgr&);
    std::span<const ScriptInstruction> ScriptCommands() const
    {
        return script_commands;
    }
    const ScriptInstruction& CurrentScriptCommand() const
    {
        return script_current;
    }
    ScriptOutcome script_outcome = ScriptOutcome::None;
    std::size_t script_index = 0;
    float script_wait_end = 0;
    std::uint64_t script_selections = 0, script_updates = 0, script_failures = 0, script_sounds = 0;
    std::string script_error;

    enum class TriggerOutcome
    {
        None,
        Ignored,
        Heard,
        Turned,
        State,
        Target,
        Animation,
        Sound,
        Relay,
        Failed
    };
    bool HandleTrigger(HOBJECT sender, std::string_view text);
    bool PlayAnimation(std::string_view name);
    float m_fHearingDist = 0;
    DDWORD m_dwFlags = 0, m_dwScriptFlags = 0;
    DVector m_vTrackObjPos{};
    std::string trigger_relay_target;
    TriggerOutcome trigger_outcome = TriggerOutcome::None;
    std::uint64_t trigger_calls = 0, trigger_commands = 0, trigger_failures = 0;
    std::uint64_t hearing_events = 0, hearing_reactions = 0, hearing_ignored = 0, trigger_relays = 0;
    int heard_sound = -1;
    HOBJECT heard_sender = nullptr; // Non-owning historical identity; inspect through checked lookup.
    DVector heard_position{};
    float heard_distance = 0;
    std::string trigger_text, trigger_error;

    bool InitStatics(std::shared_ptr<CAnim_Sound>);
    bool CanSee(HOBJECT);
    OpenBlood2::AISight sight;
    std::uint64_t sight_queries = 0;
    const CAnim_Sound* AnimSound() const
    {
        return m_pAnim_Sound.get();
    }
    int m_nState = StateIdle;
    // Target assignment must go through the original inter-object link path.
    HOBJECT GetTarget() const
    {
        return m_hTarget;
    }
    bool SetNewTarget(HOBJECT);
    bool SetTarget(std::string_view name);
    bool RefreshTargetPosition();
    bool MC_FaceTarget();
    bool MC_FacePos(DVector);
    DVector m_vTargetPos{};
    int Metacmd = 1;
    std::uint64_t target_changes = 0, target_link_breaks = 0, target_failures = 0, face_commands = 0;
    std::string target_name, target_error;
    DVector face_position{};
    float face_yaw_delta = 0;
    bool target_position_valid = false;
    OpenBlood2::AIKeyOutcome key_outcome = OpenBlood2::AIKeyOutcome::None;
    std::uint64_t model_keys = 0, audio_keys = 0, audio_starts = 0, key_failures = 0;
    std::uint64_t camera_suppressed = 0, script_suppressed = 0, chance_suppressed = 0;
    std::uint64_t busy_voices = 0, no_target = 0, unavailable_keys = 0;
    int selected_variant = 0, chance_roll = -1;
    std::string ai_key, ai_key_error, ai_sound_resource;

  protected:
    DDWORD ObjectMessageFn(HOBJECT, DDWORD, HMESSAGEREAD) override;
    bool TriggerFailure(const char*);
    bool StateFailure(const char*);
    bool AnimationCommand(DDWORD clip, int command);
    bool OnStringKey(ArgList*) override;
    DDWORD EngineMessageFn(DDWORD, void*, DFLOAT) override;

  private:
    bool script_busy = false;
    std::vector<ScriptInstruction> script_commands;
    ScriptInstruction script_current;
    bool ScriptFailure(std::string_view);
    bool UpdateScriptCommand(PathMgr&);
    bool FinishScript();
    bool trigger_busy = false;
    bool motion_busy = false;
    PathList script_path;
    bool MotionFailure(const char*);
    bool MotionReady();
    bool ScriptLocomotion(bool run);
    bool FaceScriptPoint(DVector);
    bool DeliverPathAction(const PathListData&);
    DLink m_Link{};
    bool registered_cabal = true;
    static void InitLists();
    int PerceptionFailure(const char*);
    HOBJECT m_hTarget = nullptr;
    bool TargetFailure(const char*);
    // Original subclasses share class/gender tables and their retained voice.
    // Shared ownership replaces a borrowed pointer, preserving sharing safely.
    std::shared_ptr<CAnim_Sound> m_pAnim_Sound;
};
