#pragma once
#include "Destructable.h"
#include "cpp_engineobjects_de.h"
#include "game/brush_messages.h"
// Selected original Projectile/GameProjectiles lifecycle. Flight is a separate
// replacement engine phase; no generic sprite physics or hitscan substitution.
class CProjectile : public BaseClass
{
  public:
    CProjectile();
    bool Setup(DVector, DBYTE, float damage, float speed, int radius, HOBJECT);
    bool AdvanceFlight(double seconds);
    void HandleContact(const OpenBlood2::SegmentHit&);
    void Explode();
    CDestructable m_damage;
    HOBJECT m_hFiredFrom = nullptr, m_hHitObject = nullptr;
    DVector m_vDir{};
    float m_fDamage = 0, m_fVelocity = 0, m_fStartTime = 0, m_fLifeTime = 10;
    int m_nRadius = 0;
    DBYTE m_nWeaponType = 0;
    bool configured = false, m_bExplode = false, exploded = false, failed = false;
    OpenBlood2::SegmentHit contact;
    OpenBlood2::RadiusDamageReport targets;
    std::string error, trail_error;
    std::uint64_t flight_steps = 0;
    DDWORD flags = FLAG_VISIBLE | FLAG_TOUCH_NOTIFY | FLAG_REMOVEIFOUTSIDE | FLAG_FORCECLIENTUPDATE |
                   FLAG_POINTCOLLIDE | FLAG_NOSLIDING;

  protected:
    DDWORD EngineMessageFn(DDWORD, void*, DFLOAT) override;
    virtual bool InitialUpdate();
    virtual bool Update();
    virtual void AddExplosion(DVector, DVector) = 0;
    void Finish(const char*);
    bool Fail(const char*);
};
class CEnergyBlastProjectile final : public CProjectile
{
  protected:
    bool InitialUpdate() override;
    bool Update() override;
    void AddExplosion(DVector, DVector) override;
};
