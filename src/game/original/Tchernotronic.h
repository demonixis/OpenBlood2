#pragma once
#include "B2BaseClass.h"
#include "game/brush_messages.h"
#include <string>
// Original ObjectDLL/Tchernotronic (5/11/98): a triggered model that animates, then
// explodes and removes itself, with its Save/Load block; see tchernotronic_contracts.md.
class Tchernotronic : public B2BaseClass
{
  public:
    Tchernotronic() : B2BaseClass(OT_MODEL) {}
    std::string m_hstrTriggerTarget, m_hstrTriggerMessage;
    DDWORD m_nRestAnim = DDWORD(-1), m_nMovingAnim = DDWORD(-1);
    bool m_bTimer = false;
    float m_fRemoveTime = 0;
    // Inspection: trigger/explosion history.
    std::uint64_t triggers = 0, ignored_player_triggers = 0, explosions = 0, destroy_messages = 0;
    OpenBlood2::SceneExplosionReport explosion;
    OpenBlood2::RadiusDamageReport targets;

  protected:
    DDWORD EngineMessageFn(DDWORD, void*, DFLOAT) override;
    DDWORD ObjectMessageFn(HOBJECT, DDWORD, HMESSAGEREAD) override;

  private:
    void InitialUpdate();
    bool Update();
    void TriggerMsg(HOBJECT, const char*);
    bool PlayAnimation(DDWORD);
    void AddExplosion(DVector);
    void Save(HMESSAGEWRITE);
    void Load(HMESSAGEREAD);
};
