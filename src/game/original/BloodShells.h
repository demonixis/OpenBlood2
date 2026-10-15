#pragma once
#include "PlayerObj.h"
#include "ViewWeapon.h"
#include "MoveMgr.h"
#include "game/session.h"

// Original single-player entry branches adapted to the typed local boundary.
// These shells intentionally have no menu/audio/save/network facades.
class CBloodServerShell {
public:
    explicit CBloodServerShell(ServerDE& server) : server_(server) {}
    void PreStartWorld();
    bool PostStartWorld();
    CPlayerObj* OnClientEnterWorld(HCLIENT);
    void OnClientExitWorld(HCLIENT);
    bool OnMessage(HCLIENT,const blood2::PlayerInit&);
    FireResult OnMessage(HCLIENT,const blood2::WeaponFire&);
    bool OnMessage(HCLIENT,const blood2::PlayerMove&);
    void Update(float);
    CPlayerObj* Player(HCLIENT);
    struct PathEntry { DVector position; std::string name, action_target, action_message; };
    std::vector<PathEntry> paths;
    bool world_started = false;
    std::uint64_t updates = 0;
private:
    CPlayerObj* CreatePlayer(HCLIENT);
    bool RespawnPlayer(CPlayerObj*,HCLIENT);
    ServerDE& server_;
};
class CBloodClientShell {
public:
    explicit CBloodClientShell(ServerDE& server) : server_(server) {}
    bool StartNewWorld(const char*,blood2::WorldMessage&);
    bool OnEnterWorld(HCLIENT);
    void OnExitWorld();
    bool OnMessage(const blood2::PlayerEvent&,CBloodServerShell&);
    bool Update();
    bool UpdateMovement(CBloodServerShell&,double,std::string&);
    CMoveMgr move_mgr;
    blood2::MovementInput movement_input;
    bool SyncWeapons(blood2::ModelStore&,blood2::Resources&,CBloodServerShell&,std::string&);
    bool UpdateWeapons(CBloodServerShell&,double,std::string&);
    std::array<CViewWeapon,2> weapons;
    blood2::WeaponInput weapon_input;
    bool m_bInWorld = false, m_bFirstWorld = true, m_bFirstUpdate = true;
    HOBJECT player = nullptr;
    DVector position{}, camera_position{};
    blood2::PlayerRotation rotation;
    blood2::PlayerPhysics physics;
    blood2::PlayerStats stats;
    std::uint64_t messages = 0, updates = 0;
    std::string blocker;
private:
    bool InitSinglePlayer(CBloodServerShell&);
    HCLIENT client_ = nullptr;
    ServerDE& server_;
};
