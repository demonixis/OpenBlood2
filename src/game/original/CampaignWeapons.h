#pragma once
#include "Weapon.h"
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
