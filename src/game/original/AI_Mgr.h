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
        FireBlocked,
        ExtraAction
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

// Selected original AI services and production Cultist/Civilian lifecycle.
// See ai_runtime_contracts.md for remaining state/action failures.
// Selected CanSee and explicit target/facing commands are documented in
// ai_sight_contracts.md and ai_target_contracts.md.
class AI_Mgr : public CBaseCharacter
{
  public:
    AI_Mgr();
    enum class NPCType
    {
        Diagnostic,
        Cultist,
        Civilian,
        Gideon
    };
    static void ResetRuntimeTables();
    using RuntimeTables = std::array<std::shared_ptr<CAnim_Sound>, 5>;
    static void ExchangeRuntimeTables(RuntimeTables&);
    void ConfigureRuntime(NPCType); // Original constructor defaults, selected by production factories only.
    bool AI_Update();
    bool RuntimeReady() const
    {
        return runtime_initialized && !runtime_failed;
    }
    NPCType npc_type = NPCType::Diagnostic;
    bool runtime_initialized = false, runtime_failed = false, m_bSetShutdown = false, m_bRemoveMe = false;
    bool npc_male = true, npc_labtech = false, npc_clown = false, npc_robe = false, npc_sorority = false;
    float m_fLastUpdate = 0, npc_hp = 30, npc_random_hp = 0, npc_armor = 0;
    std::array<std::string, 5> npc_weapons{"NONE", "NONE", "NONE", "NONE", "NONE"};
    std::array<float, 10>
        npc_ammo{}; // Source bullets, BMG, shells, grenades, rockets, flares, charges, cells, fuel, prox.
    std::string spot_target, spot_message, runtime_error;
    std::uint64_t runtime_updates = 0, runtime_failures = 0, runtime_camera_pauses = 0, runtime_desyncs = 0;
    std::uint64_t runtime_target_losses = 0;
    bool RuntimeFailure(std::string_view);

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
    bool MC_Taunt_Beg();
    bool MC_Taunt_Bold();
    virtual bool AI_STATE_Escape_RunAway();
    virtual bool AI_STATE_Escape_Hide();
    bool AI_STATE_GuardLocation();
    bool SetTrackedObject(HOBJECT);
    bool RefreshTrackedPosition();
    HOBJECT GetTrackedObject() const;
    DVector m_vGuardPos{};
    float m_fRollSpeed = 2;
    bool tracked_position_valid = false;
    std::uint64_t tracked_changes = 0, tracked_invalidations = 0, escape_updates = 0;
    float escape_hint_dot = 0;
    bool MC_Jump();
    bool MC_Crouch();
    bool MC_Crawl();
    bool MC_Fire_Crawl();
    bool AI_STATE_JumpOverObj();
    bool AI_STATE_CrawlUnderObj();
    bool AI_STATE_WalkAroundObj();
    bool AI_STATE_RunAroundObj();
    bool AI_STATE_WalkToPos();
    bool AI_STATE_RunToPos();
    bool AI_STATE_SearchVisualTarget();
    bool AI_STATE_SearchSmellTarget();
    bool AI_STATE_FindAmmo();
    void ReceiveAmmo(HOBJECT, HMESSAGEREAD);
    std::uint64_t ammo_searches = 0, ammo_collections = 0, ammo_refusals = 0, ammo_target_losses = 0;
    DBYTE searched_ammo = 0;
    std::string searched_ammo_class;
    HOBJECT searched_ammo_target = nullptr; // Last selected identity; may outlive the FindAmmo state.
    HOBJECT last_ammo_source = nullptr;     // Checked historical identity, never ownership.
    bool SetSmellCursor(HOBJECT);
    HOBJECT smell_cursor = nullptr;
    DVector smell_position{};
    std::uint64_t smell_searches = 0, smell_advances = 0, smell_invalidations = 0;
    int smell_exit = 0; // Last call: 1 no target, 2 other stimulus, 3 no cursor, 4 no visible hint.

    DVector m_vDestPos{};
    float m_fJumpSpeed = 500;
    int navigation_state = StateInactive;
    float navigation_distance = 0, navigation_threshold = 0;
    std::uint64_t navigation_updates = 0, navigation_arrivals = 0, navigation_failures = 0;
    std::string navigation_error;
    bool navigation_plan_failed = false;
    bool MC_Walk();
    bool MC_Run();
    bool MC_Fire_Walk();
    bool MC_Fire_Run();
    bool CheckObstructed(DVector direction, float speed, bool&);
    int CalculateObstruction(); // Flags, or -1 for a required query failure.
    bool NavigateObstacle();
    bool IsLedge(DVector direction, bool&);
    virtual bool AI_STATE_Dodge();
    int CalculateDodge(DVector);
    bool MC_Roll_Left();
    bool MC_Roll_Right();
    int m_nDodgeFlags = 0;
    std::uint64_t dodge_updates = 0, dodge_queries = 0;
    virtual bool AI_STATE_AttackClose();
    virtual bool AI_STATE_AttackFar();
    struct ObstacleRay
    {
        DVector from{}, to{};
        OpenBlood2::SegmentHit hit;
        OpenBlood2::SegmentResult result = OpenBlood2::SegmentResult::Unavailable;
    };
    ObstacleRay dodge_ray;
    std::array<ObstacleRay, 9> obstacle_rays;
    ObstacleRay ledge_ray;
    unsigned block_flags = 0;
    float last_ledge_check = 0, obstruction_velocity = 0, obstruction_vertical = 0;
    bool obstructed = false, ledge = false, ledge_sampled = false;
    std::uint64_t obstruction_checks = 0, obstacle_queries = 0, ledge_queries = 0;
    std::uint64_t locomotion_calls = 0, locomotion_failures = 0;
    int locomotion_command = 0;
    std::string locomotion_error;
    bool HandleDamage();
    bool MC_Recoil();
    bool MC_Dead();
    bool AI_STATE_Dying();
    bool CreateCorpse();
    bool StartBurnDeath();
    bool CreateDeathGibs();
    float m_fAIMass = 250; // Original AI_DEFAULT_MASS, distinct from aggregate overrides.
    HOBJECT fire_source = nullptr;
    std::uint64_t gib_events = 0, limbs_created = 0, big_gib_voice_requests = 0;
    int big_gib_voice_roll = -1;
    std::uint64_t combat_voice_deliveries = 0;
    std::string gib_error;
    DDWORD m_nCorpseType = 0;
    OpenBlood2::SoundId m_hCurSound = 0; // Original per-actor retained sound, distinct from shared table voice.
    int death_slot = -1, kill_voice_roll = -1;
    std::uint64_t death_starts = 0, corpses_created = 0, kill_voice_requests = 0;
    HOBJECT last_corpse = nullptr; // Non-owning; the corpse survives its original actor.
    OpenBlood2::BodyPlacement corpse_placement;
    std::string death_error;
    virtual bool AI_STATE_Recoil();
    std::uint64_t damage_reactions = 0, recoil_starts = 0;
    int recoil_node = -1, recoil_slot = -1;
    std::string damage_reaction_error;
    bool MC_Idle();
    bool MC_BestWeapon();
    virtual bool MC_Fire_Stand();
    bool CheckClearShot(DVector origin, DVector& direction, bool alternate = false);
    virtual bool Fire(bool alternate = false); // Source acceptance includes dry attempts; inspect fire_result.
    virtual bool MC_Extra(std::string_view);
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
    // Original state dispatch, also used by the production AI_Update loop.
    bool DispatchState();
    virtual bool ComputeState(int stimulus = -1);
    virtual bool AI_STATE_Idle();
    bool AI_STATE_Passive();
    virtual bool AI_STATE_Teleport();
    virtual bool AI_STATE_Special(int); // Consolidates the eight original virtual handlers.
    int m_nLastState = StateIdle, m_nCurMetacmd = 1, m_nTrapped = 0;
    DDWORD m_nCurAnimation = 999;
    bool m_bAnimating = false, m_bHasTargeted = false, animation_blocked = false;
    float m_fWaitForIdleTime = 0;
    DVector m_vDims{1, 1, 1}, m_vScale{1, 1, 1};
    std::string m_szAIState = "IDLE";
    std::uint64_t state_changes = 0, state_dispatches = 0, state_failures = 0;
    std::uint64_t animation_starts = 0, animation_finishes = 0;
    std::uint64_t animation_resize_blocks = 0; // Clip continued; physical bounds were retained.
    DVector animation_requested_dims{};        // Last different-clip resize request, before collision.
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
    enum class ScriptFireMode
    {
        None,
        Stand,
        Walk,
        Run
    };
    bool Script_Fire_Stand(bool& engaged);
    bool Script_Fire_Walk();
    bool Script_Fire_Run();
    ScriptFireMode script_fire_mode = ScriptFireMode::None;
    bool script_fire_engaged = false;
    std::uint64_t script_fire_calls = 0;
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
    // Diagnostic callers can prepare scripts separately; production triggers
    // prepare them and force the original immediate AI_Update.
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
    CAnim_Sound* AnimSound()
    {
        return m_pAnim_Sound.get();
    }
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
    virtual bool OnAcceptedDamage()
    {
        return true;
    }
    bool TriggerFailure(const char*);
    bool StateFailure(const char*);
    bool AnimationCommand(DDWORD clip, int command);
    bool EscapeRun(bool civilian);
    bool EscapeHide();
    bool OnStringKey(ArgList*) override;
    DDWORD EngineMessageFn(DDWORD, void*, DFLOAT) override;

  private:
    bool RuntimePrecreate(ObjectCreateStruct*, int);
    bool RuntimeInitialize(int);
    bool RuntimeUpdateStep();
    bool runtime_busy = false;
    bool locomotion_busy = false;
    bool LocomotionReady();
    bool TauntMovement(unsigned slot, int command);
    bool RollMovement(bool right);
    bool LocomotionFailure(std::string_view);
    bool Locomotion(bool run, bool fire, bool crawl = false);
    bool navigation_busy = false;
    bool NavigationState(int);
    bool NavigationStep(int);
    bool EscapeRunStep(bool civilian);
    bool EscapeHideStep();
    bool GuardStep();
    bool SmellStep();
    bool AmmoStep();
    bool NavigationDecision(int stimulus = -1);
    bool NavigationFailure(std::string_view);
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
    bool ScriptFire(ScriptFireMode, bool& engaged);
    bool FaceScriptPoint(DVector);
    bool DeliverPathAction(const PathListData&);
    DLink m_Link{};
    bool registered_cabal = true;
    static void InitLists();
    int PerceptionFailure(const char*);
    HOBJECT m_hTarget = nullptr;
    HOBJECT m_hTrackObject = nullptr;
    bool TargetFailure(const char*);
    // Original subclasses share class/gender tables and their retained voice.
    // Shared ownership replaces a borrowed pointer, preserving sharing safely.
    std::shared_ptr<CAnim_Sound> m_pAnim_Sound;
};
