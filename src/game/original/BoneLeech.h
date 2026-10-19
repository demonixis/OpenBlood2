#pragma once
#include "AI_Mgr.h"
// Selected original BoneLeech lifecycle, smell decisions and jumping attack.
// The client attachment handshake remains an explicit runtime failure.
class BoneLeech : public AI_Mgr
{
  public:
    bool ComputeState(int stimulus = -1) override;
    bool AI_STATE_AttackClose() override;
    bool FireJump();
    bool attachment_requested = false;
    std::uint64_t jumps = 0;

  protected:
    DDWORD EngineMessageFn(DDWORD, void*, DFLOAT) override;
};
