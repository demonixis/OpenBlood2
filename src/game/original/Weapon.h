#pragma once
#include "WeaponDefs.h"
#include "game/weapon_messages.h"

class CInventoryMgr;
enum class FireResult { Fired, Dry, Invalid, Unavailable, DeliveryFailed };
struct WeaponShot {
    blood2::WeaponFire request;
    DVector direction{}, end{};
    float damage=0;
    blood2::SegmentResult collision=blood2::SegmentResult::Miss;
    blood2::SegmentHit hit;
    bool damage_delivered=false;
    FireResult result=FireResult::Invalid;
};
// Selected vector-weapon path of CWeapon; no projectile/FX/audio facades.
class CWeapon {
protected:
    explicit CWeapon(DBYTE type) : data(*weapon_definition(type)) {}
public:
    virtual ~CWeapon() = default;
    void Init(HOBJECT owner,CInventoryMgr* inventory,bool left) { m_hOwner=owner; m_pInventoryMgr=inventory; m_bLeftHand=left; }
    float CheckAmmo() const;
    FireResult Fire(const blood2::WeaponFire&);
    const WeaponData& data;
    HOBJECT m_hOwner=nullptr;
    CInventoryMgr* m_pInventoryMgr=nullptr;
    bool m_bLeftHand=false;
    blood2::DamageType damage_type=blood2::DamageType::Normal;
    float m_fLastShotTime=0;
    bool m_bLastFireAlt=false;
    std::uint64_t shots=0;
    WeaponShot last_shot;
};
class CBeretta final : public CWeapon { public: CBeretta() : CWeapon(WEAP_BERETTA) {} };
class CMelee final : public CWeapon { public: CMelee() : CWeapon(WEAP_MELEE) { damage_type=blood2::DamageType::Melee; } };
