#include "../player_fixture.h"
#include "game/game_settings.h"
#include "game/original/PickupObject.h"
#include <cmath>

using namespace OpenBlood2;
#define CHECK(x)                                                                                                       \
    do                                                                                                                 \
    {                                                                                                                  \
        if (!(x))                                                                                                      \
        {                                                                                                              \
            std::cerr << __LINE__ << ": " #x "\n";                                                                     \
            return false;                                                                                              \
        }                                                                                                              \
    } while (false)
namespace
{
    PickupObject* create(GameSession& g, const char* name, Creation reason = Creation::World)
    {
        const auto id = g.objects.create(g.objects.find_class(name), {}, reason, {{"Pos", DVector{300, 100, 300}}});
        const auto* record = g.objects.get(id);
        return record ? dynamic_cast<PickupObject*>(record->game.get()) : nullptr;
    }
    bool run(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(test_player::setup(g, root, true, true, true));
        std::string error;
        // Use the existing validated local player as the receiver of typed offers.
        // This does not exercise multiplayer player admission, which remains a later lot.
        CHECK(g.enter_player(error));
        auto* player = g.server_shell.Player(g.local_client);
        CHECK(player);
        g.server.set_game_settings(DIFFICULTY_MEDIUM, GAMETYPE_BLOODBATH);
        auto* ammo = create(g, "BulletAmmoPU");
        auto* health = create(g, "HealthPU");
        auto* armor = create(g, "WardPU");
        auto* medkit = create(g, "MedKitPU");
        CHECK(ammo && health && armor && medkit);
        CHECK(ammo->respawn_seconds == 15 && health->respawn_seconds == 30 && armor->respawn_seconds == 45 &&
              medkit->respawn_seconds == 60);
        CHECK(ammo->value.amount == 100 && medkit->value.amount == 1);
        player->m_InventoryMgr.SetAmmoCount(1, 0);
        const auto id = ammo->m_hObject;
        CHECK(g.objects.engine_message(id, MID_TOUCHNOTIFY, player->m_hObject));
        CHECK(ammo->picked_up && g.server.IsObjectAlive(id));
        CHECK(!(g.server.GetObjectFlags(id) & (FLAG_VISIBLE | FLAG_TOUCH_NOTIFY)));
        CHECK(player->m_InventoryMgr.GetAmmoCount(1) == 100);
        CHECK(g.objects.engine_message(id, MID_TOUCHNOTIFY, player->m_hObject));
        CHECK(player->m_InventoryMgr.GetAmmoCount(1) == 100); // Duplicate offer cannot collect again.
        CHECK(g.objects.advance(14.99) && ammo->picked_up);
        CHECK(g.objects.advance(.011) && !ammo->picked_up);
        CHECK(g.server.GetObjectFlags(id) == ammo->m_dwFlags);
        // The synthetic archive deliberately has no audio. Failure remains explicit.
        CHECK(ammo->error.starts_with("Respawn sound:"));
        CHECK(g.objects.engine_message(id, MID_TOUCHNOTIFY, player->m_hObject) && ammo->picked_up);
        CHECK(ammo->sound_requests == 2);

        g.server.multiplayer_pickups.ammo.level = PickupLevel::Half;
        auto* half = create(g, "BulletAmmoPU");
        CHECK(half && half->value.amount == 50);
        g.server.multiplayer_pickups.powerups.level = PickupLevel::Half;
        medkit = create(g, "MedKitPU");
        CHECK(medkit && medkit->value.amount == 1);
        g.server.multiplayer_pickups.health.level = PickupLevel::Insane;
        health = create(g, "HealthPU");
        CHECK(health && health->value.amount == 75);
        g.server.multiplayer_pickups.armor.level = PickupLevel::None;
        armor = create(g, "WardPU");
        CHECK(armor && armor->multiplayer_filtered && !g.server.IsObjectAlive(armor->m_hObject));
        auto* weapon = create(g, "BerettaPU");
        CHECK(weapon && weapon->multiplayer_filtered && !g.server.IsObjectAlive(weapon->m_hObject));
        auto* drop = create(g, "BulletAmmoPU", Creation::String);
        CHECK(drop && drop->respawn_seconds == 0);
        player->m_InventoryMgr.SetAmmoCount(1, 0);
        CHECK(g.objects.engine_message(drop->m_hObject, MID_TOUCHNOTIFY, player->m_hObject));
        CHECK(drop->picked_up && !g.server.IsObjectAlive(drop->m_hObject));
        CHECK(g.unload_world());
        return true;
    }
} // namespace
int main(int argc, char** argv)
{
    return argc == 2 && run(argv[1]) ? 0 : 1;
}
