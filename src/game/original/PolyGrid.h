#pragma once
#include "ClientSFX.h"
#include "PolyGridFX.h"
class PolyGrid : public CClientSFX
{
  public:
    bool Setup(const OpenBlood2::PolyGridSettings&);
    OpenBlood2::PolyGridSettings settings;
    std::uint64_t effect = 0;
    std::string error;

  protected:
    DDWORD EngineMessageFn(DDWORD, void*, DFLOAT) override;

  private:
    bool setup_ = false;
};
