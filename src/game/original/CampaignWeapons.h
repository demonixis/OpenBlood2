#pragma once
#include "Weapon.h"
class CVomit final : public CWeapon
{
  public:
    CVomit() : CWeapon(WEAP_GIDEON_VOMIT)
    {
        damage_type = OpenBlood2::DamageType::Fire;
    }
    // The original projectile override removes gravity. CVomitProjectile's
    // flags already encode that policy; its damage type remains NORMAL.
};
class CGideonSpear final : public CWeapon
{
  public:
    CGideonSpear() : CWeapon(WEAP_GIDEON_SPEAR) {}
    FireResult FireVector() override;
    HOBJECT immobilize_target = nullptr;
    std::uint64_t immobilize_deliveries = 0;
    std::string immobilize_error;
};
class CHowitzer final : public CWeapon
{
  public:
    CHowitzer() : CWeapon(WEAP_HOWITZER) {}
};
class CNapalmCannon final : public CWeapon
{
  public:
    CNapalmCannon() : CWeapon(WEAP_NAPALMCANNON) {}
};
class CNagaSpike final : public CWeapon
{
  public:
    CNagaSpike() : CWeapon(WEAP_NAGA_SPIKE) {}
};
class CNagaDebris final : public CWeapon
{
  public:
    CNagaDebris() : CWeapon(WEAP_NAGA_DEBRIS)
    {
        damage_type = OpenBlood2::DamageType::Explode;
    }
};
class CGroundStrike final : public CWeapon
{
  public:
    CGroundStrike() : CWeapon(WEAP_ZEALOT_GROUNDFIRE) {}
};
class CShockwave final : public CWeapon
{
  public:
    CShockwave() : CWeapon(WEAP_ZEALOT_SHOCKWAVE) {}
};
class CThiefSuck final : public CBoneleechSuck
{
  public:
    CThiefSuck() : CBoneleechSuck(WEAP_THIEF_SUCK) {}
};
class CZealotSpell final : public CWeapon
{
  public:
    explicit CZealotSpell(DBYTE type) : CWeapon(type) {}
    FireResult FireVector() override;
    std::string effect_error;
};
class CDeathRay final : public CWeapon
{
  public:
    CDeathRay() : CWeapon(WEAP_DEATHRAY) {}
    ~CDeathRay() override;
    HOBJECT ring_anchor = nullptr;
    void BeginFire(bool) override;
    std::string area_error, effect_error;
    unsigned area_deliveries = 0;
};
class CWeapTimeBomb final : public CWeapProximityBomb
{
  public:
    CWeapTimeBomb() : CWeapProximityBomb(WEAP_TIMEBOMB) {}
};

class CDrudgeLightning final : public CWeapon
{
  public:
    CDrudgeLightning() : CWeapon(WEAP_DRUDGE_LIGHTNING) {}
    FireResult FireVector() override;
    DVector lightning_end{};
    std::uint64_t lightning = 0;
    std::string effect_error;
};

class CMiniGun final : public CWeapon
{
  public:
    CMiniGun() : CWeapon(WEAP_MINIGUN) {}
};
class CBehemothShockwave final : public CWeapon
{
  public:
    CBehemothShockwave() : CWeapon(WEAP_BEHEMOTH_SHOCKWAVE) {}
};
class CSkull final : public CWeapon
{
  public:
    CSkull() : CWeapon(WEAP_SKULL) {}
};
