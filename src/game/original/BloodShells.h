#pragma once
#include "CommLink.h"
#include "MoveMgr.h"
#include "Music.h"
#include "PathMgr.h"
#include "PlayerDeathCamera.h"
#include "PlayerObj.h"
#include "ScreenFade.h"
#include "ViewWeapon.h"
#include "VoiceMgr.h"
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
    bool OnMessage(HCLIENT, const OpenBlood2::CreatureResponse&);
    bool OnMessage(HCLIENT, const OpenBlood2::PlayerRotation&);
    bool OnMessage(HCLIENT, const OpenBlood2::PlayerItemCommand&);
    bool OnMessage(HCLIENT, const OpenBlood2::PlayerDetonate&);
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
    explicit CBloodClientShell(ServerDE& server, const OpenBlood2::Resources& resources,
                               OpenBlood2::MusicCatalog& catalog, OpenBlood2::AudioMixer& audio)
        : voice_manager(server), resources_(resources), server_(server), music_catalog_(catalog), audio_(audio)
    {
    }
    bool StartNewWorld(const char*, OpenBlood2::WorldMessage&);
    bool OnEnterWorld(HCLIENT, bool restored = false);
    bool restored_entry = false;
    OpenBlood2::PlayerInit entry_profile;
    void OnExitWorld();
    bool OnMessage(const OpenBlood2::PlayerEvent&, CBloodServerShell&);
    bool Update();
    bool UpdateMovement(CBloodServerShell&, double, std::string&);
    bool UpdateUse(CBloodServerShell&);
    bool use_down = false, use_was_down = false;
    bool medkit_down = false, medkit_was_down = false;
    bool flashlight_down = false, flashlight_was_down = false;
    CFlashlightFX flashlight;
    bool binoculars_down = false, binoculars_was_down = false;
    bool goggles_down = false, goggles_was_down = false, eye_down = false, eye_was_down = false;
    bool night_goggles = false;
    HOBJECT seeing_eye = nullptr, creature = nullptr;
    OpenBlood2::SoundId goggles_loop = 0, eye_loop = 0;
    float blind_until = 0, creature_light = 1, creature_damage_time = 0, last_use_hit = 0;
    unsigned use_hits = 0, total_use_hits = 0;
    bool UpdateInventoryEffects();
    void ClearInventoryEffects();
    DVector SceneLightScale() const;
    bool HandleInventoryView(const OpenBlood2::PlayerEvent&, CBloodServerShell&);

    // Original SMSG_ZOOMVIEW/INVITEMACTION client state and UpdateCamera FOV (radians; 0 before entry).
    bool UpdateZoom(double seconds);
    float ViewFovX() const; // Effective first-person horizontal FOV for presentation.
    bool m_bZoomView = false, m_bBinocularsActive = false;
    bool m_bShowCrosshair = true, m_bCrosshairOriginallyOn = true;
    DBYTE m_nCrosshair = 1, m_nCrosshairOriginalNum = 1;
    float m_fovX = 0;
    std::uint64_t zoom_messages = 0;
    bool proximity_down = false, proximity_was_down = false;
    bool remote_down = false, remote_was_down = false, detonate_down = false, detonate_was_down = false;
    std::uint64_t item_actions = 0, item_sounds = 0;
    OpenBlood2::SoundId item_voice = 0; // Checked inspection token, not owned.
    std::string item_audio_error;
    CScreenFade fade;
    CVoiceMgr voice_manager;
    CMusic music;
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
    CPlayerDeathCamera death_camera;
    std::uint64_t death_notifications = 0;
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
        std::uint64_t received = 0, ignored_dead = 0, blocked = 0, queued = 0, completed = 0;
        bool pending = false;
        bool demo_end_pending = false; // Owned terminal intent; never a DAT load request.
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
    const OpenBlood2::Resources& resources_;
    HCLIENT client_ = nullptr;
    ServerDE& server_;
    OpenBlood2::MusicCatalog& music_catalog_;
    OpenBlood2::AudioMixer& audio_;
};
