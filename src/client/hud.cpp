#include "client/hud.h"
#include "game/original/ClientCrosshair.h"
#include "game/original/WeaponDefs.h"
#include <algorithm>
#include <cmath>
namespace OpenBlood2
{
    HudState hud_state(const PlayerStats& s, bool in_world, bool external_camera, bool dead, bool show_crosshair,
                       DBYTE crosshair, DBYTE ctf_team, DBYTE ctf_flag)
    {
        HudState h;
        h.stats = s;
        h.crosshair = crosshair;
        h.ctf_team = ctf_team;
        h.ctf_flag = ctf_flag;
        h.crosshair_visible = show_crosshair && original_crosshair_visible(in_world, external_camera, dead);
        if (!in_world || external_camera)
            return h;
        h.visible = weapon_definition(s.weapon) && std::isfinite(s.health) && std::isfinite(s.armor) &&
                    std::isfinite(s.ammo) && std::isfinite(s.alternate_ammo) && std::isfinite(s.max_health) &&
                    std::isfinite(s.max_armor);
        return h;
    }
    bool valid_hud_stats(const PlayerStats& s)
    {
        return std::isfinite(s.max_health) && s.max_health >= 0 && std::isfinite(s.max_armor) && s.max_armor >= 0 &&
               std::isfinite(s.health) && std::isfinite(s.armor) && std::isfinite(s.bullets) && std::isfinite(s.ammo) &&
               s.ammo >= 0 && std::isfinite(s.alternate_ammo) && s.alternate_ammo >= 0 &&
               s.weapon_slot < inventory_weapon_slots && s.ammo_type < 13 && s.alternate_ammo_type < 13 &&
               (!s.weapon || weapon_definition(s.weapon)) &&
               std::all_of(
                   s.weapon_slots.begin(), s.weapon_slots.end(),
                   [](DBYTE type) { return !type || weapon_definition(type); });
    }
    bool valid_hud_items(const InventoryItems& s)
    {
        // Item pictures are relative to interface/statusbar/items/ (keys use authored names).
        const auto valid_item = [](const std::optional<InventoryItemView>& item)
        {
            const auto name = [](const std::string& s, bool path)
            {
                return s.size() <= 100 && s.find('\0') == std::string::npos &&
                       (!path || (!s.empty() && s.front() != '/' && s.front() != '\\' &&
                                  s.find(':') == std::string::npos && s.find("..") == std::string::npos));
            };
            return !item || (item->type >= 1 && item->type <= 9 && name(item->icon, true) &&
                             name(item->highlighted_icon, true) && name(item->name, false) &&
                             (item->type == 6 ? !item->name_id : item->name_id >= 301 && item->name_id <= 308));
        };
        return valid_item(s.current) && valid_item(s.previous) && valid_item(s.next);
    }
    bool ClientHud::begin(std::uint64_t world)
    {
        if (!world || world <= owner_.world)
            return false;
        owner_ = {world, {}};
        stats = {};
        items = {};
        return true;
    }
    bool ClientHud::bind(ClientObjectId owner)
    {
        if (owner.world != owner_.world || !owner.object || !owner.object.generation ||
            owner.object.slot >= ClientWorld::object_limit)
            return false;
        if (owner != owner_)
        {
            stats = {};
            items = {};
            owner_ = owner;
        }
        return true;
    }
    bool ClientHud::accepts(ClientObjectId owner, std::uint64_t sequence) const
    {
        return owner.object && owner == owner_ && sequence && sequence > sequence_;
    }
    bool ClientHud::receive(ClientObjectId owner, std::uint64_t sequence, const PlayerStats& value)
    {
        if (!accepts(owner, sequence) || !valid_hud_stats(value))
            return false;
        stats = value;
        sequence_ = sequence;
        return true;
    }
    bool ClientHud::receive(ClientObjectId owner, std::uint64_t sequence, const InventoryItems& value)
    {
        if (!accepts(owner, sequence) || !valid_hud_items(value))
            return false;
        items = value;
        items.show = false;
        if (value.show)
            ++item_shows;
        ++item_messages;
        sequence_ = sequence;
        return true;
    }
} // namespace OpenBlood2
