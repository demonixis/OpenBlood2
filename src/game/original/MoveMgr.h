#pragma once
#include "server_de.h"

class CBloodServerShell;
// Selected locomotion/container branches of original ClientShellDLL/MoveMgr.
// One checked local player body replaces the legacy client/server duplicate.
// See MOVEMENT_CONTRACTS.md and LOCOMOTION_CONTRACTS.md for the source selections and physics adaptations.
class CMoveMgr
{
  public:
    bool Init(ServerDE&, HCLIENT);
    void Term();
    bool OnPhysicsUpdate(const OpenBlood2::PlayerPhysics&);
    bool OnServerForcePos(const OpenBlood2::PlayerPosition&);
    bool Update(CBloodServerShell&, const OpenBlood2::MovementInput&, float yaw, double seconds, bool can_move,
                bool firing = false, float pitch = 0);
    OpenBlood2::PlayerMove WritePositionInfo(DVector destination, DVector velocity) const;
    OpenBlood2::MovementState body;
    OpenBlood2::PlayerPhysics physics;
    DDWORD m_dwControlFlags = 0, m_dwLastControlFlags = 0;
    DBYTE m_ClientMoveCode = 0;
    bool m_bOnGround = false, m_bLastJumpCommand = false, m_bFalling = false;
    bool m_bForcedCrouch = false;
    OpenBlood2::ContainerState containers;
    bool m_bBodyInLiquid = false, m_bBodyOnLadder = false, m_bBodyOnConveyor = false, m_bSwimmingOnSurface = false;
    bool m_bCanSwimJump = false, m_bSwimmingJump = false, m_bUseGravity = true;
    ContainerCode m_eLastContainerCode = CC_NOTHING;
    float m_fSwimVel = 175, m_fLadderVel = 400;
    float m_fVelMagnitude = 0, m_FrameTime = 0;
    double discarded_time = 0;
    std::uint64_t updates = 0, substeps = 0, jumps = 0, landings = 0;
    bool ready() const
    {
        return server_ && configured_ && positioned_;
    }

  private:
    void UpdateControlFlags(const OpenBlood2::MovementInput&, bool);
    void UpdateOnGround();
    void UpdateMotion(float yaw, float pitch);
    void UpdateInLiquid(const OpenBlood2::ContainerInfo&);
    void UpdateOnLadder(const OpenBlood2::ContainerInfo&);
    void UpdateOnConveyor(const OpenBlood2::ContainerInfo&);
    bool MoveLocalSolidObject(CBloodServerShell&);
    ServerDE* server_ = nullptr;
    HCLIENT client_ = nullptr;
    HOBJECT object_ = nullptr;
    bool configured_ = false, positioned_ = false;
};
