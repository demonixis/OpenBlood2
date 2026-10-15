#pragma once
#include "server_de.h"

class CBloodServerShell;
// Selected standing, dry-land branches of original ClientShellDLL/MoveMgr.
// One checked local player body replaces the legacy client/server duplicate.
// See MOVEMENT_CONTRACTS.md for the source selections and physics adaptations.
class CMoveMgr {
public:
    bool Init(ServerDE&,HCLIENT);
    void Term();
    bool OnPhysicsUpdate(const blood2::PlayerPhysics&);
    bool OnServerForcePos(const blood2::PlayerPosition&);
    bool Update(CBloodServerShell&,const blood2::MovementInput&,float yaw,double seconds,bool can_move);
    blood2::PlayerMove WritePositionInfo(DVector destination,DVector velocity) const;
    blood2::MovementState body;
    blood2::PlayerPhysics physics;
    DDWORD m_dwControlFlags=0, m_dwLastControlFlags=0;
    DBYTE m_ClientMoveCode=0;
    bool m_bOnGround=false, m_bLastJumpCommand=false, m_bFalling=false;
    float m_fVelMagnitude=0, m_FrameTime=0;
    double discarded_time=0;
    std::uint64_t updates=0, substeps=0, jumps=0, landings=0;
    bool ready() const { return server_ && configured_ && positioned_; }
private:
    void UpdateControlFlags(const blood2::MovementInput&,bool);
    void UpdateOnGround();
    void UpdateMotion(float yaw);
    bool MoveLocalSolidObject(CBloodServerShell&);
    ServerDE* server_=nullptr;
    HCLIENT client_=nullptr;
    HOBJECT object_=nullptr;
    bool configured_=false, positioned_=false;
};
