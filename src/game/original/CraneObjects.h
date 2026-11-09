#pragma once
#include "Destructable.h"
#include "VolumeBrush.h"

class CraneControl final : public VolumeBrush
{
  public:
    CraneControl()
    {
        info.code = CC_CRANECONTROL;
    }
    ~CraneControl() override;
    void UpdatePhysics(OpenBlood2::ContainerPhysics&) override;

    HOBJECT camera = nullptr;
    float yaw = 0;
    std::string control_error;

  private:
    HOBJECT crane_ = nullptr, ball_ = nullptr;
};

class WreckingBall final : public B2BaseClass
{
  public:
    WreckingBall();
    DDWORD EngineMessageFn(DDWORD, void*, DFLOAT) override;

    HOBJECT crane = nullptr, chain = nullptr;
    DVector anchor{};
    CDestructable damage;
    std::string error;

  private:
    bool Update();
    bool ReadAnchor(DVector&) const;
    void DamageNearbyModels();
    bool first_ = true;
};
