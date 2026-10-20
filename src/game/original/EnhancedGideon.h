#pragma once
#include "Gideon.h"

// Original EnhancedGideon shares Gideon's combat/teleport algorithms. The
// lifecycle/profile differences remain selected explicitly by NPCType.
class EnhancedGideon final : public Gideon
{
  public:
    bool MC_Walk() override;
    bool MC_Run() override;
    bool AI_STATE_Script(PathMgr& paths) override
    {
        return RunScriptState(paths, &m_bNoFire);
    }

  protected:
    bool ScriptLocomotion(bool run) override;

  private:
    bool Locomote(bool run, bool script);
};
