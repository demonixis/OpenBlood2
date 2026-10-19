#pragma once
#include "AI_Mgr.h"
// Original Naga: hidden entrance, boss health, spike/eye/debris attacks and death fade.
class Naga final : public AI_Mgr
{
  public:
    ~Naga() override;
    bool ComputeState(int = -1) override;
    bool MC_Fire_Stand() override;
    bool MC_Extra(std::string_view) override;
    bool Fire(bool = false) override;
    bool SetNewState(int) override;
    bool AI_STATE_AttackClose() override;
    bool AI_STATE_AttackFar() override
    {
        return AI_STATE_AttackClose();
    }
    bool AI_STATE_Special(int) override;
    bool MC_Dead() override;
    HOBJECT spike = nullptr;
    HATTACHMENT spike_attachment = 0;
    unsigned ceiling_whomps = 0;
    float full_health = 0;
    bool create_health = true;

  protected:
    DDWORD EngineMessageFn(DDWORD, void*, DFLOAT) override;
    bool OnAcceptedDamage() override;

  private:
    bool Fade(bool in);
};
