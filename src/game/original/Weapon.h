#pragma once
#include "WeaponDefs.h"
#include "game/weapon_messages.h"
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
    bool damage_delivered = false;
    FireResult result = FireResult::Invalid;
};
// Selected vector-weapon path of CWeapon; no projectile/FX/audio facades.
class CWeapon
{
  protected:
    explicit CWeapon(DBYTE type) : data(*weapon_definition(type)) {}

  public:
    virtual ~CWeapon();
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
