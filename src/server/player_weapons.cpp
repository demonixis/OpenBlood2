#include "server/session.h"
#include <algorithm>
namespace OpenBlood2
{
    bool ServerSession::weapon_loadout(std::uint64_t id, Net::WeaponLoadout& out, std::string& error)
    {
        error.clear();
        const auto found = std::find_if(players_.begin(), players_.end(), [id](const auto& p) { return p.id == id; });
        if (found == players_.end() || !found->setup.owner.force_revision)
        {
            error = "Weapon recipient requires a checked movement setup";
            return false;
        }
        auto* player = game_.server_shell.Player(found->client);
        const auto* owner = game_.objects.get(found->object);
        if (!player || !owner || owner->life != Life::Alive || owner->handle != player->m_hObject)
        {
            error = "Weapon owner is unavailable";
            return false;
        }
        const auto& inventory = player->m_InventoryMgr;
        Net::WeaponLoadout next;
        next.force_revision = found->setup.owner.force_revision;
        next.character = player->m_nCharacter;
        next.slot = inventory.m_nCurWeaponSlot;
        next.alive = !player->m_damage.IsDead();
        next.input_enabled = !player->camera_movement_blocked && !(inventory.eye && inventory.eye->active);
        next.binoculars = inventory.binoculars && inventory.binoculars->active;
        next.fire_allowed = next.alive && next.input_enabled && player->attributes_received &&
                            !player->MovementBlocked() && !next.binoculars;
        for (unsigned hand = 0; hand < 2; ++hand)
            if (const auto* weapon = inventory.CurrentWeapon(hand != 0))
            {
                next.weapons[hand] = weapon->data.m_nType;
                next.ammunition[hand] = weapon->CheckAmmo(false);
                next.alternate_ammunition[hand] = weapon->CheckAmmo(true);
            }
        if (!found->weapons || *found->weapons != next)
        {
            if (queue_private(*found, found->object, next) != DE_OK)
            {
                error = "Private weapon state queue refused";
                return false;
            }
            found->weapons = next;
        }
        out = next;
        return true;
    }
} // namespace OpenBlood2
