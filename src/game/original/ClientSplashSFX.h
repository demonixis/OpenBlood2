#pragma once
#include "ClientSFX.h"
#include "game/splash_settings.h"

class CClientSplashSFX final : public CClientSFX
{
  public:
    std::uint64_t Setup(const OpenBlood2::SplashSettings&, std::string&);

  protected:
    DDWORD EngineMessageFn(DDWORD message, void* data, DFLOAT reason) override;
};
