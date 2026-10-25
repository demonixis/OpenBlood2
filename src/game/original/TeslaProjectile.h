#pragma once
#include "Projectile.h"
#include "engine/audio.h"
// Selected original GameProjectiles.cpp Tesla Cannon projectiles. See tesla_contracts.md.
// Primary: a flying sprite that explodes (EXP_TESLA_PRIMARY).
class CTeslaProjectile final : public CProjectile
{
  public:
    CTeslaProjectile()
    {
        filename = "Sprites\\Teslaprime.spr";
        skin = nullptr;
        damage_type = OpenBlood2::DamageType::Explode;
    }
    ~CTeslaProjectile() override;
    OpenBlood2::SoundId flight_sound = 0;
    OpenBlood2::SceneExplosionReport effect;
    std::string light_error, sound_error;

  protected:
    bool Update() override;
    void AddExplosion(DVector, DVector) override;
};
// Shared original single-update forward hitscan (2000 units, LiquidFilterFn, shooter recast).
struct TeslaRay
{
    DVector from{}, to{};
    OpenBlood2::SegmentResult result = OpenBlood2::SegmentResult::Unavailable;
    OpenBlood2::SegmentHit hit;
    bool skipped_shooter = false;
};
// Alternate (every alt shot but the second): hits along its forward vector, radius damage, removed.
class CTeslaBoltProjectile final : public CProjectile
{
  public:
    CTeslaBoltProjectile() : CProjectile(OT_MODEL)
    {
        filename = "Models\\Ammo\\Lightining.abc";
        skin = "Skins\\Ammo\\Pulsebeam.dtx";
        damage_type = OpenBlood2::DamageType::Electric;
    }
    TeslaRay ray;
    bool AdvanceFlight(double) override
    {
        return !failed;
    } // Removed by its first update: the source never flies it.
  protected:
    bool Update() override;
    void AddExplosion(DVector, DVector) override {} // m_bExplosion is false; only unported client marks.
};
// Second alternate shot: arms where its ray stops, then arcs at characters for 2.5 s.
class CTeslaBallProjectile final : public CProjectile
{
  public:
    CTeslaBallProjectile() : CProjectile(OT_MODEL)
    {
        filename = "Models\\Ammo\\Lightining.abc";
        skin = "Skins\\Ammo\\Pulsebeam.dtx";
        damage_type = OpenBlood2::DamageType::Electric;
    }
    TeslaRay ray;
    bool armed = false;
    float attract_radius = 100, arm_time = 0, last_fire_time = 0;
    HOBJECT last_target = nullptr; // Identity only (source m_hLastObj), never dereferenced.
    std::uint64_t bolts = 0, lightning_count = 0, lightning = 0;
    std::string lightning_error;
    OpenBlood2::RadiusDamageReport last_bolt;
    OpenBlood2::SceneExplosionReport effect;
    std::string sound_error;
    bool AdvanceFlight(double) override
    {
        return !failed;
    } // The first update hitscans; once armed it is still.
  protected:
    bool Update() override;
    void AddExplosion(DVector, DVector) override;
    void FireBolt();
    void SaveBlock(ProjectileBlock&) const override;
    std::string LoadBlock(const ProjectileBlock&) override;
};
// Original CShikariLoogieProjectile (GameProjectiles.cpp): an OT_NORMAL Acid spit that
// damages what it hits directly (no radius, no explosion); its bug-spray trail is
// client-only.
class CShikariLoogieProjectile final : public CProjectile
{
  public:
    std::uint64_t trail = 0;
    CShikariLoogieProjectile() : CProjectile(OT_NORMAL)
    {
        filename = "Sprites\\napalmprime.spr";
        skin = nullptr;
        damage_type = OpenBlood2::DamageType::Acid;
        flags |= FLAG_FORCECLIENTUPDATE;
    }

  protected:
    bool InitialUpdate() override;
    std::string LoadBlock(const ProjectileBlock&) override;
    bool Update() override;
    void AddExplosion(DVector, DVector) override;
};
