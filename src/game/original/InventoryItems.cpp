// Original InventoryMgr.cpp item cursor: SelectNextItem, SelectPrevItem, SelectItem,
// GetNextItem, GetPrevItem, GetItemCharges, the RemoveItem cursor rule and UpdateClient values.
#include "InventoryMgr.h"
#include <algorithm>

namespace
{
    // Shared/SharedDefs.h item types; ClientRes.h IDS_ITEM_* names; GameInvItems.cpp pictures.
    constexpr DBYTE INV_MEDKIT = 1, INV_FLASHLIGHT = 2, INV_NIGHTGOGGLES = 3, INV_THEEYE = 4, INV_BINOCULARS = 5,
                    INV_KEY = 6, INV_PROXIMITY = 7, INV_TIMEBOMB = 9;
    struct ItemArt
    {
        const char *icon, *highlighted;
        std::uint32_t name_id;
    };
    ItemArt item_art(DBYTE type)
    {
        switch (type)
        {
            case INV_MEDKIT:
                return {"medkit.pcx", "medkit_h.pcx", 302};
            case INV_FLASHLIGHT:
                return {"flashlight.pcx", "flashlight_h.pcx", 301};
            case INV_NIGHTGOGGLES:
                return {"night_vision.pcx", "night_vision_h.pcx", 303};
            case INV_THEEYE:
                return {"the_eye.pcx", "the_eye_h.pcx", 305};
            case INV_BINOCULARS:
                return {"binoculars.pcx", "binoculars_h.pcx", 304};
            case INV_PROXIMITY:
                return {"p_bombs.pcx", "p_bombs_h.pcx", 306};
            case INV_PROXIMITY + 1:
                return {"r_bombs.pcx", "r_bombs_h.pcx", 307};
            case INV_TIMEBOMB:
                return {"t_bombs.pcx", "t_bombs_h.pcx", 308};
            default:
                return {nullptr, nullptr, 0};
        }
    }
} // namespace

bool CInventoryMgr::ItemRef::operator==(const ItemRef& other) const
{
    return type == other.type && (type != INV_KEY || OpenBlood2::same_name(key, other.key));
}

std::vector<CInventoryMgr::ItemRef> CInventoryMgr::CarriedItems() const
{
    std::vector<ItemRef> items;
    const auto add = [&](DBYTE type, std::string key = {})
    {
        ItemRef item{type, std::move(key)};
        if (GetItemCharges(item) > 0) // The original Update removes zero-charge items.
            items.push_back(std::move(item));
    };
    if (medkit)
        add(INV_MEDKIT);
    if (flashlight)
        add(INV_FLASHLIGHT);
    if (goggles)
        add(INV_NIGHTGOGGLES);
    if (eye && eye->available)
        add(INV_THEEYE);
    if (binoculars)
        add(INV_BINOCULARS);
    for (const auto& k : keys)
        add(INV_KEY, k.name);
    for (DBYTE type = INV_PROXIMITY; type <= INV_TIMEBOMB; ++type)
        if (GetWeapon(10 + type - INV_PROXIMITY))
            add(type);
    return items;
}

// Original GetItemCharges: the item count for types 1..6, inventory-bomb ammo for 7..9.
DBYTE CInventoryMgr::GetItemCharges(const ItemRef& item) const
{
    switch (item.type)
    {
        case INV_MEDKIT:
            return medkit ? medkit->count : 0;
        case INV_FLASHLIGHT:
            return flashlight ? flashlight->count : 0;
        case INV_NIGHTGOGGLES:
            return goggles ? goggles->count : 0;
        case INV_THEEYE:
            return eye && eye->available ? 1 : 0;
        case INV_BINOCULARS:
            return binoculars ? 1 : 0;
        case INV_KEY:
            for (const auto& k : keys)
                if (OpenBlood2::same_name(k.name, item.key))
                    return k.uses;
            return 0;
        default:
            if (item.type >= INV_PROXIMITY && item.type <= INV_TIMEBOMB)
                if (const auto* w = GetWeapon(10 + item.type - INV_PROXIMITY))
                    return static_cast<DBYTE>(std::clamp(GetAmmoCount(w->GetAmmoType(false)), 0.f, 255.f));
            return 0;
    }
}

// Original RemoveItem: removing the current item selects the next one (wrapping to the
// first) when others remain, else none. Removal paths here are typed resets, consumed
// keys and zero charges, so the rule is applied against the list last checked.
void CInventoryMgr::ReconcileItemCursor()
{
    auto items = CarriedItems();
    if (current_item && std::find(items.begin(), items.end(), *current_item) == items.end())
    {
        const auto old = std::find(last_items_.begin(), last_items_.end(), *current_item);
        current_item.reset();
        if (old != last_items_.end())
            for (std::size_t step = 1; step < last_items_.size(); ++step)
            {
                const auto& candidate =
                    last_items_[(std::size_t(old - last_items_.begin()) + step) % last_items_.size()];
                if (std::find(items.begin(), items.end(), candidate) != items.end())
                {
                    current_item = candidate;
                    break;
                }
            }
    }
    last_items_ = std::move(items);
}
std::optional<CInventoryMgr::ItemRef> CInventoryMgr::GetCurrentItem()
{
    ReconcileItemCursor();
    return current_item;
}
void CInventoryMgr::SelectNextItem()
{
    ReconcileItemCursor();
    const auto& items = last_items_;
    const auto it = current_item ? std::find(items.begin(), items.end(), *current_item) : items.end();
    // Next link, or the first item when there is no next (or no cursor).
    const auto next = it == items.end() || it + 1 == items.end() ? items.begin() : it + 1;
    if (next == items.end())
    {
        current_item.reset();
        return;
    }
    show_items = true;
    current_item = *next;
}
void CInventoryMgr::SelectPrevItem()
{
    ReconcileItemCursor();
    const auto& items = last_items_;
    const auto it = current_item ? std::find(items.begin(), items.end(), *current_item) : items.end();
    if (items.empty())
    {
        current_item.reset();
        return;
    }
    // Previous link, or the last item when there is no previous (or no cursor).
    show_items = true;
    current_item = it == items.end() || it == items.begin() ? items.back() : *(it - 1);
}
// Original Load's SelectItem: the first item of that type; otherwise unchanged.
void CInventoryMgr::SelectItem(DBYTE type)
{
    ReconcileItemCursor();
    for (const auto& item : last_items_)
        if (item.type == type)
        {
            current_item = item;
            return;
        }
}
OpenBlood2::InventoryItems CInventoryMgr::ItemView()
{
    ReconcileItemCursor();
    const auto& items = last_items_;
    const auto view = [&](const ItemRef& item)
    {
        OpenBlood2::InventoryItemView v;
        v.type = item.type;
        v.charges = GetItemCharges(item);
        if (item.type == INV_KEY)
        {
            for (const auto& k : keys)
                if (OpenBlood2::same_name(k.name, item.key))
                {
                    v.icon = k.icon;
                    v.highlighted_icon = k.highlighted_icon;
                    v.name = k.display_name;
                }
        }
        else
        {
            const auto art = item_art(item.type);
            v.icon = art.icon;
            v.highlighted_icon = art.highlighted;
            v.name_id = art.name_id;
        }
        return v;
    };
    OpenBlood2::InventoryItems result;
    const auto it = current_item ? std::find(items.begin(), items.end(), *current_item) : items.end();
    if (it != items.end())
    {
        result.current = view(*it);
        // GetNextItem/GetPrevItem: the neighbouring links, never wrapping past the list head.
        if (it + 1 != items.end())
            result.next = view(*(it + 1));
        if (it != items.begin())
            result.previous = view(*(it - 1));
    }
    else if (!items.empty())
    {
        // No cursor: the head's neighbours, the first and the last item.
        result.next = view(items.front());
        result.previous = view(items.back());
    }
    result.show = std::exchange(show_items, false);
    return result;
}
