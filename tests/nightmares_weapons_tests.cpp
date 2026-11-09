#include "game/original/AI_Mgr.h"
#include "game/original/NightmaresProjectiles.h"
#include "game/original/WeaponPickups.h"
#include "game/server_adapter.h"
#include <iostream>

using namespace OpenBlood2;
#define CHECK(x)                                                                                                       \
    do                                                                                                                 \
    {                                                                                                                  \
        if (!(x))                                                                                                      \
        {                                                                                                              \
            std::cerr << __func__ << ':' << __LINE__ << ": " #x "\n";                                                  \
            return false;                                                                                              \
        }                                                                                                              \
    } while (false)

namespace
{
    bool mount(GameSession& game, const char* path)
    {
        Config config;
        config.data_dir = path;
        config.nightmares = true;
        std::string error;
        CHECK(game.initialize() && game.resources.mount_game(config, error));
        return true;
    }

    bool floor(GameSession& game)
    {
        WorldModel model;
        model.points = {{-4000, 0, -4000}, {4000, 0, -4000}, {4000, 0, 4000}, {-4000, 0, 4000}};
        model.surfaces.resize(1);
        model.surfaces[0].flags = 1;
        model.polygons = {{0, {0, 1, 2, 3}}};
        std::string error;
        CHECK(game.collision.build(model, error));
        game.server.collision_ready = true;
        return true;
    }

    AI_Mgr* actor(GameSession& game, DVector at, const char* weapon)
    {
        auto h = game.objects.create(game.objects.find_class("Fanatic"), {}, Creation::World,
                                     {{"Pos", at}, {"AIState", std::string("IDLE")}, {"AIWeapon1", std::string(weapon)}});
        auto* ai = dynamic_cast<AI_Mgr*>(game.server.HandleToObject(h));
        if (!ai || !ai->RuntimeReady())
            return nullptr;
        game.server.SetObjectState(h, OBJSTATE_INACTIVE);
        ai->m_damage.SetHitPoints(1000);
        ai->m_damage.SetArmorPoints(0);
        return ai;
    }

    bool step(GameSession& game, unsigned ticks = 1)
    {
        for (unsigned i = 0; i < ticks; ++i)
            if (!game.objects.advance(1. / 60) || !game.server.advance_projectiles(1. / 60))
            {
                for (const auto& error : game.objects.diagnostics())
                    std::cerr << error << '\n';
                return false;
            }
        return true;
    }

    bool pickups(const char* path)
    {
        auto game = std::make_unique<GameSession>();
        std::string error;
        CHECK(mount(*game, path) && GameSession::start_new_game(game, "01_subwaya", 1, 0, error));
        auto* player = game->server_shell.Player(game->local_client);
        CHECK(player);
        auto& inv = player->m_InventoryMgr;
        for (const auto type : {WEAP_COMBATSHOTGUN, WEAP_FLAYER})
        {
            ObjectCreateStruct state;
            CHECK(game->server.GetObjectPos(player->m_hObject, &state.m_Pos) == DE_OK);
            auto h = game->objects.create(game->objects.find_class(WeaponPickupClass(type)), state);
            auto* pickup = dynamic_cast<WeaponPickup*>(game->server.HandleToObject(h));
            CHECK(pickup && pickup->error.empty());
            CHECK(game->objects.engine_message(h, MID_TOUCHNOTIFY, player->m_hObject));
            CHECK(pickup->picked_up && inv.HasWeapon(type));
            CHECK(inv.ChangeWeapon(type) == InventoryResult::Ok);
            CHECK(game->client_shell.SyncWeapons(game->models, game->resources, game->server_shell, error));
        }
        return true;
    }

    bool gas(const char* path)
    {
        GameSession game;
        CHECK(mount(game, path) && floor(game));
        auto* shooter = actor(game, {0, 50, 0}, "COMBAT");
        auto* victim = actor(game, {0, 50, 150}, "BERETTA");
        CHECK(shooter && victim);
        auto* gun = dynamic_cast<CCombatShotgun*>(shooter->m_InventoryMgr.CurrentWeapon());
        auto& inventory = shooter->m_InventoryMgr;
        CHECK(gun && gun->GetAmmoType() == AMMO_SHELL && gun->GetAmmoType(true) == AMMO_DIEBUGDIE);
        inventory.SetAmmoCount(AMMO_SHELL, 8);
        inventory.SetAmmoCount(AMMO_DIEBUGDIE, 10);
        DVector position;
        CHECK(game.server.GetObjectPos(shooter->m_hObject, &position) == DE_OK);
        position.z += 35;
        CHECK(gun->Fire({position, {0, 0, -1}, 9, WEAP_COMBATSHOTGUN, false, false}) == FireResult::Fired &&
              gun->last_pellets == 16 && inventory.GetAmmoCount(AMMO_SHELL) == 6);
        CHECK(gun->Fire({position, {0, 0, 1}, 9, WEAP_COMBATSHOTGUN, true, false}) == FireResult::Fired &&
              inventory.GetAmmoCount(AMMO_DIEBUGDIE) == 5 && inventory.GetAmmoCount(AMMO_SHELL) == 6);
        const auto handle = gun->projectile;
        auto* grenade = dynamic_cast<CGasGrenade*>(game.server.HandleToObject(handle));
        CHECK(grenade && grenade->m_fVelocity == 1500 && grenade->trail && grenade->trail_error.empty());
        for (unsigned i = 0; i < 120 && !grenade->smoking; ++i)
            CHECK(step(game) && game.server.IsObjectAlive(handle));
        CHECK(grenade->smoking && grenade->explosion_passes == 1);
        const float before = victim->m_damage.m_fHitPoints;
        CHECK(step(game, 65) && grenade->gas_ticks >= 2 && grenade->gas_deliveries >= 2 &&
              victim->m_damage.m_fHitPoints < before);
        CHECK(step(game, 600) && !game.server.IsObjectAlive(handle));
        return true;
    }

    bool flayer(const char* path)
    {
        GameSession game;
        CHECK(mount(game, path) && floor(game));
        auto* shooter = actor(game, {0, 50, 0}, "FLAYER");
        auto* victim = actor(game, {0, 50, 135}, "BERETTA");
        CHECK(shooter && victim);
        auto* weapon = dynamic_cast<CFlayer*>(shooter->m_InventoryMgr.CurrentWeapon());
        auto& inventory = shooter->m_InventoryMgr;
        CHECK(weapon);
        inventory.SetAmmoCount(AMMO_FOCUS, 100);
        DVector position;
        CHECK(game.server.GetObjectPos(shooter->m_hObject, &position) == DE_OK);
        position.z += 35;
        CHECK(weapon->Fire({position, {0, 0, 1}, 42, WEAP_FLAYER, false, false}) == FireResult::Fired &&
              inventory.GetAmmoCount(AMMO_FOCUS) == 75);
        const auto handle = weapon->projectile;
        auto* hook = dynamic_cast<CFlayerChain*>(game.server.HandleToObject(handle));
        CHECK(hook);
        for (unsigned i = 0; i < 120 && hook->state == CFlayerChain::State::Tracking; ++i)
            CHECK(step(game) && game.server.IsObjectAlive(handle));
        CHECK(hook->state == CFlayerChain::State::Attached && victim->m_nTrapped == 1);
        CHECK(step(game, 450) && game.server.IsObjectAlive(handle) && hook->damage_deliveries >= 6);
        CHECK(step(game, 55) && hook->state == CFlayerChain::State::Retracting && victim->m_nTrapped == 0);
        const auto gib = hook->gib;
        CHECK(gib && hook->gib_attachment && step(game, 90));
        CHECK(!game.server.IsObjectAlive(handle) && !game.server.IsObjectAlive(gib));

        inventory.SetAmmoCount(AMMO_FOCUS, 100);
        CHECK(weapon->Fire({position, {0, 0, 1}, 42, WEAP_FLAYER, true, false}) == FireResult::Fired &&
              inventory.GetAmmoCount(AMMO_FOCUS) == 0);
        const auto portal_handle = weapon->projectile;
        auto* portal = dynamic_cast<CFlayerPortal*>(game.server.HandleToObject(portal_handle));
        CHECK(portal && step(game, 230) && portal->chains_created == 0);
        CHECK(step(game, 25) && portal->chains_created == 1 && game.server.IsObjectAlive(portal->last_chain));
        return true;
    }
} // namespace

int main(int argc, char** argv)
{
    if (argc != 3)
        return 2;
    const std::string_view test = argv[1];
    return (test == "pickups" ? pickups(argv[2]) : test == "gas" ? gas(argv[2]) : test == "flayer" && flayer(argv[2]))
               ? 0
               : 1;
}
