#pragma once
#include "ClientSFX.h"
#include "LightningFX.h"
#include "game/brush_messages.h"
class CClientLightningSFX : public CClientSFX
{
  public:
    OpenBlood2::LightningSettings bolt;
    OpenBlood2::RadiusDamageReport targets;
    float damage = 500, damage_radius = 300, sound_radius = 1000;
    std::string sound = "Sounds\\Thunder.wav", error, sound_error;
    std::uint64_t fires = 0, event = 0, voice = 0;
    void Fire(HOBJECT);
    bool AllowsDamageAttribution(HOBJECT responsible) const
    {
        return delivering_damage_ && responsible == damage_sender_;
    }

  private:
    HOBJECT damage_sender_ = nullptr;
    bool delivering_damage_ = false;

  protected:
    DDWORD EngineMessageFn(DDWORD, void*, DFLOAT) override;
    DDWORD ObjectMessageFn(HOBJECT, DDWORD, HMESSAGEREAD) override;
};
