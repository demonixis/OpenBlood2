#pragma once
#include "ClientSFX.h"
#include "client/line_effects.h"

class CClientCastLineSFX final : public CClientSFX
{
  public:
    bool Setup(DVector start_color, DVector end_color, float start_alpha, float end_alpha, HOBJECT target);
    bool Geometry(DVector origin, const DRotation&, OpenBlood2::LineEffectGeometry&) const;
    DDWORD EngineMessageFn(DDWORD, void*, DFLOAT) override;

  private:
    HOBJECT target_ = nullptr;
    DVector color_{};
    float length_ = 0, start_alpha_ = 1, end_alpha_ = 1;
    bool configured_ = false;
};

class CClientDebugLine final : public CClientSFX
{
  public:
    bool Setup(DVector from, DVector to);
    DDWORD EngineMessageFn(DDWORD, void*, DFLOAT) override;
};
