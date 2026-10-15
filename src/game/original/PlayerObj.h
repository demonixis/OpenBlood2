#pragma once
#include "B2BaseClass.h"
#include "InventoryMgr.h"
#include "dlink.h"
#include <array>
#include <random>
#include <string>
#include <vector>

// Selective source port of the entry/lifetime portions of PlayerObj,
// BaseCharacter, Destructable and InventoryMgr. See PLAYER_CONTRACTS.md and
// M2_PLAYER_SOURCES.json. Inventory/vector weapons are selected separately;
// locomotion and containers are selected separately; complete combat remains outside this port.
class CDestructable : public Aggregate
{
  public:
    CDestructable();
    ~CDestructable() override;
    DBOOL Init(HOBJECT, CInventoryMgr*);
    void SetHitPoints(float hp)
    {
        m_fHitPoints = hp;
        if (hp)
            m_bDead = DFALSE;
    }
    void SetArmorPoints(float armor)
    {
        m_fArmorPoints = armor;
    }
    bool IsDead() const
    {
        return m_bDead != 0;
    }
    void CalculateResistance(DBYTE);
    HOBJECT m_hObject = nullptr;
    CInventoryMgr* m_pInventoryMgr = nullptr;
    DLink m_Link{};
    static DLink m_DestructableHead;
    static DDWORD m_dwNumDestructables;
    DBOOL m_bDead = DFALSE;
    float m_fHitPoints = 1, m_fArmorPoints = 0, m_fMass = 1, m_fResistance = 1;
    bool m_bGodMode = false;
    float m_fLastDamageAmount = 0;
    OpenBlood2::Damage last_damage;

  protected:
    DDWORD ObjectMessageFn(LPBASECLASS, HOBJECT, DDWORD, HMESSAGEREAD) override;

  public:
    float m_fMaxHitPoints = 1, m_fMaxMegaHitPoints = 1, m_fMaxArmorPoints = 1, m_fMaxNecroArmorPoints = 1;
};
class CBaseCharacter : public B2BaseClass
{
  public:
    CBaseCharacter();
    CInventoryMgr m_InventoryMgr;
    CDestructable m_damage;
    // Retain unhandled model commands for inspection, never execute them as
    // though sound/weapon/character animation-key services were available.
    std::vector<std::string> pending_model_commands;

  protected:
    DDWORD EngineMessageFn(DDWORD, void*, DFLOAT) override;
};
class CPlayerObj : public CBaseCharacter
{
  public:
    ~CPlayerObj() override;
    void PostPropRead(ObjectCreateStruct*);
    DBOOL InitialUpdate(int);
    DBOOL Update(DVector*);
    bool Respawn();
    bool GoToStartPoint(HSTRING start_name);
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
        FireCrawl
    };
    MoveState move_state = MoveState::None;
    bool crouch_requested = false, crouched = false, forced_crouch = false;
    float eye_level = 36;
    std::string locomotion_clip, locomotion_requested_clip, locomotion_error;
    std::uint64_t animation_changes = 0, blocked_resizes = 0;
    bool OnCommandUse();
    bool movement_blocked = false;
    bool door_push_pending = false;
    DVector door_push_velocity{};
    std::uint64_t door_pushes = 0;
    void ObjectTouchDoor(HOBJECT);
    bool ApplyDoorPush();
    DVector use_from{}, use_to{};
    OpenBlood2::SegmentHit use_hit;
    OpenBlood2::SegmentResult use_result = OpenBlood2::SegmentResult::Miss;
    std::uint64_t use_commands = 0, use_messages = 0;
    bool use_door_refused = false;
    bool UpdateClientPhysics();
    bool UpdateStats();
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
    DBYTE m_nCharacter = 0, m_nSkin = 0;
    DBYTE m_nAttribStrength = 1, m_nAttribSpeed = 1, m_nAttribResistance = 1, m_nAttribMagic = 1;
    DBOOL m_bFirstUpdate = DTRUE, m_bDead = DFALSE;

  protected:
    DDWORD EngineMessageFn(DDWORD, void*, DFLOAT) override;
    DDWORD ObjectMessageFn(HOBJECT, DDWORD, HMESSAGEREAD) override;

  private:
    OpenBlood2::PlayerStats last_sent_stats_{-1, -1, -1};
    std::minstd_rand idle_random_{1};
    float animation_start_ = 0;
    HCLIENT m_hClient = nullptr;
    HSTRING m_hstrTrigger = nullptr, m_hstrPlayerName = nullptr;
    HOBJECT initial_smell_ = nullptr;
};
