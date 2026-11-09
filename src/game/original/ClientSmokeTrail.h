#pragma once
#include "ClientSFX.h"

class CClientSmokeTrail final : public CClientSFX
{
  public:
    bool Setup(DVector velocity, DBOOL small);
    DVector velocity{};
    bool small = false, configured = false;
};
