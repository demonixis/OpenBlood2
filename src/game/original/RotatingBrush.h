#pragma once
#include "DestructableBrush.h"
#include "Rotating.h"
class RotatingBrush : public CDestructableBrush
{
  public:
    RotatingBrush();
    Rotating rotating;

  protected:
    DDWORD EngineMessageFn(DDWORD, void*, DFLOAT) override;
};
