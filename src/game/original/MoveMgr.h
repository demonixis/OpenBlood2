#pragma once
#include "SurfaceTypes.h"
#include "game/local_movement.h"

class CBloodServerShell;
class CVoiceMgr;
// Selected locomotion/container branches of original ClientShellDLL/MoveMgr.
// CMoveMgr integrates a client-owned value body. LocalMovement delivers the
// prediction and the immediate authoritative correction without double integration.
// See movement_contracts.md and locomotion_contracts.md for the source selections and physics adaptations.
class CMoveMgr
{
  public:
    explicit CMoveMgr(OpenBlood2::LocalMovement& movement) : body(movement.body()), movement_(movement) {}
    bool Init(HCLIENT);
    void Term();
    bool OnPhysicsUpdate(const OpenBlood2::PlayerPhysics&);
    bool OnServerForcePos(const OpenBlood2::PlayerPosition&);
    bool Update(CBloodServerShell&, CVoiceMgr&, bool external_camera, const OpenBlood2::MovementInput&, float yaw,
                double seconds, bool can_move, bool firing = false, float pitch = 0);
    OpenBlood2::PlayerMove WritePositionInfo(DVector destination, DVector velocity) const;
    OpenBlood2::ClientMotionState& body;
    OpenBlood2::PlayerPhysics physics;
    DDWORD m_dwControlFlags = 0, m_dwLastControlFlags = 0;
    DBYTE m_ClientMoveCode = 0;
    SurfaceType m_eLastSurface = SURFTYPE_UNKNOWN;
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
    std::uint64_t jump_voice_requests = 0, jump_voice_starts = 0;
    bool ready() const
    {
        return movement_.valid() && configured_ && positioned_;
    }

  private:
    OpenBlood2::MovementInput input_;
    void UpdateControlFlags(const OpenBlood2::MovementInput&, bool);
    void UpdateOnGround();
    void UpdateMotion(float yaw, float pitch, CVoiceMgr&, int character, bool external_camera);
    void UpdateInLiquid(const OpenBlood2::ContainerInfo&);
    void UpdateOnLadder(const OpenBlood2::ContainerInfo&);
    void UpdateOnConveyor(const OpenBlood2::ContainerInfo&);
    bool MoveLocalSolidObject(CBloodServerShell&);
    OpenBlood2::LocalMovement& movement_;
    bool configured_ = false, positioned_ = false;
};
