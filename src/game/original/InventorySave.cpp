// Selected InventoryMgr.cpp:2843-3061 Save/Load branches: typed values, and their original
// byte layout for MID_SAVEOBJECT/MID_LOADOBJECT. Staging replaces destructive Term-before-read.
#include "InventoryMgr.h"
#include "SaveFields.h"
#include "SeeingEye.h"
#include <algorithm>
#include <cmath>
#include <string>

namespace
{
    bool valid(const OpenBlood2::InventoryState& state, std::string& error)
    {
        // Players keep 1..6 (ValidateAttributes); the original NPC strengths are 5 and Gideon's 10.
        if (state.strength < 1 || state.strength > 10 || state.magic < 1 || state.magic > 6 ||
            state.current_slot >= state.right.size() || state.current_item > 9 ||
            (state.current_slot >= 10 && !state.right[state.current_slot]) || !std::isfinite(state.damage_multiplier) ||
            state.damage_multiplier < 0 || state.damage_multiplier > 1000 || state.keys.size() > 256 ||
            (state.goggles && (state.goggles->count > 100 || !std::isfinite(state.goggles->remaining) ||
                               state.goggles->remaining < -60 || state.goggles->remaining > 30)) ||
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
            if ((i >= 10 && ((i > 12 && state.right[i]) ||
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
    if (goggles)
        state.goggles = OpenBlood2::FlashlightState{goggles->count, goggles->remaining};
    state.eye = eye && eye->available;
    state.binoculars = binoculars.has_value();
    state.current_slot = m_nCurWeaponSlot;
    if (current_item)
    {
        const auto items = CarriedItems();
        if (std::find(items.begin(), items.end(), *current_item) != items.end())
            state.current_item = current_item->type;
    }
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
    if (state.goggles)
    {
        staged.goggles.emplace(m_hOwner);
        staged.goggles->count = state.goggles->count;
        staged.goggles->remaining = state.goggles->remaining;
    }
    if (state.eye)
        staged.eye = std::make_unique<CInvTheEye>(m_hOwner);
    if (state.binoculars)
        staged.binoculars.emplace(m_hOwner); // Inactive; player Load separately clears zoom.
    // Commit only after every fallible service. Old child removal is deferred;
    // transferred weapons must point at this aggregate, never the local stage.
    m_Weapons.swap(staged.m_Weapons);
    m_LWeapons.swap(staged.m_LWeapons);
    std::swap(m_hMuzzleFlash, staged.m_hMuzzleFlash);
    keys.swap(staged.keys);
    medkit.swap(staged.medkit);
    flashlight.swap(staged.flashlight);
    binoculars.swap(staged.binoculars);
    goggles.swap(staged.goggles);
    eye.swap(staged.eye);
    m_fAmmo = staged.m_fAmmo;
    m_nAttribStrength = staged.m_nAttribStrength;
    m_nAttribMagic = staged.m_nAttribMagic;
    m_bInfiniteAmmo = staged.m_bInfiniteAmmo;
    m_nCurWeaponSlot = staged.m_nCurWeaponSlot;
    damage_multiplier = staged.damage_multiplier;
    requested_weapons = {}; // Initialization profile, not source saved weapons.
    current_item.reset();
    last_items_.clear();
    show_items = false;
    if (state.current_item)
        SelectItem(state.current_item); // Original Load: SelectItem(dwCurItem) after the items.
    for (unsigned i = 0; i < state.right.size(); ++i)
        for (bool left : {false, true})
            if (auto* weapon = GetWeapon(i, left))
                weapon->Init(m_hOwner, this, left);
    ++revision;
    return true;
}

namespace
{
    // Original Shared/SharedDefs.h item types and InventoryMgr.cpp's block marker.
    constexpr DBYTE INV_NONE = 0, INV_MEDKIT = 1, INV_FLASHLIGHT = 2, INV_NIGHTGOGGLES = 3, INV_THEEYE = 4,
                    INV_BINOCULARS = 5, INV_KEY = 6, INV_BASEINVWEAPON = 7;
    constexpr DDWORD inventory_marker = 0xcececece;
    constexpr unsigned original_weapon_slots = 10, original_inventory_weapons = 3;
} // namespace

// The original writes its item list in pickup order. This port keeps typed items rather than
// that intrusive list, so items are written in type order (medkit, flashlight, goggles, eye,
// binoculars, then keys in pickup order); a load restores that order. Non-key items carry
// their original picture names but no item names: the port has no server FormatString.
void CInventoryMgr::Save(HMESSAGEWRITE h, DDWORD)
{
    auto* s = BaseClass::GetServerDE();
    OpenBlood2::InventoryState state;
    std::string error;
    if (!Save(state, error))
    {
        s->RejectPayload(h, "CInventoryMgr Save: " + error);
        return;
    }
    const auto byte = [&](unsigned value) { s->WriteToMessageByte(h, static_cast<DBYTE>(value)); };
    s->WriteToMessageDWord(h, inventory_marker);
    byte(state.strength);
    byte(state.magic);
    byte(0); // m_bGodMode: this port has no inventory cheat.
    byte(state.infinite_ammo);
    for (unsigned i = 0; i < original_weapon_slots; ++i)
    {
        byte(state.right[i]); // Weapons are recreated from their types; no CWeapon state is saved.
        byte(state.left[i]);
    }
    for (unsigned i = 0; i < original_inventory_weapons; ++i)
    {
        const auto type = state.right[original_weapon_slots + i];
        byte(type ? type + INV_BASEINVWEAPON - WEAP_PROXIMITYBOMB : INV_NONE);
    }
    for (float ammo : state.ammo) // AMMO_MAXAMMOTYPES + 1 raw pools.
        s->WriteToMessageFloat(h, ammo);
    const bool eye_item = eye && eye->available;
    const DDWORD count = (medkit ? 1 : 0) + (flashlight ? 1 : 0) + (goggles ? 1 : 0) + (eye_item ? 1 : 0) +
                         (binoculars ? 1 : 0) + static_cast<DDWORD>(keys.size());
    s->WriteToMessageDWord(h, count);
    // The manager writes the type, then CInvItem::Save its own block (InvItem.h:193-207).
    const auto item = [&](DBYTE type, const char* name, const char* display, const char* pic, const char* pic_h,
                          bool dropped, bool active, DBYTE charges)
    {
        byte(type);
        s->WriteToLoadSaveMessageObject(h, m_hOwner);
        for (const char* text : {name, display, pic, pic_h})
            OpenBlood2::save_fields::write_nullable(h, text);
        byte(1); // m_bInitialized: Update marks every carried item initialized.
        byte(dropped);
        byte(type);
        byte(active);
        byte(charges);
    };
    if (medkit)
        item(INV_MEDKIT, nullptr, nullptr, "medkit.pcx", "medkit_h.pcx", false, false, medkit->count);
    if (flashlight)
    {
        item(INV_FLASHLIGHT, nullptr, nullptr, "flashlight.pcx", "flashlight_h.pcx", false, flashlight->active,
             flashlight->count);
        s->WriteToMessageFloat(h, flashlight->remaining); // m_fDischargeTime: a remaining duration.
    }
    if (goggles)
    {
        item(INV_NIGHTGOGGLES, nullptr, nullptr, "night_vision.pcx", "night_vision_h.pcx", false, goggles->active,
             goggles->count);
        s->WriteToMessageFloat(h, goggles->remaining);
    }
    if (eye_item)
    {
        item(INV_THEEYE, nullptr, nullptr, "the_eye.pcx", "the_eye_h.pcx", eye->eye != nullptr, eye->active, 1);
        s->WriteToLoadSaveMessageObject(h, eye->eye); // m_hSeeingObject.
    }
    if (binoculars)
        item(INV_BINOCULARS, nullptr, nullptr, "binoculars.pcx", "binoculars_h.pcx", false, binoculars->active, 1);
    for (const auto& key : keys)
        item(INV_KEY, key.name.c_str(), key.display_name.c_str(), key.icon.c_str(), key.highlighted_icon.c_str(), false,
             false, key.uses);
    s->WriteToMessageDWord(h, state.current_slot);
    const auto* current = CurrentWeapon();
    s->WriteToMessageDWord(h, current ? current->data.m_nType : 0); // m_nCurWeapon.
    s->WriteToMessageDWord(h, state.current_item);                  // Current item type (INV_NONE without one).
    s->WriteToMessageDWord(h, 0);                                   // m_WeapCount: never set by the original.
    s->WriteToMessageFloat(h, state.damage_multiplier);
    s->WriteToMessageFloat(h, 1); // m_fFireRateMultiplier: the original default, unchanged here.
    s->WriteToMessageFloat(h, 1); // m_fMeleeMultiplier: never set by the original.
    byte(0);                      // m_bDropEye.
    s->WriteToMessageDWord(h, inventory_marker);
}

void CInventoryMgr::Load(HMESSAGEREAD h, DDWORD param)
{
    auto* s = BaseClass::GetServerDE();
    OpenBlood2::InventoryState state;
    std::string problem;
    const auto refuse = [&](std::string why)
    {
        if (problem.empty())
            problem = std::move(why);
    };
    const auto byte = [&] { return s->ReadFromMessageByte(h); };
    const auto flag = [&]
    {
        const auto value = byte();
        if (value > 1)
            refuse("a saved boolean is out of range");
        return value != 0;
    };
    if (s->ReadFromMessageDWord(h) != inventory_marker)
        refuse("opening block marker is invalid");
    state.strength = byte();
    state.magic = byte();
    flag(); // m_bGodMode: no inventory cheat in this port.
    state.infinite_ammo = flag();
    for (unsigned i = 0; i < original_weapon_slots; ++i)
    {
        state.right[i] = byte();
        state.left[i] = byte();
    }
    for (unsigned i = 0; i < original_inventory_weapons; ++i)
        if (const auto type = byte(); type != INV_NONE)
        {
            // Save writes slot i's own bomb type; the original then calls AddInventoryWeapon(type, i).
            if (type != INV_BASEINVWEAPON + i)
                refuse("inventory weapon " + std::to_string(i) + " has type " + std::to_string(type));
            state.right[original_weapon_slots + i] = static_cast<DBYTE>(WEAP_PROXIMITYBOMB + i);
        }
    for (float& ammo : state.ammo)
        ammo = s->ReadFromMessageFloat(h);
    const DDWORD count = s->ReadFromMessageDWord(h);
    if (count > 5 + 256) // One of each typed item and the 256-key bound.
    {
        s->RejectPayload(h, "CInventoryMgr Load: item count exceeds the inventory bound");
        return;
    }
    HOBJECT eye_object = nullptr;
    for (DDWORD i = 0; i < count && problem.empty(); ++i)
    {
        const DBYTE type = byte();
        HOBJECT owner = nullptr;
        s->ReadFromLoadSaveMessageObject(h, &owner); // Read into a temporary and discarded, as the original.
        std::optional<std::string> name, display, pic, pic_h;
        for (auto* text : {&name, &display, &pic, &pic_h})
            if (!OpenBlood2::save_fields::read_nullable(h, *text))
                return;
        byte();                   // m_bInitialized.
        byte();                   // m_bDropped (uninitialized garbage for most original items).
        const DBYTE own = byte(); // m_nType.
        byte();                   // m_bIsActive: forced FALSE on every load.
        const DBYTE charges = byte();
        if (own != type)
            refuse("item block type differs from its list type");
        switch (type)
        {
            case INV_MEDKIT:
                if (state.medkit_charge)
                    refuse("duplicate medkit");
                state.medkit_charge = charges;
                break;
            case INV_FLASHLIGHT:
            case INV_NIGHTGOGGLES:
            {
                auto& slot = type == INV_FLASHLIGHT ? state.flashlight : state.goggles;
                if (slot)
                    refuse("duplicate charged item");
                slot = OpenBlood2::FlashlightState{charges, s->ReadFromMessageFloat(h)};
                break;
            }
            case INV_THEEYE:
                if (state.eye)
                    refuse("duplicate eye");
                state.eye = true;
                s->ReadFromLoadSaveMessageObject(h, &eye_object);
                break;
            case INV_BINOCULARS:
                if (state.binoculars)
                    refuse("duplicate binoculars");
                state.binoculars = true;
                break;
            case INV_KEY:
                state.keys.push_back(
                    {name.value_or(""), display.value_or(""), pic.value_or(""), pic_h.value_or(""), charges});
                break;
            default: // The original switch skips the item's data and desynchronizes.
                refuse("unsupported item type " + std::to_string(type));
                break;
        }
    }
    if (!problem.empty())
    {
        s->RejectPayload(h, "CInventoryMgr Load: " + problem);
        return;
    }
    const DDWORD slot = s->ReadFromMessageDWord(h);
    state.current_slot = static_cast<DBYTE>(slot < 0x100 ? slot : 0xFF); // Out of range: refused by the validator.
    s->ReadFromMessageDWord(h);                             // m_nCurWeapon: derived from the restored slot.
    const DDWORD current_item = s->ReadFromMessageDWord(h); // Original SelectItem: unknown types select nothing.
    state.current_item = static_cast<DBYTE>(current_item <= 9 ? current_item : INV_NONE);
    s->ReadFromMessageDWord(h); // m_WeapCount: never initialized by the original.
    state.damage_multiplier = s->ReadFromMessageFloat(h);
    s->ReadFromMessageFloat(h); // m_fFireRateMultiplier and m_fMeleeMultiplier are unused here.
    s->ReadFromMessageFloat(h);
    flag(); // m_bDropEye.
    if (s->ReadFromMessageDWord(h) != inventory_marker)
        refuse("closing block marker is invalid");
    std::string error;
    if (!problem.empty())
        s->RejectPayload(h, "CInventoryMgr Load: " + problem);
    else if (eye_object && !dynamic_cast<SeeingEye*>(s->HandleToObject(eye_object)))
        s->RejectPayload(h, "CInventoryMgr Load: the thrown eye reference is not a SeeingEye");
    else if (!Load(state,
                   param == 1 ? OpenBlood2::InventoryLoadType::KeepAlive : OpenBlood2::InventoryLoadType::Restore,
                   error))
        s->RejectPayload(h, "CInventoryMgr Load: " + error);
    else if (eye_object && eye)
    {
        // Original CInvTheEye::Load: SeeingEye::Init(owner, client) with the restore's NULL client.
        // The eye's own Load (later in the save order) reads the same owner again; the saved
        // eye->owner interlink is restored after every Load.
        eye->eye = eye_object;
        static_cast<SeeingEye*>(s->HandleToObject(eye_object))->m_hFiredFrom = m_hOwner;
    }
}
