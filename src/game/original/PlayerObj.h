#pragma once
#include "AISound.h"
#include "Anim_Sound.h"
#include "B2BaseClass.h"
#include "Destructable.h"
#include "InventoryMgr.h"
#include "Movement.h"
#include "SurfaceTypes.h"
#include "dlink.h"
#include "game/player_state.h"
#include <array>
#include <random>
#include <string>
#include <vector>

// Selective source port of the entry/lifetime portions of PlayerObj,
// BaseCharacter, Destructable and InventoryMgr. See player_contracts.md and
// m2_player_sources.json. Inventory/vector weapons are selected separately;
// locomotion and containers are selected separately; complete combat remains outside this port.
class CBaseCharacter : public B2BaseClass
{
  public:
    CBaseCharacter();
    DDWORD m_dwSurfType = SURFTYPE_FLESH;
    CInventoryMgr m_InventoryMgr;
    CDestructable m_damage;
    CMovement m_MoveObj;
    CAnim_Sound m_Anim_Sound;
    std::uint64_t motion_keys = 0, motion_actions = 0, motion_failures = 0;
    unsigned motion_applied = 0; // RotY, ForwVel, RightVel, UpVel, AddXYZ, DimsXYZ bits.
    std::string motion_key, motion_error;
    // Retain commands outside the selected original character/player key paths.
    // Recording a key never claims its missing service was executed.
    std::vector<std::string> pending_model_commands;

  protected:
    virtual bool OnStringKey(ArgList*);
    DDWORD EngineMessageFn(DDWORD, void*, DFLOAT) override;
};
enum PowerupType
{
    PU_NONE = -1,
    PU_INVULNERABLE,
    PU_INVISIBLE,
    PU_TRIPLEDAMAGE,
    PU_INCORPOREAL,
    PU_SILENT,
    PU_MAXTYPES
};
class CPlayerObj : public CBaseCharacter
{
  public:
    ~CPlayerObj() override;
    void ProcessTriggerMsg(HMESSAGEREAD);
    std::uint64_t trigger_messages = 0, fade_messages = 0, unsupported_triggers = 0, ignored_triggers = 0;
    std::string last_player_trigger, last_unsupported_player_trigger, player_trigger_error;
    bool AddRemoteBomb(HOBJECT);
    void RemoveRemoteBomb(HOBJECT);
    void ClearRemoteBombs();
    bool OnCommandDetonate();
    ObjectList* remote_bombs = nullptr;
    std::uint64_t remote_commands = 0, remote_signals = 0;
    std::string remote_error;
    // Original activation storage/query only; collection/timing is still unported.
    std::array<bool, PU_MAXTYPES> m_bPowerupActivated{};
    bool IsPowerupActive(int index) const
    {
        return index >= 0 && index < PU_MAXTYPES && m_bPowerupActivated[index];
    }
    bool PlayPlayerSound(std::string_view, float radius, DBYTE volume, bool stream, DBYTE priority = 2);
    OpenBlood2::SoundId m_hCurSound = 0;
    float m_fNextPingTime = 0;
    std::uint64_t activation_ping_failures = 0;
    bool m_bDisableFootsteps = false, m_bNextFootstepLeft = true;
    SurfaceType m_eLastSurface = SURFTYPE_UNKNOWN, footstep_surface = SURFTYPE_UNKNOWN;
    ContainerCode footstep_container = CC_NOTHING;
    bool GetFootstepSound(std::string&);
    bool GetLandingSound(std::string&);
    bool UpdateLandingSound();
    bool m_bFalling = false, m_bServerFalling = false;
    void ObjectTouch(HOBJECT, float force);
    std::uint64_t fall_damage_requests = 0, fall_damage_deliveries = 0;
    float fall_damage_force = 0, fall_damage_speed = 0, fall_damage_amount = 0;
    std::string fall_damage_error;
    float m_startFall = -1, m_fMinFallPos = 0, m_fMaxFallPos = 0;
    SurfaceType landing_surface = SURFTYPE_UNKNOWN;
    ContainerCode landing_container = CC_NOTHING;
    float landing_distance = 0;
    std::uint64_t landing_contacts = 0, landing_requests = 0, landing_starts = 0;
    std::string landing_resource, landing_error;
    bool PlayVoiceGroupEventOnClient(int event, bool single_player_only = false);
    bool PlayVoiceUniqueEventOnClient(int event);
    void PlayWeaponVoiceSoundOnClient(int weapon);
    static CPlayerObj* LocalVoicePlayer(); // Checked replacement for the source single-player global.
    void UpdateDamageVoice();
    std::uint32_t m_nLastHitPoints = 0, m_nLastDamage = 0;
    int damage_voice_roll = -1, damage_voice_event = -1;
    std::uint64_t damage_voice_checks = 0, damage_voice_requests = 0;
    std::string damage_voice_error;
    std::uint64_t weapon_pickup_voice_requests = 0, voice_messages = 0, voice_delivery_failures = 0;
    std::uint64_t trigger_sound_requests = 0, trigger_sound_starts = 0, trigger_voice_stops = 0;
    OpenBlood2::SoundId trigger_sound = 0; // Historical unretained local one-shot.
    std::string trigger_sound_resource, trigger_sound_error;
    std::uint64_t sound_keys = 0, sound_attempts = 0, sound_starts = 0, sound_failures = 0;
    std::uint64_t suppressed_footsteps = 0, unresolved_footsteps = 0, unavailable_sound_alerts = 0;
    OpenBlood2::SoundAlertReport sound_alert;
    std::uint64_t sound_alerts = 0;
    std::string sound_resource, sound_error;
    void UpdateSound();
    void PostPropRead(ObjectCreateStruct*);
    DBOOL InitialUpdate(int);
    DBOOL Update(DVector*);
    bool Respawn();
    void HandleDeath();
    bool CreateDeathCorpse();
    bool CreateDeathGibs();
    HOBJECT last_death_corpse = nullptr; // Non-owning, independent world lifetime.
    OpenBlood2::BodyPlacement death_placement;
    int death_voice_event = -1, death_slot = -1;
    std::uint64_t deaths = 0, death_messages = 0, death_corpses = 0, death_gibs = 0, death_limbs = 0;
    std::string death_error, death_effect_error;
    bool SaveFields(OpenBlood2::PlayerState&, std::string&) const;
    bool LoadFields(const OpenBlood2::PlayerState&, std::string&);
    bool RestoreModelState(std::string&);
    bool restore_model_loaded = false, restore_engine_loaded = false;
    std::string restore_model_error;
    bool restore_initialization = false, restore_fields_loaded = false;
    bool GoToStartPoint(HSTRING start_name);
    std::string entry_start_name; // Owned inspection of the actual selected marker.
    bool entry_start_fallback = false;
    bool SetCharacter(DBYTE);
    bool OnSinglePlayerInit(const OpenBlood2::PlayerInit&);
    void ValidateAttributes();
    FireResult HandleWeaponFireMessage(const OpenBlood2::WeaponFire&);
    bool HandlePlayerPositionMessage(const OpenBlood2::PlayerMove&);
    OpenBlood2::ContainerState containers;
    bool UpdateContainerState();
    bool UpdateAirLevel(float seconds);
    bool UpdateAnim(const OpenBlood2::PlayerControls&);
    enum class MoveState
    {
        None,
        Idle,
        Walk,
        Run,
        StrafeRight,
        StrafeLeft,
        Jump,
        Crouch,
        Crawl,
        FireStand,
        FireWalk,
        FireRun,
        FireJump,
        FireCrouch,
        FireCrawl,
        Dead = 16 // Source 15 is the unported multiplayer humiliation state.
    };
    MoveState move_state = MoveState::None;
    bool crouch_requested = false, crouched = false, forced_crouch = false;
    float eye_level = 36;
    std::string locomotion_clip, locomotion_requested_clip, locomotion_error;
    std::uint64_t animation_changes = 0, blocked_resizes = 0;
    bool OnCommandUse();
    bool OnCommandItem(DBYTE);
    bool command_run = false;
    bool movement_blocked = false, camera_movement_blocked = false;
    bool door_push_pending = false;
    DVector door_push_velocity{};
    std::uint64_t door_pushes = 0;
    bool ObjectTouchDoor(HOBJECT);
    bool ApplyDoorPush();
    DVector use_from{}, use_to{};
    OpenBlood2::SegmentHit use_hit;
    OpenBlood2::SegmentResult use_result = OpenBlood2::SegmentResult::Miss;
    std::uint64_t use_commands = 0, use_messages = 0;
    bool use_door_refused = false;
    bool UpdateClientPhysics();
    bool UpdateStats(bool force = false);
    bool InitializeClientState();
    bool TeleportClientToServerPos();
    void SetClient(HCLIENT client)
    {
        m_hClient = client;
        m_InventoryMgr.SetClient(client);
    }
    HCLIENT GetClient() const
    {
        return m_hClient;
    }
    bool initialized = false, attributes_received = false;
    std::uint64_t respawns = 0, updates = 0;
    std::uint64_t movement_messages = 0;
    std::uint64_t protected_volume_hits = 0;
    DBYTE m_byClientMoveCode = 0;
    bool m_bOnGround = false;
    std::string entry_error;
    float m_fYaw = 0, m_fPitch = 0, m_fMoveVel = 0, m_fJumpVel = 600;
    float m_fAirLevel = 100, m_fSmellTime = 0, m_fSmellDelay = 0.5f;
    bool UpdateSmellTrail();
    bool m_bSpectatorMode = false; // Guard storage only; spectator mode is not integrated.
    HOBJECT last_smell = nullptr;
    std::uint64_t smell_drops = 0, smell_checks = 0;
    float smell_distance = 0;
    std::string smell_error;
    DBYTE m_nCharacter = 0, m_nSkin = 0;
    DBYTE m_nAttribStrength = 1, m_nAttribSpeed = 1, m_nAttribResistance = 1, m_nAttribMagic = 1;
    DBOOL m_bFirstUpdate = DTRUE, m_bDead = DFALSE;

  protected:
    DDWORD EngineMessageFn(DDWORD, void*, DFLOAT) override;
    DDWORD ObjectMessageFn(HOBJECT, DDWORD, HMESSAGEREAD) override;
    bool OnStringKey(ArgList*) override;

  private:
    friend class AI_Mgr;
    DLink m_CabalLink{}, m_MonsterLink{};
    OpenBlood2::PlayerStats last_sent_stats_{-1, -1, -1};
    std::minstd_rand idle_random_{1};
    float animation_start_ = 0;
    HCLIENT m_hClient = nullptr;
    HSTRING m_hstrTrigger = nullptr, m_hstrPlayerName = nullptr;
    HOBJECT initial_smell_ = nullptr;
};
