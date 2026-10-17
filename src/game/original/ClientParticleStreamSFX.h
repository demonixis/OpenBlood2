#pragma once
#include "ClientSFX.h"
#include "ParticleStreamFX.h"
class CClientParticleStreamSFX : public CClientSFX
{
  public:
    OpenBlood2::ParticleStreamSettings settings;
    std::uint64_t effect = 0, commands = 0;
    std::string last_command, error;

  protected:
    DDWORD EngineMessageFn(DDWORD, void*, DFLOAT) override;
    DDWORD ObjectMessageFn(HOBJECT, DDWORD, HMESSAGEREAD) override;

  private:
    bool ReadProp();
};
