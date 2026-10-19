#pragma once
#include "ClientSFX.h"
#include "ParticleStreamFX.h"
// Original ObjectDLL/ParticleSystem descriptor, using the shared owned particle runtime.
class ParticleSystem final : public CClientSFX
{
  public:
    OpenBlood2::ParticleStreamSettings settings;
    bool static_system = true;
    std::uint64_t effect = 0;
    std::string error;

  protected:
    DDWORD EngineMessageFn(DDWORD, void*, DFLOAT) override;
};
