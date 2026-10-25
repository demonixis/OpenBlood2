#pragma once
#include "CommLink.h"
#include "Credits.h"
#include "MoveMgr.h"
#include "Music.h"
#include "PathMgr.h"
#include "PlayerDeathCamera.h"
#include "PlayerObj.h"
#include "ScreenFade.h"
#include "ViewWeapon.h"
#include "VoiceMgr.h"
#include "game/session.h"
#include <optional>

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
    bool OnMessage(HCLIENT, const OpenBlood2::PlayerControlAction&);
    bool OnMessage(HCLIENT, const OpenBlood2::PlayerText&);
    bool OnMessage(HCLIENT, const OpenBlood2::PlayerAltFire&);
    bool OnMessage(HCLIENT, const OpenBlood2::CreatureResponse&);
    bool OnMessage(HCLIENT, const OpenBlood2::PlayerRotation&);
    bool OnMessage(HCLIENT, const OpenBlood2::PlayerItemCommand&);
    bool OnMessage(HCLIENT, const OpenBlood2::PlayerDetonate&);
    bool OnMessage(HCLIENT, const OpenBlood2::PlayerItemCursor&);
    bool OnMessage(HCLIENT, const OpenBlood2::PlayerUseInventory&);
    bool OnMessage(HCLIENT, const OpenBlood2::PlayerCheat&);
    void Update(float);
    // Original SaveGame/RestoreGame over the in-memory logical store. The caller owns the
    // frame boundary and the candidate session (GameSession::save_game/load_game).
    bool SaveGame(DBYTE bySaveType, bool bSavePlayers, bool bSaveConsole);
    bool RestoreGame(DBYTE bySaveType);
    // Original CMSG_SAVEGAME (BloodServerShell.cpp:401-431): file name (ignored), save type and
    // the nested client blob, attached to the sender's player before SaveGame. No ACK is sent;
    // the caller reports the result.
    bool OnSaveGameMessage(HCLIENT, HMESSAGEREAD);
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
    bool inspection_start = false; // Explicit local inspection may use an authored multiplayer marker.
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
    // Original SaveClientData/LoadClientData (BloodClientShell.cpp:6351-6431): the client blob
    // a save nests in CMSG_SAVEGAME and a restore receives back in SMSG_CLIENTSAVEDATA.
    void SaveClientData(HMESSAGEWRITE);
    bool LoadClientData(HMESSAGEREAD, std::string& error);
    int m_nLastLoadType = OpenBlood2::LOADTYPE_NEW_GAME; // Saved in the blob, discarded on load.
    std::uint64_t client_data_saves = 0, client_data_loads = 0;
    OpenBlood2::PlayerInit entry_profile;
    // Original PreLoadWorld END/CREDITS test, evaluated at entry from the normalized
    // world path; m_Credits then rolls over the world (CM_CREDITS, no screen clear).
    bool m_bRollCredits = false;
    CCredits m_Credits;
    // Reads the cres.dll TEXT resource "CREDITS" and initializes m_Credits.
    bool InitCredits(bool clear_screen, std::string& error);
    // Original GS_PLAYING roll: Init on the first frame, then CCredits::Update both in the
    // playing branch and after RenderCamera ([blg]), so two updates per rendered frame.
    void UpdateRollCredits(double seconds);
    std::string credits_error;
    std::uint64_t credits_failures = 0;
    void OnExitWorld();
    bool OnMessage(const OpenBlood2::PlayerEvent&, CBloodServerShell&);
    bool Update();
    bool UpdateMovement(CBloodServerShell&, double, std::string&);
    bool UpdateUse(CBloodServerShell&);
    bool grab_down = false, taunt_down = false, drop_weapon_requested = false;
    std::string control_notice;
    std::vector<std::string> chat_lines; // Bounded local console-message history.
    std::uint64_t message_revision = 0;
    double messages_until = 0, control_notice_until = 0;
    void SetControlNotice(std::string text)
    {
        control_notice = std::move(text);
        control_notice_until = server_.GetTime() + 3;
    }
    // Original CCheatMgr::SendCheatMessage (MessageMgr.cpp:1022): the application's Options >
    // CHEATS rows send CMSG_CHEAT for the entered player. The server's ProcessCheat result.
    bool SendCheatMessage(CBloodServerShell&, DBYTE code, bool state);
    bool use_down = false, use_was_down = false, alt_fire_was_down = false;
    // Original OnCommandOn(COMMAND_USE) while dead in GS_PLAYING: single player switches to
    // GS_MENU, pauses and opens the Load menu. The application owns the menu and consumes this.
    bool dead_use_menu = false;
    std::uint64_t dead_use_menus = 0;
    bool medkit_down = false, medkit_was_down = false;
    bool flashlight_down = false, flashlight_was_down = false;
    CFlashlightFX flashlight;
    bool binoculars_down = false, binoculars_was_down = false;
    bool goggles_down = false, goggles_was_down = false, eye_down = false, eye_was_down = false;
    bool night_goggles = false;
    HOBJECT seeing_eye = nullptr, creature = nullptr, orb_view = nullptr;
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
    bool time_bomb_down = false, time_bomb_was_down = false;
    bool proximity_down = false, proximity_was_down = false;
    bool remote_down = false, remote_was_down = false, detonate_down = false, detonate_was_down = false;
    bool next_item_requested = false, prev_item_requested = false, use_item_requested = false;
    OpenBlood2::InventoryItems inventory_items; // Latest SMSG_ITEMCHANGED values.
    std::uint64_t inventory_show_requests = 0, inventory_item_messages = 0;
    std::uint64_t item_actions = 0, item_sounds = 0;
    OpenBlood2::SoundId item_voice = 0; // Checked inspection token, not owned.
    std::string item_audio_error;
    CScreenFade fade;
    CVoiceMgr voice_manager;
    CMusic music;
    CMoveMgr move_mgr;
    OpenBlood2::MovementInput movement_input;
    // OpenBlood2 XR adaptation: the movement direction's yaw when it differs from the aim
    // (rotation.yaw, which the server reads for firing and USE). Empty on desktop.
    std::optional<float> locomotion_yaw;
    // OpenBlood2 XR adaptation: the left-hand weapon's fire direction when the left controller
    // aims it (dual weapons); empty on desktop, where both hands fire along rotation.
    std::optional<DVector> left_hand_forward;
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
    // Scripted/death cameras own the view. The Eye and Orb are player-aimed remote views.
    bool player_look_allowed() const
    {
        return !death_camera.active && (!view.object || view.object == seeing_eye || view.object == orb_view);
    }

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
    struct CTFState
    {
        DBYTE team = 0, flag_icon = 0;
        int score = 0;
        std::uint64_t messages = 0, notices = 0, sounds = 0;
        float last_need_flag = 0;
        std::uint32_t notice_resource = 0;
        std::string notice, error;
        OpenBlood2::SoundId voice = 0;
    } ctf;
    bool HandleCTF(const OpenBlood2::CTFEvent&);
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
