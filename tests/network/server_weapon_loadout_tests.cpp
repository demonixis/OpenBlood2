#include "game/game_settings.h"
#include "server/session.h"
#include <iostream>
using namespace OpenBlood2;
using namespace OpenBlood2::Net;
#define CHECK(x)                                                                                                       \
    do                                                                                                                 \
    {                                                                                                                  \
        if (!(x))                                                                                                      \
        {                                                                                                              \
            std::cerr << __LINE__ << ": " #x " " << error << '\n';                                                     \
            return false;                                                                                              \
        }                                                                                                              \
    } while (false)
namespace
{
    bool run(const char* data)
    {
        std::string error;
        Config config;
        config.data_dir = path_from_utf8(data);
        ServerSession session;
        CHECK(session.start(config, "multi/bb_crypt", GAMETYPE_BLOODBATH, error));
        Joined first, second;
        JoinProfile profile;
        CHECK(session.join(profile, first, error));
        profile.character = 1;
        CHECK(session.join(profile, second, error) && session.tick(error));
        MovementSetup a, b;
        WeaponLoadout x, y;
        CHECK(session.movement_setup(first.player, a, error) && session.movement_setup(second.player, b, error));
        CHECK(session.weapon_loadout(first.player, x, error) && session.weapon_loadout(second.player, y, error));
        CHECK(x.character == 0 && y.character == 1 && x.force_revision == a.owner.force_revision && x.weapons[0] == 1 &&
              (!x.weapons[1] && x.view_revision == std::array<std::uint64_t, 2>{1, 1}) && x.alive && x.input_enabled &&
              x.fire_allowed && !x.binoculars);
        std::vector<PrivateEvent> events;
        for (const auto& joined : {first, second})
        {
            CHECK(session.take_private_events(joined.player, events, error));
            unsigned loadouts = 0;
            for (const auto& event : events)
            {
                CHECK(event.owner == joined);
                loadouts += std::holds_alternative<WeaponLoadout>(event.value);
            }
            CHECK(loadouts == 1);
        }
        CHECK(session.weapon_loadout(first.player, x, error));
        CHECK(session.take_private_events(first.player, events, error) && events.empty());
        auto& game = const_cast<GameSession&>(session.game());
        const auto* object = game.objects.get(first.object);
        auto* player = dynamic_cast<CPlayerObj*>(object->game.get());
        CHECK(player);
        auto& inv = player->m_InventoryMgr;
        CHECK(inv.ObtainWeapon(WEAP_BERETTA) == InventoryResult::Ok);
        inv.SetAmmoCount(AMMO_BULLET, 37);
        CHECK(session.weapon_loadout(first.player, x, error));
        CHECK((x.weapons == std::array<DBYTE, 2>{1, 1}) && x.ammunition[0] == 37 && x.ammunition[1] == 37 &&
              (x.view_revision == std::array<std::uint64_t, 2>{1, 2}));
        CHECK(session.take_private_events(first.player, events, error) && events.size() == 1);
        CHECK(session.take_private_events(second.player, events, error) && events.empty());
        CHECK(inv.ChangeWeapon(WEAP_ASSAULTRIFLE) == InventoryResult::Ok);
        const auto* gun = inv.CurrentWeapon();
        CHECK(gun && gun->GetAmmoType(false) != gun->GetAmmoType(true));
        inv.SetAmmoCount(gun->GetAmmoType(false), 14);
        inv.SetAmmoCount(gun->GetAmmoType(true), 7);
        CHECK(session.weapon_loadout(first.player, x, error));
        CHECK(x.weapons[0] == WEAP_ASSAULTRIFLE && x.ammunition[0] == 14 && x.alternate_ammunition[0] == 7);
        player->movement_blocked = true;
        CHECK(session.weapon_loadout(first.player, x, error) && x.input_enabled && !x.fire_allowed);
        player->movement_blocked = false;
        player->m_bImprisoned = true;
        CHECK(session.weapon_loadout(first.player, x, error) && !x.fire_allowed);
        player->m_bImprisoned = false;
        player->camera_movement_blocked = true;
        CHECK(session.weapon_loadout(first.player, x, error) && !x.input_enabled && !x.fire_allowed);
        player->camera_movement_blocked = false;
        inv.binoculars.emplace(player->m_hObject);
        inv.binoculars->active = true;
        CHECK(session.weapon_loadout(first.player, x, error) && x.binoculars && x.input_enabled && !x.fire_allowed);
        inv.binoculars->active = false;
        const auto force = x.force_revision;
        const auto revisions = x.view_revision;
        CHECK(player->TeleportClientToServerPos() && session.movement_setup(first.player, a, error));
        CHECK(session.weapon_loadout(first.player, x, error) && x.force_revision > force && x.fire_allowed &&
              x.view_revision == revisions);
        CHECK(session.take_private_events(first.player, events, error));
        for (const auto& event : events)
        {
            std::vector<std::uint8_t> bytes;
            PrivateEvent decoded;
            CHECK(encode_private(event, bytes) && decode_private(bytes, decoded) == DecodePrivate::Ok &&
                  decoded.owner == first);
        }
        CHECK(session.leave(first.player) && !session.weapon_loadout(first.player, x, error));
        CHECK(session.weapon_loadout(second.player, y, error) && y.character == 1 && y.weapons[0] == 1);
        std::cout << "Private server loadouts: per-player character/ammo, dual hands, alternate ammo, "
                     "camera/imprisonment/binocular gates and force/departure passed\n";
        return true;
    }
} // namespace
int main(int argc, char** argv)
{
    return argc == 2 && run(argv[1]) ? 0 : 1;
}
