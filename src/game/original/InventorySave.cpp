// Selected InventoryMgr.cpp:2843-3061 Save/Load branches. Typed values replace
// message serialization; staging replaces destructive Term-before-read.
#include "InventoryMgr.h"
#include <cmath>

namespace
{
    bool valid(const OpenBlood2::InventoryState& state, std::string& error)
    {
        if (state.strength < 1 || state.strength > 6 || state.magic < 1 || state.magic > 6 ||
            state.current_slot >= state.right.size() ||
            (state.current_slot >= 10 && !state.right[state.current_slot]) || !std::isfinite(state.damage_multiplier) ||
            state.damage_multiplier < 0 || state.damage_multiplier > 1000 || state.keys.size() > 256 ||
            (state.medkit_charge && *state.medkit_charge > 100) ||
            (state.flashlight && (state.flashlight->count > 100 || !std::isfinite(state.flashlight->remaining) ||
                                  state.flashlight->remaining < -60 || state.flashlight->remaining > 120)))
        {
            error = "Invalid inventory attributes, selection, multiplier or item count";
            return false;
        }
        for (auto ammo : state.ammo)
            if (!std::isfinite(ammo) || ammo < 0)
            {
                error = "Invalid inventory ammo";
                return false;
            }
        for (unsigned i = 0; i < state.right.size(); ++i)
        {
            const auto* right = weapon_definition(state.right[i]);
            if ((i >= 10 && ((i > 11 && state.right[i]) ||
                             (state.right[i] && state.right[i] != WEAP_PROXIMITYBOMB + i - 10) || state.left[i])) ||
                (i < 10 && inventory_bomb(state.right[i])) || (state.right[i] && !right) ||
                (state.left[i] && (!right || state.left[i] != state.right[i] || !right->m_dwTwoHandStrengthReq)))
            {
                error = "Unsupported weapon or invalid dual-hand inventory slot";
                return false;
            }
        }
        for (std::size_t i = 0; i < state.keys.size(); ++i)
        {
            const auto& key = state.keys[i];
            for (const auto* text : {&key.name, &key.display_name, &key.icon, &key.highlighted_icon})
                if (text->empty() || text->size() > 100 || text->find('\0') != std::string::npos)
                {
                    error = "Invalid saved key string";
                    return false;
                }
            if (!key.uses)
            {
                error = "Invalid saved key charge";
                return false;
            }
            for (std::size_t j = 0; j < i; ++j)
                if (OpenBlood2::same_name(key.name, state.keys[j].name))
                {
                    error = "Duplicate saved key";
                    return false;
                }
        }
        return true;
    }
} // namespace

bool CInventoryMgr::Save(OpenBlood2::InventoryState& output, std::string& error) const
{
    error.clear();
    auto* server = BaseClass::GetServerDE();
    if (!server || !server->IsObjectAlive(m_hOwner))
    {
        error = "Inventory save requires a live owner";
        return false;
    }
    OpenBlood2::InventoryState state;
    state.strength = m_nAttribStrength;
    state.magic = m_nAttribMagic;
    state.infinite_ammo = m_bInfiniteAmmo;
    for (unsigned i = 0; i < state.right.size(); ++i)
        for (bool left : {false, true})
            if (const auto* weapon = GetWeapon(i, left))
            {
                if (weapon->firing)
                {
                    error = "Inventory save during a weapon callback rejected";
                    return false;
                }
                (left ? state.left : state.right)[i] = weapon->data.m_nType;
            }
    state.ammo = m_fAmmo;
    state.keys = keys;
    if (medkit)
        state.medkit_charge = medkit->count;
    if (flashlight)
        state.flashlight = OpenBlood2::FlashlightState{flashlight->count, flashlight->remaining};
    state.current_slot = m_nCurWeaponSlot;
    state.damage_multiplier = damage_multiplier;
    if (!valid(state, error))
        return false;
    output = std::move(state);
    return true;
}

bool CInventoryMgr::Load(const OpenBlood2::InventoryState& state, OpenBlood2::InventoryLoadType type,
                         std::string& error)
{
    error.clear();
    auto* server = BaseClass::GetServerDE();
    if (!server || !server->IsObjectAlive(m_hOwner) || m_hClient)
    {
        error = "Inventory restore requires a live owner before client association";
        return false;
    }
    if (type != OpenBlood2::InventoryLoadType::Restore && type != OpenBlood2::InventoryLoadType::KeepAlive)
    {
        error = "Unsupported inventory load type";
        return false;
    }
    if (!valid(state, error))
        return false;
    for (unsigned i = 0; i < state.right.size(); ++i)
        for (bool left : {false, true})
            if (const auto* weapon = GetWeapon(i, left); weapon && weapon->firing)
            {
                error = "Inventory restore during a weapon callback rejected";
                return false;
            }

    CInventoryMgr staged;
    if (!staged.Init(m_hOwner, nullptr))
    {
        error = "Inventory restore could not create its muzzle child";
        return false;
    }
    staged.m_nAttribStrength = state.strength;
    staged.m_nAttribMagic = state.magic;
    staged.m_bInfiniteAmmo = state.infinite_ammo;
    for (unsigned i = 0; i < state.right.size(); ++i)
        for (auto weapon : {state.right[i], state.left[i]})
            if (weapon && staged.ObtainWeapon(weapon, static_cast<int>(i)) != InventoryResult::Ok)
            {
                error = "Inventory restore could not recreate a weapon";
                return false;
            }
    // Original Load assigns raw pools after obtaining weapons: no capacity
    // clamp, infinite-ammo setter guard or pickup reward may alter saved counts.
    staged.m_fAmmo = state.ammo;
    if (type != OpenBlood2::InventoryLoadType::KeepAlive)
        staged.keys = state.keys;                 // Source drops INV_KEY only for dwLoadType == 1.
    staged.m_nCurWeaponSlot = state.current_slot; // After recreation, as in source.
    staged.damage_multiplier = state.damage_multiplier;
    if (state.medkit_charge)
    {
        staged.medkit.emplace().Init(m_hOwner);
        staged.medkit->count = *state.medkit_charge;
    }

    if (state.flashlight)
    {
        staged.flashlight = std::make_unique<CInvFlashlight>(m_hOwner);
        staged.flashlight->count = state.flashlight->count;
        staged.flashlight->remaining = state.flashlight->remaining;
        if (!staged.flashlight->CreateLight())
        {
            error = staged.flashlight->error;
            return false;
        }
        // Original InvItem::Load clears active for both load modes; new child stays hidden.
    }
    // Commit only after every fallible service. Old child removal is deferred;
    // transferred weapons must point at this aggregate, never the local stage.
    m_Weapons.swap(staged.m_Weapons);
    m_LWeapons.swap(staged.m_LWeapons);
    std::swap(m_hMuzzleFlash, staged.m_hMuzzleFlash);
    keys.swap(staged.keys);
    medkit.swap(staged.medkit);
    flashlight.swap(staged.flashlight);
    m_fAmmo = staged.m_fAmmo;
    m_nAttribStrength = staged.m_nAttribStrength;
    m_nAttribMagic = staged.m_nAttribMagic;
    m_bInfiniteAmmo = staged.m_bInfiniteAmmo;
    m_nCurWeaponSlot = staged.m_nCurWeaponSlot;
    damage_multiplier = staged.damage_multiplier;
    requested_weapons = {}; // Initialization profile, not source saved weapons.
    for (unsigned i = 0; i < state.right.size(); ++i)
        for (bool left : {false, true})
            if (auto* weapon = GetWeapon(i, left))
                weapon->Init(m_hOwner, this, left);
    ++revision;
    return true;
}
