#pragma once
#include "Destructable.h"
#include "cpp_engineobjects_de.h"
#include "game/brush_messages.h"
// Original CProjectile Save/Load block (Projectile.cpp:830-920) with the OpenBlood2 shooter
// tail; see save_load_contracts lot 9. Fields the port does not model keep the values written.
struct ProjectileBlock
{
    DVector dir{};
    float velocity = 0;
    DDWORD flags = 0;
    DBYTE damage_type = 0;
    DDWORD radius = 0;
    float start_time = 0; // Absolute once decoded; written relative.
    bool remove = false;
    float damage = 0, life = 0;
    DDWORD damage_message = 0;
    bool shockwave = false, explosion = false, sparks = false;
    DVector shockwave_min{.1f, .1f, 1}, shockwave_max{1, 1, 1}, light_color{1, 1, 1};
    float shockwave_duration = 0;
    std::string shockwave_file;
    DVector dims{1, 1, 1};
    bool explode = false;
    DVector last_position{};
    bool armed = false;
    float pitch_velocity = 0, yaw_velocity = 0, pitch = 0, yaw = 0;
    HOBJECT shooter = nullptr; // OpenBlood2 tail (departure): the original loses m_hFiredFrom.
};
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
    // The m_fStartTime most subclasses shadow with their constructor's GetTime() (the 0.01-second
    // scale delay): unsaved, so it restarts on a restore as in the original.
    float scale_start = 0;

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
    // Save/load (ProjectileSave.cpp). The block hooks let a subclass map the fields it models;
    // Check runs on the decoded block before any member changes.
    virtual void SaveObject(HMESSAGEWRITE);
    virtual void LoadObject(HMESSAGEREAD);
    virtual void SaveBlock(ProjectileBlock&) const;
    virtual std::string CheckBlock(const ProjectileBlock&) const;
    virtual std::string LoadBlock(const ProjectileBlock&); // A service failure after the change.
};
class CEnergyBlastProjectile final : public CProjectile
{
  public:
    std::uint64_t trail = 0;
    bool divine = false;
    CEnergyBlastProjectile()
    {
        filename = "Sprites\\yellowflare2.spr";
        skin = "Skins\\Ammo\\Grenade.dtx";
    }

  protected:
    std::string LoadBlock(const ProjectileBlock&) override;
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
