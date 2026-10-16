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
    float CheckAmmo() const;
    FireResult Fire(const OpenBlood2::WeaponFire&);
    virtual const char* FireSound(bool alternate) const;
    virtual FireResult FireProjectile();
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
    FireResult FireProjectile() override;
    HOBJECT projectile = nullptr; // Checked, independent runtime child.
    std::string projectile_error;
};
class CBeretta final : public CWeapon
{
  public:
    CBeretta() : CWeapon(WEAP_BERETTA) {}
};
class CMelee final : public CWeapon
{
  public:
    CMelee() : CWeapon(WEAP_MELEE)
    {
        damage_type = OpenBlood2::DamageType::Melee;
    }
};
