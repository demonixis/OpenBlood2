#pragma once
#include "AISound.h"
#include "WeaponDefs.h"
#include "WeaponFX.h"
#include "engine/audio.h"
#include "game/weapon_messages.h"
#include <array>
#include <string>

class CInventoryMgr;
enum class FireResult
{
    Fired,
    Dry,
    Invalid,
    Unavailable,
    DeliveryFailed,
    Blocked
};
struct WeaponShot
{
    OpenBlood2::WeaponFire request;
    DVector direction{}, end{};
    float damage = 0;
    OpenBlood2::SegmentResult collision = OpenBlood2::SegmentResult::Miss;
    OpenBlood2::SegmentHit hit;
    bool damage_delivered = false, interrupted = false;
    FireResult result = FireResult::Invalid;
};
enum class WeaponImpactBranch
{
    World,
    Brush,
    Door,
    Volume,
    Other
};
struct WeaponImpact
{
    OpenBlood2::SegmentHit hit;
    DVector from{};
    unsigned surface = 0, stopping_surface = 0;
    WeaponImpactBranch branch = WeaponImpactBranch::Other;
    bool continued = false, effect_requested = false, damage_delivered = false, target_removed = false;
    CWeaponFX effect;
    OpenBlood2::SoundAlertReport alert;
};
// Selected vector-weapon path and original retained fire-sound lifecycle.
class CWeapon
{
  protected:
    explicit CWeapon(DBYTE type) : data(*weapon_definition(type)) {}

  public:
    virtual ~CWeapon();
    void PlayFireSound(bool alternate, DVector position);
    void UpdateSounds();
    void KillSounds();
    std::array<OpenBlood2::SoundId, 4> fire_sounds{};
    std::array<float, 4> fire_sound_ends{};
    std::string fire_sound_resource, fire_sound_error;
    OpenBlood2::SoundId last_fire_sound = 0; // Historical ID, resolve before inspecting.
    OpenBlood2::SoundAlertReport fire_sound_alert;
    std::uint64_t sound_attempts = 0, sounds_played = 0, sound_failures = 0, sound_evictions = 0, sounds_released = 0,
                  sound_alert_failures = 0;
    bool ShowHandModel(DBOOL);
    bool DropHandModel();
    bool firing = false; // Guards the synchronous server Fire callback, including NPC REST reset.
    HOBJECT m_hHandModel = nullptr;
    HATTACHMENT m_hHandAttachment = 0;
    std::string hand_error;
    void Init(HOBJECT owner, CInventoryMgr* inventory, bool left)
    {
        m_hOwner = owner;
        m_pInventoryMgr = inventory;
        m_bLeftHand = left;
    }
    virtual DBYTE GetAmmoType(bool = false) const
    {
        return data.m_nAmmoType;
    }
    float CheckAmmo(bool alternate = false) const;
    FireResult Fire(const OpenBlood2::WeaponFire&);
    // Subclass Fire() prologue, called on every accepted Fire request before ammo checks.
    virtual void BeginFire(bool /*alternate*/) {}
    virtual const char* FireSound(bool alternate) const;
    virtual FireResult FireProjectile();
    HOBJECT projectile = nullptr;               // Checked independent runtime child, never owning.
    const char* alt_projectile_class = nullptr; // Source m_szAltProjectileClass override.
    std::string projectile_error;
    virtual FireResult FireVector();
    std::vector<WeaponImpact> impacts; // Latest validated shot attempt, bounded to 64 contacts.
    std::string impact_error;
    std::uint64_t impact_contacts = 0, impact_sounds = 0, impact_sound_failures = 0;
    std::minstd_rand impact_random{1}; // Separate owned client-FX randomness; never changes game/shot RNG.
    const WeaponData& data;
    HOBJECT m_hOwner = nullptr;
    CInventoryMgr* m_pInventoryMgr = nullptr;
    bool m_bLeftHand = false;
    OpenBlood2::DamageType damage_type = OpenBlood2::DamageType::Normal;
    float m_fLastShotTime = 0;
    bool m_bLastFireAlt = false;
    std::uint64_t shots = 0;
    DDWORD last_pellets = 0; // Vectors of the last accepted fire (shotgun: 16); last_shot is the final one.
    WeaponShot last_shot;
};
// Original visual ray and ordinary damage ray deliberately have different filters.
class CNagaEyes final : public CWeapon
{
  public:
    CNagaEyes() : CWeapon(WEAP_NAGA_EYEBEAM) {}
    FireResult FireVector() override;
    OpenBlood2::SegmentResult beam_collision = OpenBlood2::SegmentResult::Unavailable;
    DVector beam_end{};
    OpenBlood2::SegmentHit beam_hit;
    std::uint64_t beam = 0;
    std::string beam_error, explosion_error;
};
class CEnergyBlast final : public CWeapon
{
  public:
    CEnergyBlast() : CWeapon(WEAP_ZEALOT_ENERGYBLAST) {}
    const char* FireSound(bool alternate) const override;
};
class CGideonShield final : public CWeapon
{
  public:
    CGideonShield() : CWeapon(WEAP_GIDEON_SHIELD) {}
    FireResult FireVector() override;
    std::uint64_t effect = 0;
    std::string effect_error;
};
class CGideonWind final : public CWeapon
{
  public:
    CGideonWind() : CWeapon(WEAP_GIDEON_WIND) {}
    FireResult FireVector() override
    {
        // Both original definitions select the shockwave projectile. The unused
        // 1200-unit vector push branch is not part of this integration.
        return last_shot.result = FireResult::Unavailable;
    }
};
class CWeapRemoteBomb final : public CWeapon
{
  public:
    CWeapRemoteBomb() : CWeapon(WEAP_REMOTEBOMB) {}
    FireResult FireProjectile() override;
};
class CWeapProximityBomb final : public CWeapon
{
  public:
    CWeapProximityBomb() : CWeapon(WEAP_PROXIMITYBOMB) {}
    FireResult FireProjectile() override;
};
class CBeretta final : public CWeapon
{
  public:
    CBeretta() : CWeapon(WEAP_BERETTA) {}
};
// Original CShotgun::Fire only selects shell ejection (a client effect not ported) before
// CWeapon::Fire, whose FireSpread casts its sixteen vectors.
class CShotgun final : public CWeapon
{
  public:
    CShotgun() : CWeapon(WEAP_SHOTGUN) {}
};
class CAssaultRifle final : public CWeapon
{
  public:
    CAssaultRifle() : CWeapon(WEAP_ASSAULTRIFLE) {}
    DBYTE GetAmmoType(bool alternate = false) const override
    {
        return alternate ? AMMO_DIEBUGDIE : data.m_nAmmoType;
    }
};
class CFlareGun final : public CWeapon
{
  public:
    CFlareGun() : CWeapon(WEAP_FLAREGUN) {}
};
// Original CTeslaCannon: only the second alternate shot since creation is a ball;
// the source never initializes or resets numAltFires (SetSpecialData has no caller).
class CTeslaCannon final : public CWeapon
{
  public:
    CTeslaCannon() : CWeapon(WEAP_TESLACANNON) {}
    DDWORD numAltFires = 0;
    std::uint64_t lightning_unavailable = 0;
    void BeginFire(bool alternate) override;
    FireResult FireProjectile() override;
};
class CSubMachineGun final : public CWeapon
{
  public:
    CSubMachineGun() : CWeapon(WEAP_SUBMACHINEGUN) {}
};
// Original CSoulCrowbar/CSoulAxe/CSoulPipe: every Fire draws IntRandom(0, 4) for the
// alternate slash sound, primary or not (the source writes m_szAltFireSound).
class CSoulWeapon final : public CWeapon
{
  public:
    explicit CSoulWeapon(DBYTE type) : CWeapon(type)
    {
        damage_type = OpenBlood2::DamageType::Melee;
    }
    const char* alt_sound = nullptr;
    void BeginFire(bool) override;
    const char* FireSound(bool alternate) const override
    {
        return alternate && alt_sound ? alt_sound : CWeapon::FireSound(alternate);
    }
};
// Original CShikariClaw: every Fire draws IntRandom(0, 4); the primary writes the
// alternate slash sound and the alternate writes the primary one (source swap).
class CShikariClaw final : public CWeapon
{
  public:
    CShikariClaw() : CWeapon(WEAP_SHIKARI_CLAW)
    {
        damage_type = OpenBlood2::DamageType::Melee;
    }
    const char *fire_sound = nullptr, *alt_sound = nullptr;
    void BeginFire(bool alternate) override;
    const char* FireSound(bool alternate) const override
    {
        const char* chosen = alternate ? alt_sound : fire_sound;
        return chosen ? chosen : CWeapon::FireSound(alternate);
    }
};
// Original CShikariSpit: an Acid loogie projectile with gravity removed.
class CShikariSpit final : public CWeapon
{
  public:
    CShikariSpit() : CWeapon(WEAP_SHIKARI_SPIT)
    {
        damage_type = OpenBlood2::DamageType::Acid;
    }
};
class CMelee final : public CWeapon
{
  public:
    CMelee() : CWeapon(WEAP_MELEE)
    {
        damage_type = OpenBlood2::DamageType::Melee;
    }
};
