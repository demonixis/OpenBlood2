#pragma once
#include "Destructable.h"
#include "cpp_engineobjects_de.h"
#include "game/brush_messages.h"
// Selected original Projectile/GameProjectiles lifecycle. Flight is a separate
// replacement engine phase; no generic sprite physics or hitscan substitution.
class CProjectile : public BaseClass
{
  public:
    explicit CProjectile(DBYTE type = OT_SPRITE);
    bool Setup(DVector, DBYTE, float damage, float speed, int radius, HOBJECT);
    virtual bool AdvanceFlight(double seconds);
    virtual void HandleContact(const OpenBlood2::SegmentHit&);
    void Explode(bool repeat = false);
    bool contacted = false;
    OpenBlood2::DamageType damage_type = OpenBlood2::DamageType::Explode;
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
    const char* filename = "Models\\Ammo\\Grenade.abc";
    const char* skin = "Skins\\Ammo\\Grenade.dtx";
    virtual bool InitialUpdate();
    virtual bool Update();
    virtual void AddExplosion(DVector, DVector) = 0;
    bool AddImpact(DVector);
    bool exploding_ = false;
    void Finish(const char*);
    bool Fail(const char*);
};
class CEnergyBlastProjectile final : public CProjectile
{
  public:
    bool divine = false;
    CEnergyBlastProjectile()
    {
        filename = "Sprites\\yellowflare2.spr";
        skin = "Skins\\Ammo\\Grenade.dtx";
    }

  protected:
    bool InitialUpdate() override;
    bool Update() override;
    void AddExplosion(DVector, DVector) override;
};

class CBehemothShockwaveProjectile final : public CProjectile
{
  public:
    CBehemothShockwaveProjectile() : CProjectile(OT_NORMAL) {}
    DVector ground_from{}, ground_to{}, blast_position{}, blast_normal{};
    OpenBlood2::SegmentResult ground_result = OpenBlood2::SegmentResult::Unavailable;
    OpenBlood2::SegmentHit ground_hit;
    OpenBlood2::SceneExplosionReport effect;
    bool AdvanceFlight(double) override
    {
        return !failed;
    } // One update at the shooter's current position.
  protected:
    bool Update() override;
    void AddExplosion(DVector, DVector) override;
};

// Original GameProjectiles.cpp CHowitzerShell: one update casts 3000 units along
// the shell's forward vector (past its shooter), explodes at the hit and is removed.
class CHowitzerShell : public CProjectile
{
  public:
    CHowitzerShell() : CProjectile(OT_NORMAL)
    {
        m_fLifeTime = 5;
    }
    DVector ray_from{}, ray_to{};
    OpenBlood2::SegmentResult ray_result = OpenBlood2::SegmentResult::Unavailable;
    OpenBlood2::SegmentHit ray_hit;
    bool alternate = false;
    bool skipped_shooter = false, direct_hit = false;
    std::uint64_t orphaned = 0; // Shooter already pending removal at the shell's update.
    OpenBlood2::SceneExplosionReport effect;
    bool AdvanceFlight(double) override
    {
        return !failed;
    } // Hitscan: the source never flies the shell.
  protected:
    bool Update() override;
    void AddExplosion(DVector, DVector) override;
};
