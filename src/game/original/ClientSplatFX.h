#pragma once
#include "ClientSFX.h"
class CClientSplatFX : public CClientSFX
{
  public:
    bool Setup(DVector position, DVector direction, float scale, float growth);
    std::uint64_t event = 0;
    std::string error;

  protected:
    DDWORD EngineMessageFn(DDWORD, void*, DFLOAT) override;
};
