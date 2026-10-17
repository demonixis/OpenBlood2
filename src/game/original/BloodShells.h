#pragma once
#include "CommLink.h"
#include "MoveMgr.h"
#include "PathMgr.h"
#include "PlayerObj.h"
#include "ViewWeapon.h"
#include "game/session.h"

// Original single-player entry branches adapted to the typed local boundary.
// These shells intentionally have no menu/audio/save/network facades.
class CBloodServerShell
{
  public:
    explicit CBloodServerShell(ServerDE& server) : paths(server), server_(server) {}
    void PreStartWorld();
    bool PostStartWorld();
    CPlayerObj* OnClientEnterWorld(HCLIENT);
    CPlayerObj* OnRestoredClientEnterWorld(HCLIENT, HOBJECT, const char* start_name = nullptr);
    void OnClientExitWorld(HCLIENT);
    bool OnMessage(HCLIENT, const OpenBlood2::PlayerInit&);
    FireResult OnMessage(HCLIENT, const OpenBlood2::WeaponFire&);
    bool OnMessage(HCLIENT, const OpenBlood2::PlayerMove&);
    bool OnMessage(HCLIENT, const OpenBlood2::PlayerControls&);
    bool OnMessage(HCLIENT, const OpenBlood2::PlayerUse&);
    bool OnMessage(HCLIENT, const OpenBlood2::PlayerItemCommand&);
    void Update(float);
    CPlayerObj* Player(HCLIENT);
    bool CapturePlayerInventory(HCLIENT);
    struct InventoryCapture
    {
        OpenBlood2::InventoryState state;
        std::string error;
        std::uint64_t attempts = 0, captures = 0;
        bool valid = false;
    } inventory_capture; // Inventory only; never a successful KeepAliveSave.
    bool CapturePlayerDamage(HCLIENT);
    struct DamageCapture
    {
        OpenBlood2::DestructableState state;
        std::string error;
        std::uint64_t attempts = 0, captures = 0;
        bool valid = false;
    } damage_capture; // Damage aggregate only; not complete player preservation.
    bool CapturePlayerFields(HCLIENT);
    struct PlayerCapture
    {
        OpenBlood2::PlayerState state;
        std::string error;
        std::uint64_t attempts = 0, captures = 0;
        bool valid = false;
    } player_capture; // Selected fields only; engine/client/timers still prevent transfer.
    void SetStartPointName(const std::string& name)
    {
        start_point_name = name;
    }
    std::string start_point_name; // Original CopyString ownership, retained across PreStartWorld.
    PathMgr paths;
    bool world_started = false;
    std::uint64_t updates = 0;

  private:
    CPlayerObj* CreatePlayer(HCLIENT);
    bool RespawnPlayer(CPlayerObj*, HCLIENT);
    ServerDE& server_;
};
class CBloodClientShell
{
  public:
    explicit CBloodClientShell(ServerDE& server) : server_(server) {}
    bool StartNewWorld(const char*, OpenBlood2::WorldMessage&);
    bool OnEnterWorld(HCLIENT, bool restored = false);
    bool restored_entry = false;
    void OnExitWorld();
    bool OnMessage(const OpenBlood2::PlayerEvent&, CBloodServerShell&);
    bool Update();
    bool UpdateMovement(CBloodServerShell&, double, std::string&);
    bool UpdateUse(CBloodServerShell&);
    bool use_down = false, use_was_down = false;
    bool medkit_down = false, medkit_was_down = false;
    std::uint64_t item_actions = 0, item_sounds = 0;
    OpenBlood2::SoundId item_voice = 0; // Checked inspection token, not owned.
    std::string item_audio_error;
    CMoveMgr move_mgr;
    OpenBlood2::MovementInput movement_input;
    bool SyncWeapons(OpenBlood2::ModelStore&, OpenBlood2::Resources&, CBloodServerShell&, std::string&);
    bool UpdateWeapons(CBloodServerShell&, double, std::string&);
    std::array<CViewWeapon, 2> weapons;
    OpenBlood2::WeaponInput weapon_input;
    bool m_bInWorld = false, m_bFirstWorld = true, m_bFirstUpdate = true;
    HOBJECT player = nullptr;
    DVector position{}, camera_position{};
    OpenBlood2::CameraView view;
    DVector listener_position{};
    bool player_visible = false, weapons_visible = true;
    bool UpdateCameraSelection();
    OpenBlood2::PlayerRotation rotation;
    OpenBlood2::PlayerPhysics physics;
    OpenBlood2::PlayerStats stats;
    std::uint64_t messages = 0, updates = 0;
    std::string blocker;
    struct ExitStatus
    {
        OpenBlood2::WorldExit request;
        OpenBlood2::WorldMessage load;
        std::string error;
        std::uint64_t received = 0, ignored_dead = 0, blocked = 0;
        bool pending = false;
    } world_exit;
    OpenBlood2::BossHealth boss_health;
    std::uint64_t boss_health_messages = 0;
    bool boss_health_visible = false;
    CCommLink communication;
    OpenBlood2::MissionObjectives objectives;
    std::string objectives_error;
    std::uint64_t objective_messages = 0;
    bool objectives_resolved = false;

  private:
    bool InitSinglePlayer(CBloodServerShell&);
    HCLIENT client_ = nullptr;
    ServerDE& server_;
};
