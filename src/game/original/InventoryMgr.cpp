#include "InventoryMgr.h"
#include "AI_Mgr.h"
#include "PlayerObj.h"
#include "game/game_settings.h"
#include <algorithm>
#include <cmath>
namespace
{
    constexpr float fMaxAmmoCount[13][6] = {
        // Strength or Magic attrib:
        //   1		2		3		4		5		6
        {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f},             // None
        {100.0f, 200.0f, 300.0f, 400.0f, 500.0f, 600.0f}, // Bullets
        {50.0f, 75.0f, 100.0f, 125.0f, 150.0f, 175.0f},   // Shells
        {20.0f, 40.0f, 60.0f, 80.0f, 100.0f, 120.0f},     // BMG Rounds
        {20.0f, 40.0f, 60.0f, 80.0f, 100.0f, 120.0f},     // Flares
        {25.0f, 50.0f, 75.0f, 100.0f, 150.0f, 200.0f},    // DieBugDie
        {5.0f, 10.0f, 20.0f, 35.0f, 50.0f, 120.0f},       // Rockets
        {25.0f, 25.0f, 50.0f, 75.0f, 100.0f, 125.0f},     // Fuel
        {100.0f, 200.0f, 300.0f, 400.0f, 500.0f, 600.0f}, // Batteries
        {100.0f, 200.0f, 300.0f, 400.0f, 500.0f, 600.0f}, // Focus
        {10.0f, 10.0f, 10.0f, 10.0f, 10.0f, 10.0f},       // ProximityBomb
        {10.0f, 10.0f, 10.0f, 10.0f, 10.0f, 10.0f},       // RemoteBomb
        {10.0f, 10.0f, 10.0f, 10.0f, 10.0f, 10.0f},       // TimeBomb
    };
}
bool CInventoryMgr::Init(HOBJECT object, HCLIENT client)
{
    auto* server = BaseClass::GetServerDE();
    m_hOwner = object;
    if (!m_hMuzzleFlash)
    {
        ObjectCreateStruct state;
        state.m_ObjectType = OT_LIGHT;
        if (auto* flash = server->CreateObject(server->GetClass("BaseClass"), &state))
        {
            m_hMuzzleFlash = flash->m_hObject;
            if (server->CreateInterObjectLink(m_hOwner, m_hMuzzleFlash) != DE_OK)
                return false;
        }
        else
            return false;
    }
    if (client)
        SetClient(client);
    return true;
}
CInventoryMgr::~CInventoryMgr()
{
    if (m_hMuzzleFlash)
        BaseClass::GetServerDE()->RemoveObject(m_hMuzzleFlash);
}
DDWORD CInventoryMgr::EngineMessageFn(LPBASECLASS object, DDWORD message, void* data, DFLOAT fData)
{
    if (message == MID_UPDATE)
        for (unsigned i = 0; i < m_Weapons.size(); ++i)
        {
            if (m_Weapons[i])
                m_Weapons[i]->UpdateSounds();
            if (m_LWeapons[i])
                m_LWeapons[i]->UpdateSounds();
        }
    if (message == MID_UPDATE)
        if (auto* ai = dynamic_cast<AI_Mgr*>(object);
            ai && ai->npc_type != AI_Mgr::NPCType::Diagnostic && ai->RuntimeReady())
            if (!UpdateHandModels())
                ai->RuntimeFailure("NPC held weapon update failed");
    if (message == MID_LINKBROKEN && data == m_hMuzzleFlash)
    {
        // The engine notifies on either endpoint's removal. Release this owned
        // child before clearing the handle, including when the owner is pending.
        BaseClass::GetServerDE()->RemoveObject(m_hMuzzleFlash);
        m_hMuzzleFlash = nullptr;
    }
    return Aggregate::EngineMessageFn(object, message, data, fData);
}
void CInventoryMgr::SetAmmoCount(DBYTE type, float count)
{
    if (type == 0 || type > 12 || m_bInfiniteAmmo || !std::isfinite(count) || count < 0)
        return;
    m_fAmmo[type] = count;
    ++revision;
}
float CInventoryMgr::MaxAmmo(DBYTE type) const
{
    if (type == 0 || type > 12)
        return 0;
    auto attribute = type == 9 ? m_nAttribMagic : m_nAttribStrength;
    return fMaxAmmoCount[type][std::clamp<int>(attribute, 1, 6) - 1];
}
void CInventoryMgr::SetStrength(DBYTE strength)
{
    // NPCs such as Gideon store strength 10. Only the ammo-table column is
    // capped at six; keep the typed boundary's existing minimum of one.
    strength = std::max<DBYTE>(strength, 1);
    const auto column = std::min<unsigned>(strength - 1, 5);
    if (m_nAttribStrength > strength)
        for (DBYTE i = 1; i <= 8; ++i)
            m_fAmmo[i] = std::min(m_fAmmo[i], fMaxAmmoCount[i][column]);
    m_nAttribStrength = strength;
    ++revision;
}
void CInventoryMgr::SetMagic(DBYTE magic)
{
    magic = std::clamp<DBYTE>(magic, 1, 6);
    if (m_nAttribMagic > magic)
        m_fAmmo[9] = std::min(m_fAmmo[9], fMaxAmmoCount[9][magic - 1]);
    m_nAttribMagic = magic;
    ++revision;
}
InventoryResult CInventoryMgr::AddAmmo(DBYTE type, float amount)
{
    if (type == 0 || type > 12 || !std::isfinite(amount) || amount < 0)
        return InventoryResult::Invalid;
    if (m_fAmmo[type] >= MaxAmmo(type))
        return InventoryResult::Full;
    m_fAmmo[type] = std::min(m_fAmmo[type] + amount, MaxAmmo(type));
    ++revision;
    return InventoryResult::Ok;
}
InventoryResult CInventoryMgr::ObtainWeapon(DBYTE type, int slot)
{
    const auto* definition = weapon_definition(type);
    if (!definition)
        return InventoryResult::NotAvailable;
    if (!m_hOwner || !BaseClass::GetServerDE()->HandleToObject(m_hOwner) || slot < -1 ||
        slot >= int(inventory_weapon_slots))
        return InventoryResult::Invalid;
    if ((type == WEAP_NAGA_EYEBEAM || type == WEAP_ZEALOT_ENERGYBLAST || type == WEAP_GIDEON_WIND ||
         type == WEAP_GIDEON_SHIELD) &&
        dynamic_cast<CPlayerObj*>(BaseClass::GetServerDE()->HandleToObject(m_hOwner)))
        return InventoryResult::NotAvailable; // Player view/save support is a separate contract.
    if (inventory_bomb(type))
    {
        const auto reserved = 10 + type - WEAP_PROXIMITYBOMB;
        if (slot != -1 && slot != reserved)
            return InventoryResult::Invalid;
        slot = 10 + type - WEAP_PROXIMITYBOMB;
    }
    else if (slot >= 10)
        return InventoryResult::Invalid;
    int empty = -1;
    bool left = false;
    if (slot >= 0)
    {
        auto* weapon = GetWeapon(static_cast<unsigned>(slot));
        if (!weapon)
            empty = slot;
        else if (weapon->data.m_nType == type)
        {
            if (definition->m_dwTwoHandStrengthReq && !GetWeapon(static_cast<unsigned>(slot), true))
            {
                empty = slot;
                left = true;
            }
            else
                return InventoryResult::AlreadyHave;
        }
    }
    if (empty < 0 && !inventory_bomb(type))
        for (unsigned i = 0; i < 10; ++i)
        {
            auto* weapon = GetWeapon(i);
            if (!weapon && empty < 0)
                empty = static_cast<int>(i);
            else if (weapon && weapon->data.m_nType == type)
            {
                if (definition->m_dwTwoHandStrengthReq && !GetWeapon(i, true))
                {
                    empty = static_cast<int>(i);
                    left = true;
                    break;
                }
                return InventoryResult::AlreadyHave;
            }
        }
    if (empty < 0)
        return InventoryResult::Full;
    // Original strength gate was commented out; do not introduce a new one.
    std::unique_ptr<CWeapon> weapon;
    if (type == WEAP_REMOTEBOMB)
        weapon = std::make_unique<CWeapRemoteBomb>();
    else if (type == WEAP_PROXIMITYBOMB)
        weapon = std::make_unique<CWeapProximityBomb>();
    else if (type == WEAP_BERETTA)
        weapon = std::make_unique<CBeretta>();
    else if (type == WEAP_SUBMACHINEGUN)
        weapon = std::make_unique<CSubMachineGun>();
    else if (type == WEAP_ASSAULTRIFLE)
        weapon = std::make_unique<CAssaultRifle>();
    else if (type == WEAP_FLAREGUN)
        weapon = std::make_unique<CFlareGun>();
    else if (type == WEAP_MELEE)
        weapon = std::make_unique<CMelee>();
    else if (type == WEAP_NAGA_EYEBEAM)
        weapon = std::make_unique<CNagaEyes>();
    else if (type == WEAP_ZEALOT_ENERGYBLAST)
        weapon = std::make_unique<CEnergyBlast>();
    else if (type == WEAP_GIDEON_SHIELD)
        weapon = std::make_unique<CGideonShield>();
    else if (type == WEAP_GIDEON_WIND)
        weapon = std::make_unique<CGideonWind>();
    else
        return InventoryResult::NotAvailable;
    weapon->Init(m_hOwner, this, left);
    (left ? m_LWeapons : m_Weapons)[static_cast<unsigned>(empty)] = std::move(weapon);
    ++revision;
    return InventoryResult::Ok;
}
InventoryResult CInventoryMgr::ChangeWeaponSlot(DBYTE slot)
{
    auto* weapon = GetWeapon(slot);
    if (!weapon)
        return InventoryResult::NotAvailable;
    if (CurrentWeapon() && CurrentWeapon()->data.m_nType == weapon->data.m_nType)
        return InventoryResult::Current;
    if (CurrentWeapon())
        CurrentWeapon()->ShowHandModel(false);
    if (CurrentWeapon(true))
        CurrentWeapon(true)->ShowHandModel(false);
    m_nCurWeaponSlot = slot;
    ++revision;
    return InventoryResult::Ok;
}
FireResult CInventoryMgr::FireCurrentWeapon(const OpenBlood2::WeaponFire& request)
{
    auto* weapon = CurrentWeapon(request.left);
    if (!weapon || weapon->data.m_nType != request.weapon)
        return FireResult::Invalid;
    return weapon->Fire(request);
}

void CInventoryMgr::ClearWeapons()
{
    for (auto& weapon : m_Weapons)
        weapon.reset();
    for (auto& weapon : m_LWeapons)
        weapon.reset();
    m_nCurWeaponSlot = 0;
    requested_weapons = {};
    ++revision;
}

InventoryResult CInventoryMgr::AddKey(const OpenBlood2::KeyItem& item)
{
    if (item.name.empty() || item.display_name.empty() || item.icon.empty() || item.highlighted_icon.empty() ||
        !item.uses)
        return InventoryResult::Invalid;
    for (const auto& key : keys)
        if (OpenBlood2::same_name(item.name, key.name))
            return InventoryResult::AlreadyHave;
    if (keys.size() == 256)
        return InventoryResult::Full;
    keys.push_back(item);
    ++revision;
    return InventoryResult::Ok;
}
InventoryResult CInventoryMgr::QueryKey(std::string_view name)
{
    for (auto it = keys.begin(); it != keys.end(); ++it)
        if (OpenBlood2::same_name(name, it->name))
        {
            if (--it->uses == 0)
                keys.erase(it);
            ++revision;
            return InventoryResult::Ok;
        }
    return InventoryResult::NotAvailable;
}

// Original Update's live-character hand-model branch and player dead hide branch.
bool CInventoryMgr::UpdateHandModels()
{
    auto* server = BaseClass::GetServerDE();
    auto* owner = server && server->IsObjectAlive(m_hOwner)
                      ? dynamic_cast<CBaseCharacter*>(server->HandleToObject(m_hOwner))
                      : nullptr;
    if (!owner)
        return false;
    bool ok = true;
    for (bool left : {false, true})
        if (auto* w = CurrentWeapon(left))
            ok = w->ShowHandModel(!owner->m_damage.IsDead()) && ok;
    return ok;
}

// Original InventoryMgr.cpp:772-837,908-939. Only supported weapon types exist.
InventoryResult CInventoryMgr::ChangeWeapon(DBYTE type)
{
    auto* server = BaseClass::GetServerDE();
    if (!server || !server->IsObjectAlive(m_hOwner) || !weapon_definition(type))
        return InventoryResult::Invalid;
    if (CurrentWeapon() && CurrentWeapon()->data.m_nType == type)
        return InventoryResult::Current;
    for (unsigned i = 0; i < 10; ++i)
        if (m_Weapons[i] && m_Weapons[i]->data.m_nType == type)
            return ChangeWeaponSlot(static_cast<DBYTE>(i));
    return InventoryResult::NotAvailable;
}
bool CInventoryMgr::FindBestWeapon(float distance, CWeapon*& alternative) const
{
    alternative = nullptr;
    auto* server = BaseClass::GetServerDE();
    if (!server || !server->IsObjectAlive(m_hOwner) || !std::isfinite(distance) || distance < 0)
        return false;
    auto* best = CurrentWeapon();
    for (unsigned i = 0; i < 10; ++i)
    {
        auto* candidate = m_Weapons[i].get();
        if (!candidate)
            continue;
        const auto damage = best ? static_cast<DDWORD>(best->data.m_fMaxDamage) : 0;
        const float ammo = best ? GetAmmoCount(best->data.m_nAmmoType) : 0;
        // Raw AMMO_NONE pool is deliberately zero here, unlike CheckAmmo().
        if (distance < candidate->data.m_fRange && candidate->data.m_nDamageRadius < distance &&
            GetAmmoCount(candidate->data.m_nAmmoType) &&
            (static_cast<DDWORD>(candidate->data.m_fMaxDamage) > damage || ammo <= 0))
            best = candidate;
    }
    if (best != CurrentWeapon())
        alternative = best;
    return true;
}

// Original DropCurrentWeapon: selected single-player NPC branch.
InventoryResult CInventoryMgr::DropCurrentWeapon()
{
    ++drop_attempts;
    drop_error.clear();
    auto* server = BaseClass::GetServerDE();
    if (server &&
        server->GetVarValueFloat(server->GetGameConVar("GAME_GAMETYPE")) != static_cast<float>(GAMETYPE_SINGLE))
    {
        drop_error = "Original weapon drops require single-player mode";
        return InventoryResult::NotAvailable;
    }
    auto* current = CurrentWeapon();
    if (!server || !server->IsObjectAlive(m_hOwner) || !current || !server->IsObjectAlive(current->m_hHandModel))
    {
        drop_error = "No live current held weapon";
        return InventoryResult::NotAvailable;
    }
    if (m_hClient)
    {
        drop_error = "Player drop input and view-weapon state synchronization unavailable";
        return InventoryResult::NotAvailable;
    }
    // Source NPC Fire returns to REST. A nested callback may not delete its weapon.
    if (current->firing || (CurrentWeapon(true) && CurrentWeapon(true)->firing))
    {
        drop_error = "Weapon is firing";
        return InventoryResult::Busy;
    }
    if (current->data.m_nType == WEAP_MELEE)
    {
        drop_error = "Original melee weapon cannot be dropped";
        return InventoryResult::NotAvailable;
    }
    auto& weapon = m_LWeapons[m_nCurWeaponSlot] ? m_LWeapons[m_nCurWeaponSlot] : m_Weapons[m_nCurWeaponSlot];
    const auto child = weapon->m_hHandModel;
    if (!weapon->DropHandModel())
    {
        drop_error = weapon->hand_error;
        return InventoryResult::Invalid;
    }
    last_dropped = child;
    weapon.reset(); // Ownership was transferred before destruction.
    ++drops;
    ++revision;
    return InventoryResult::Ok;
}

// Selected original AddItem/SetActiveItem branches; other items remain unsupported.
InventoryResult CInventoryMgr::AddItem(DBYTE type, DBYTE value)
{
    if (type != 1 || !BaseClass::GetServerDE()->IsObjectAlive(m_hOwner))
        return InventoryResult::Invalid;
    if (!medkit)
        medkit.emplace().Init(m_hOwner);
    else if (!medkit->AddItem(value))
        return InventoryResult::AlreadyHave;
    ++revision;
    return InventoryResult::Ok;
}
InventoryResult CInventoryMgr::SetActiveItem(DBYTE type)
{
    if (type == 7 || type == 8)
        return SelectInventoryWeapon(WEAP_PROXIMITYBOMB + type - 7);
    if (type != 1 || !medkit)
        return InventoryResult::NotAvailable;
    const auto before = medkit->count;
    const bool activated = medkit->ActivateItem();
    if (before != medkit->count)
        ++revision;
    // Original localized console item-name message / inventory HUD remain unavailable.
    return activated ? InventoryResult::Ok : InventoryResult::NotAvailable;
}

// Original AddInventoryWeapon/SelectInventoryWeapon and exhaustion fallback.
InventoryResult CInventoryMgr::AddInventoryWeapon(DBYTE type, DBYTE count)
{
    auto* server = BaseClass::GetServerDE();
    if (!server || !server->IsObjectAlive(m_hOwner))
        return InventoryResult::Invalid;
    if (type != 7 && type != 8)
        return InventoryResult::NotAvailable;
    const DBYTE ammo = AMMO_PROXIMITYBOMB + type - 7;
    if (GetWeapon(ammo))
        return AddAmmo(ammo, count);
    auto result = ObtainWeapon(WEAP_PROXIMITYBOMB + type - 7, ammo);
    if (result != InventoryResult::Ok)
        return result;
    AddAmmo(ammo, count); // Source ignores the first addition's return.
    return InventoryResult::Ok;
}
InventoryResult CInventoryMgr::SelectInventoryWeapon(DBYTE type)
{
    auto* server = BaseClass::GetServerDE();
    if (!server || !server->IsObjectAlive(m_hOwner))
        return InventoryResult::Invalid;
    if (!inventory_bomb(type))
        return InventoryResult::NotAvailable;
    const DBYTE slot = 10 + type - WEAP_PROXIMITYBOMB;
    if (!GetWeapon(slot))
        return InventoryResult::NotAvailable;
    if (m_nCurWeaponSlot == slot)
        return InventoryResult::Current;
    if (!GetAmmoCount(slot))
        return InventoryResult::NotAvailable;
    return ChangeWeaponSlot(slot);
}
InventoryResult CInventoryMgr::SelectNextWeapon()
{
    auto* server = BaseClass::GetServerDE();
    if (!server || !server->IsObjectAlive(m_hOwner))
        return InventoryResult::Invalid;
    if (m_nCurWeaponSlot >= 10)
        m_nCurWeaponSlot = 0; // Original resets before searching from slot1.
    unsigned slot = (m_nCurWeaponSlot + 1) % 10;
    while (slot != m_nCurWeaponSlot && !GetWeapon(slot))
        slot = (slot + 1) % 10;
    if (slot == m_nCurWeaponSlot)
        return InventoryResult::Current;
    return ChangeWeaponSlot(static_cast<DBYTE>(slot));
}
