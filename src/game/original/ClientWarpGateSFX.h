#pragma once
#include "ClientSFX.h"
#include "WarpGateFX.h"
class CClientWarpGateSFX final : public CClientSFX
{
  public:
    OpenBlood2::WarpGateSettings settings;
    bool on = true, first_update = false;
    float sound_radius = 200;
    std::array<std::string, 3> sounds; // Source reads/saves these, but never plays them.
    std::string error;
    std::uint64_t event = 0, commands = 0;

  protected:
    DDWORD EngineMessageFn(DDWORD, void*, DFLOAT) override;
    DDWORD ObjectMessageFn(HOBJECT, DDWORD, HMESSAGEREAD) override;
};
