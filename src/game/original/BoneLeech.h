#pragma once
#include "AI_Mgr.h"
// Selected original BoneLeech lifecycle, smell decisions and jumping attack.
// Typed local handshake preserves source attach, damage, USE and detach phases.
class BoneLeech : public AI_Mgr
{
  public:
    bool ComputeState(int stimulus = -1) override;
    bool AI_STATE_AttackClose() override;
    bool FireJump();
    bool AI_STATE_EnemyAttach() override;
    bool RequestAttach(HOBJECT);
    bool Respond(HOBJECT player, bool proceed);
    bool Detach();
    HOBJECT attached = nullptr, track = nullptr;
    bool proceed = false;
    float last_damage = 0, last_detach = 0;
    DDWORD world_flags = 0;
    DVector world_dims{};
    std::uint64_t damage_ticks = 0;
    bool attachment_requested = false;
    std::uint64_t jumps = 0;

  protected:
    DDWORD EngineMessageFn(DDWORD, void*, DFLOAT) override;
    DDWORD ObjectMessageFn(HOBJECT, DDWORD, HMESSAGEREAD) override;
    // Original BoneLeech/Thief/TheHandAI::Save/Load (one block each; save/load lot 12), the port's
    // detach state, and the original MID_LOADOBJECT re-attachment after the aggregates.
    void SaveSubclass(HMESSAGEWRITE) override;
    void LoadSubclass(HMESSAGEREAD) override;
    void AfterLoad(HMESSAGEREAD) override;
    HOBJECT SavedEnemyAttach() const override
    {
        return attached;
    }
    bool AcceptsEnemyAttach() const override
    {
        return true;
    }
};
