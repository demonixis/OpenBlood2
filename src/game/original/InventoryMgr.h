#pragma once
#include "B2BaseClass.h"
#include "Flashlight.h"
#include "GameInvItems.h"
#include "Weapon.h"
#include "game/inventory_state.h"
#include <array>
#include <memory>
#include <optional>
#include <vector>

enum class InventoryResult
{
    Ok,
    AlreadyHave,
    Current,
    NotAvailable,
    Full,
    Invalid,
    Busy
};
class CInventoryMgr : public Aggregate
{
  public:
    ~CInventoryMgr() override;
    bool Save(OpenBlood2::InventoryState&, std::string&) const;
    // Restore before client association: rebuilding live client views is a
    // separate player-transfer responsibility, never silently bypassed here.
    bool Load(const OpenBlood2::InventoryState&, OpenBlood2::InventoryLoadType, std::string&);
    InventoryResult AddItem(DBYTE, DBYTE);
    InventoryResult AddInventoryWeapon(DBYTE, DBYTE);
    InventoryResult SelectInventoryWeapon(DBYTE);
    InventoryResult SelectNextWeapon();
    InventoryResult SetActiveItem(DBYTE);
    void RemoveDeathItems();
    std::optional<CInvMedkit> medkit;
    std::unique_ptr<CInvFlashlight> flashlight;
    std::optional<CInvBinoculars> binoculars;
    std::optional<CInvNightGoggles> goggles;
    std::unique_ptr<CInvTheEye> eye;
    InventoryResult AddKey(const OpenBlood2::KeyItem&);
    InventoryResult QueryKey(std::string_view);
    std::vector<OpenBlood2::KeyItem> keys;
    bool Init(HOBJECT, HCLIENT);
    void SetClient(HCLIENT client)
    {
        m_hClient = client;
    }
    void SetAmmoCount(DBYTE, float);
    float GetAmmoCount(DBYTE type) const
    {
        return type < m_fAmmo.size() ? m_fAmmo[type] : 0;
    }
    InventoryResult AddAmmo(DBYTE, float);
    float MaxAmmo(DBYTE) const;
    void SetStrength(DBYTE);
    void SetMagic(DBYTE);
    void ClearWeapons();
    bool UpdateHandModels();
    InventoryResult DropCurrentWeapon();
    std::uint64_t drop_attempts = 0, drops = 0;
    HOBJECT last_dropped = nullptr; // Non-owning; survives inventory/owner destruction.
    std::string drop_error;
    InventoryResult ObtainWeapon(DBYTE, int slot = -1);
    InventoryResult ChangeWeaponSlot(DBYTE);
    InventoryResult ChangeWeapon(DBYTE);
    bool FindBestWeapon(float target_distance, CWeapon*& alternative) const;
    CWeapon* GetWeapon(unsigned slot, bool left = false) const
    {
        return slot < inventory_weapon_slots ? (left ? m_LWeapons[slot].get() : m_Weapons[slot].get()) : nullptr;
    }
    CWeapon* CurrentWeapon(bool left = false) const
    {
        return GetWeapon(m_nCurWeaponSlot, left);
    }
    FireResult FireCurrentWeapon(const OpenBlood2::WeaponFire&);
    HOBJECT m_hOwner = nullptr, m_hMuzzleFlash = nullptr;
    HCLIENT m_hClient = nullptr;
    std::array<float, 13> m_fAmmo{};
    std::array<DBYTE, 9> requested_weapons{};
    bool m_bInfiniteAmmo = false;
    DBYTE m_nCurWeaponSlot = 0, m_nAttribStrength = 1, m_nAttribMagic = 1;
    float damage_multiplier = 1; // Original cumulative AddDamageMultiplier storage.
    std::uint64_t revision = 0;

  private:
    std::array<std::unique_ptr<CWeapon>, inventory_weapon_slots> m_Weapons{}, m_LWeapons{};
    DDWORD EngineMessageFn(LPBASECLASS, DDWORD, void*, DFLOAT) override;
};
