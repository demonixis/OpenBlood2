#pragma once
#include "B2BaseClass.h"
#include "Weapon.h"
#include <array>
#include <memory>
#include <vector>

enum class InventoryResult
{
    Ok,
    AlreadyHave,
    Current,
    NotAvailable,
    Full,
    Invalid
};
class CInventoryMgr : public Aggregate
{
  public:
    ~CInventoryMgr() override;
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
    InventoryResult ObtainWeapon(DBYTE, int slot = -1);
    InventoryResult ChangeWeaponSlot(DBYTE);
    CWeapon* GetWeapon(unsigned slot, bool left = false) const
    {
        return slot < 10 ? (left ? m_LWeapons[slot].get() : m_Weapons[slot].get()) : nullptr;
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
    std::uint64_t revision = 0;

  private:
    std::array<std::unique_ptr<CWeapon>, 10> m_Weapons{}, m_LWeapons{};
    DDWORD EngineMessageFn(LPBASECLASS, DDWORD, void*, DFLOAT) override;
};
