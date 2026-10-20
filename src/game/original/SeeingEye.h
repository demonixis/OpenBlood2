#pragma once
#include "Projectile.h"
// Original SeeingEye callbacks; CProjectile supplies checked flight/owner links only.
class SeeingEye final : public CProjectile
{
  public:
    SeeingEye();
    ~SeeingEye() override;
    bool Init(HOBJECT owner, HCLIENT client);
    bool AdvanceFlight(double) override;
    void HandleContact(const OpenBlood2::SegmentHit&) override;
    void RemoveEye(bool destroyed);
    bool dropped = false;
    float next_ping = 0;
    HOBJECT stuck = nullptr;
    HCLIENT client = nullptr;
    std::string sound_error;

  protected:
    DDWORD EngineMessageFn(DDWORD, void*, DFLOAT) override;
    DDWORD ObjectMessageFn(HOBJECT, DDWORD, HMESSAGEREAD) override;
    bool InitialUpdate() override;
    bool Update() override;
    void AddExplosion(DVector, DVector) override
    {
        Fail("SeeingEye cannot explode");
    }
    // Original SeeingEye.cpp:488-516: the owner, stuck object and dropped flag, never the
    // CProjectile block (the original eye derives from BaseClass).
    void SaveObject(HMESSAGEWRITE) override;
    void LoadObject(HMESSAGEREAD) override;

  private:
    bool removing_ = false;
};
