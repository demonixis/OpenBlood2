#pragma once
#include "ClientSFX.h"
#include "game/rain_settings.h"

class Rain final : public CClientSFX
{
  public:
    bool Setup(const OpenBlood2::RainSettings&);
    OpenBlood2::RainSettings settings;
    bool triggered = false;
    std::uint64_t effect = 0;
    std::string error;

  protected:
    DDWORD EngineMessageFn(DDWORD, void*, DFLOAT) override;
    DDWORD ObjectMessageFn(HOBJECT, DDWORD, HMESSAGEREAD) override;

  private:
    bool ReadProps();
    void Publish();
    void Save(HMESSAGEWRITE);
    void Load(HMESSAGEREAD);
};
