#pragma once
#include "DestructableModel.h"
#include "Rotating.h"

// Original ObjectDLL/RotatingModel: destructible model with the Rotating aggregate.
class RotatingModel : public CDestructableModel
{
  public:
    RotatingModel();
    Rotating rotating;

  protected:
    DDWORD EngineMessageFn(DDWORD, void*, DFLOAT) override;
};
