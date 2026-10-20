#pragma once
#include "BoneLeech.h"
// Selected original Behemoth/DrudgePriest/DeathShroud/TheHand decisions and callbacks.
class Behemoth final : public AI_Mgr
{
  public:
    bool ComputeState(int = -1) override;
    bool MC_Fire_Stand() override;
    bool AI_STATE_AttackClose() override;
    bool AI_STATE_AttackFar() override;
};
class DrudgePriest final : public AI_Mgr
{
  public:
    ~DrudgePriest() override;
    bool ComputeState(int = -1) override;
    bool MC_Fire_Stand() override;
    bool MC_Taunt_Bold() override;
    bool MC_Dead() override;
    bool AI_STATE_AttackClose() override;
    bool AI_STATE_AttackFar() override;
    float last_spawn = 0;
    unsigned spawned = 0;
    OpenBlood2::SoundId loop = 0;
    std::string sound_error;

  protected:
    DDWORD EngineMessageFn(DDWORD, void*, DFLOAT) override;
};
class DeathShroud final : public AI_Mgr
{
  public:
    ~DeathShroud() override;
    bool ComputeState(int = -1) override;
    bool MC_Fire_Stand() override;
    bool MC_Dead() override;
    bool AI_STATE_AttackFar() override;
    bool Fade(bool out);
    OpenBlood2::SoundId loop = 0;
    std::string sound_error;

  protected:
    DDWORD EngineMessageFn(DDWORD, void*, DFLOAT) override;
};
class TheHandAI final : public BoneLeech
{
  public:
    bool ComputeState(int = -1) override;
    bool AI_STATE_AttackClose() override;
};
