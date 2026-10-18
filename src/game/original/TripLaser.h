#pragma once
#include "B2BaseClass.h"
#include "engine/audio.h"
#include <string>
// Selected original ObjectDLL/TripLaser (10/23/97). Save/load are unavailable;
// see docs/contracts/trip_laser_contracts.md.
class TripLaser : public B2BaseClass
{
  public:
    TripLaser() : B2BaseClass(OT_MODEL) {}
    ~TripLaser() override;
    // Constructor defaults; world-file/string properties replace them in ReadProp.
    DVector m_vDims{}, m_vColor{255, 0, 0};
    float m_fAlpha = .8f, m_fDamage = 10, m_fDamageTime = 0;
    // TouchTrigger* are read but never sent by the source.
    std::string m_hstrTriggerTarget, m_hstrTriggerMessage, m_hstrSound, m_hstrTouchSound;
    DVector m_vEndPoints[2]{};
    DDWORD m_dwFlags = 0;
    bool m_bTriggered = false, m_bRemove = false;
    // Inspection: retained loop, last results and counts.
    OpenBlood2::SoundId m_hSound = 0, last_touch_voice = 0;
    std::uint64_t touches = 0, beam_hits = 0, damage_messages = 0, flickers = 0;
    std::string error, sound_error;

  protected:
    DDWORD EngineMessageFn(DDWORD, void*, DFLOAT) override;
    DDWORD ObjectMessageFn(HOBJECT, DDWORD, HMESSAGEREAD) override;

  private:
    bool ReadProp();
    void Update();
    void InitialUpdate();
    void HandleTouch(HOBJECT);
    void TriggerMsg(const char*);
    void StartLoop();
    void StopLoop();
};
