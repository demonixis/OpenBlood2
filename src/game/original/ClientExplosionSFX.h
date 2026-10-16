#pragma once
#include "ClientSFX.h"
#include "game/brush_messages.h"
// Original authored explosion emitter. Client FX rendering remains unavailable.
class CClientExplosionSFX : public CClientSFX
{
  public:
    DVector position{}, normal{};
    float damage = 0, radius = 0;
    DDWORD type = 0;
    std::string sound;
    bool triggered = false;
    std::uint64_t commands = 0, activations = 0;
    OpenBlood2::RadiusDamageReport targets;
    std::uint64_t voice = 0;
    std::string sound_error;

  protected:
    DDWORD EngineMessageFn(DDWORD, void*, DFLOAT) override;
    DDWORD ObjectMessageFn(HOBJECT, DDWORD, HMESSAGEREAD) override;

  private:
    bool ReadProp();
    void SetupFX();
};
