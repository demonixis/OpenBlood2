#pragma once
#include "ClientSFX.h"
#include "ParticleStreamFX.h"
class CClientParticleStreamSFX : public CClientSFX
{
  public:
    OpenBlood2::ParticleStreamSettings settings;
    // The descriptor as published: the original engine kept this effect message (with the
    // initial ON) and replayed it on a restore (save_load_contracts lot 10).
    OpenBlood2::ParticleStreamSettings published;
    std::uint64_t effect = 0, commands = 0;
    std::string last_command, error;

  protected:
    DDWORD EngineMessageFn(DDWORD, void*, DFLOAT) override;
    DDWORD ObjectMessageFn(HOBJECT, DDWORD, HMESSAGEREAD) override;

  private:
    bool ReadProp();
    void Save(HMESSAGEWRITE);
    void Load(HMESSAGEREAD);
};
