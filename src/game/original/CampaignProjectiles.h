#pragma once
#include "NapalmProjectile.h"
#include "Projectile.h"
#include "ProximityBomb.h"
class CVomitProjectile final : public CProjectile
{
  public:
    CVomitProjectile();
    std::uint64_t trail = 0;

  protected:
    bool InitialUpdate() override;
    bool Update() override;
    // Original m_bExplosion/m_bClientFX/m_bShockwave are all false.
    void AddExplosion(DVector, DVector) override {}
    std::string LoadBlock(const ProjectileBlock&) override;
};
class CTimeBomb final : public CProximityBomb
{
  public:
    CTimeBomb();

  protected:
    bool Update() override;
    void AddExplosion(DVector, DVector) override;
    void Sound(const char*) override {}
};
class CNagaSpikeProjectile final : public CProjectile
{
  public:
    CNagaSpikeProjectile();

  protected:
    void AddExplosion(DVector, DVector) override;
};
class CNagaDebrisProjectile final : public CProjectile
{
  public:
    CNagaDebrisProjectile() : CProjectile(OT_NORMAL)
    {
        filename = skin = nullptr;
    }
    bool AdvanceFlight(double) override
    {
        return !failed;
    }
    unsigned children = 0;

  protected:
    bool Update() override;
    void AddExplosion(DVector, DVector) override;
};
class NagaCeilingDebris final : public BaseClass
{
  public:
    NagaCeilingDebris() : BaseClass(OT_MODEL) {}
    float start_time = 0;
    DVector angular_step{};
    std::string error;

  protected:
    DDWORD EngineMessageFn(DDWORD, void*, DFLOAT) override;
};
class CGroundStrikeProjectile final : public CProjectile
{
  public:
    CGroundStrikeProjectile();

  protected:
    bool Update() override;
    void AddExplosion(DVector, DVector) override {} // Source disables explosion FX.
};
class CShockwaveProjectile final : public CProjectile
{
  public:
    CShockwaveProjectile() : CProjectile(OT_NORMAL)
    {
        filename = skin = nullptr;
    }
    bool AdvanceFlight(double) override
    {
        return !failed;
    }

  protected:
    bool Update() override;
    void AddExplosion(DVector, DVector) override;
};
class CDeathRayProjectile final : public CProjectile
{
  public:
    CDeathRayProjectile() : CProjectile(OT_NORMAL)
    {
        damage_type = OpenBlood2::DamageType::Electric;
    }
    bool AdvanceFlight(double) override
    {
        return !failed;
    }
    float last_bounce = 0, bounce_distance = 0;
    unsigned reflections = 0;
    bool first = true;
    std::string beam_error;

  protected:
    bool Update() override;
    void AddExplosion(DVector, DVector) override;
};
class CHowitzerAltShell final : public CHowitzerShell
{
  public:
    CHowitzerAltShell()
    {
        alternate = true;
    }

  protected:
    void AddExplosion(DVector, DVector) override;
};
class CHowitzerAltFrag final : public CProjectile
{
  public:
    CHowitzerAltFrag();

  protected:
    bool InitialUpdate() override;
    bool Update() override;
    void AddExplosion(DVector, DVector) override;
};
class CNapalmAltProjectile final : public CNapalmProjectile
{
  protected:
    void AddExplosion(DVector, DVector) override;
};
class CNapalmFireball final : public CProjectile
{
  public:
    CNapalmFireball();
    unsigned bounces = 0;
    bool AdvanceFlight(double) override;
    void HandleContact(const OpenBlood2::SegmentHit&) override;

  protected:
    bool InitialUpdate() override;
    bool Update() override;
    void AddExplosion(DVector, DVector) override;
};
void PublishCampaignExplosion(HOBJECT, DVector, DVector, unsigned, const char* = nullptr, float = 1000);

class CSkullProjectile final : public CProjectile
{
  public:
    CSkullProjectile();
    ~CSkullProjectile() override;
    void HandleContact(const OpenBlood2::SegmentHit&) override;
    HOBJECT tracked = nullptr; // Reacquired from checked destructible identities on every update.
    OpenBlood2::SoundId loop = 0;
    std::string sound_error;
    unsigned contact_damage = 0;

  protected:
    bool InitialUpdate() override;
    bool Update() override;
    void AddExplosion(DVector, DVector) override;
    std::string LoadBlock(const ProjectileBlock&) override;
};

bool PublishDivineShockball(HOBJECT, DVector, DVector, OpenBlood2::SceneExplosionReport&, const char* = nullptr);
