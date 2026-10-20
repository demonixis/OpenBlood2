#pragma once
#include "B2BaseClass.h"
#include "engine/audio.h"
#include <string>
// Selected original ObjectDLL/Spawner; the rift client FX remains unavailable. See
// spawner_contracts.md. Save/Load follow the original block (save_load_contracts lot 7).
class Spawner : public B2BaseClass
{
  public:
    Spawner() : B2BaseClass(OT_NORMAL) {}
    // Constructor defaults; world-file properties replace them in ReadProp.
    std::string m_hstrSound, m_hstrSpawnObject;
    DVector m_vMinVelocity{-100, 400, -100}, m_vMaxVelocity{100, 600, 100};
    float m_fSoundRadius = 500, m_fRespawnRate = 1;
    bool m_bUseTriggerObjPos = false, m_bStartActive = false, m_bCreateRiftEffect = false;
    DDWORD m_dwRespawnCount = DDWORD(-1); // Source DDWORD: -1 is a large unsigned count.
    // Inspection: last attempt and totals. Children are independent checked tokens.
    std::uint64_t triggers = 0, spawn_attempts = 0, spawned = 0, spawn_failures = 0, rift_unavailable = 0;
    HOBJECT last_child = nullptr;
    OpenBlood2::SoundId last_voice = 0;
    std::string error;

  protected:
    DDWORD EngineMessageFn(DDWORD, void*, DFLOAT) override;
    DDWORD ObjectMessageFn(HOBJECT, DDWORD, HMESSAGEREAD) override;

  private:
    bool ReadProp();
    void Update();
    bool SpawnObject(HOBJECT sender);
    void Save(HMESSAGEWRITE);
    void Load(HMESSAGEREAD);
};
