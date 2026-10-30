#include "../player_fixture.h"
#include "game/game_settings.h"
#include "game/original/CharacterProfiles.h"
#include <algorithm>
#include <array>
#include <iostream>
#include <random>

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
    bool fresh(const std::filesystem::path& root)
    {
        GameSession game;
        CHECK(test_player::setup(game, root));
        game.server.net_game_type = GAMETYPE_BLOODBATH;
        auto first = game.server.connect_client(), second = game.server.connect_client();
        CHECK(first && second);
        auto* player = game.server_shell.OnClientEnterWorld(first);
        auto* other = game.server_shell.OnClientEnterWorld(second);
        CHECK(player && other);
        std::vector<PlayerEvent> events;
        CHECK(game.server.take_client_events(first, events));
        CHECK(game.server.take_client_events(second, events));
        PlayerInit init;
        init.name = "Caleb";
        init.weapons[0] |= DUAL_HAND_MASK;
        std::minstd_rand expected(game.server.save_clock().generator);
        const auto index = std::uniform_int_distribution<int>(0, 0xffff)(expected);
        const auto expected_next = expected();
        CHECK(game.server_shell.OnMessage(first, init));
        CHECK(player->attributes_received && !other->attributes_received && player->respawns == 2);
        CHECK(player->m_InventoryMgr.GetWeapon(0) && player->m_InventoryMgr.GetWeapon(1));
        CHECK(player->m_InventoryMgr.GetWeapon(1, true));
        CHECK(player->m_InventoryMgr.GetAmmoCount(AMMO_BULLET) == 50);
        CHECK(player->m_InventoryMgr.GetAmmoCount(AMMO_SHELL) == 8);
        CHECK(player->m_InventoryMgr.GetAmmoCount(AMMO_FOCUS) == 0);
        CHECK(game.server.take_client_events(first, events) && !events.empty());
        CHECK(std::get<BloodbathStart>(events.front()).index == index);
        CHECK(std::count_if(events.begin(), events.end(),
                            [](const auto& e) { return std::holds_alternative<BloodbathStart>(e); }) == 1);
        CHECK(game.server.take_client_events(second, events) && events.empty());
        std::minstd_rand actual(game.server.save_clock().generator);
        CHECK(actual() == expected_next);
        const auto clock = game.server.save_clock();
        CHECK(!game.server_shell.OnMessage(first, init));
        CHECK(game.server.save_clock() == clock);
        CHECK(game.server.take_client_events(first, events) && events.empty());
        // A malformed profile fails before the entry announcement or any random draw.
        init.character = 4;
        CHECK(!game.server_shell.OnMessage(second, init));
        init.character = 0;
        init.skin = 7;
        CHECK(!game.server_shell.OnMessage(second, init));
        init.skin = 0;
        init.weapons[0] = WEAP_GIDEON_SPEAR;
        CHECK(!game.server_shell.OnMessage(second, init));
        init.weapons[0] = WEAP_BERETTA;
        init.name = std::string("bad\0name", 8);
        CHECK(!game.server_shell.OnMessage(second, init));
        CHECK(game.server.save_clock() == clock);
        CHECK(game.server.take_client_events(second, events) && events.empty());
        CHECK(!other->attributes_received && !other->m_InventoryMgr.GetWeapon(0));
        init.name = "other";
        init.strength = init.speed = init.resistance = init.magic = 5;
        CHECK(game.server_shell.OnMessage(second, init));
        CHECK(other->m_nAttribStrength == 3 && other->m_nAttribSpeed == 3 && other->m_nAttribResistance == 3 &&
              other->m_nAttribMagic == 3);
        CHECK(other->m_damage.m_fHitPoints == 100 && other->m_fMoveVel == 280);
        CHECK(game.client_shell.OnEnterWorld(first));
        CHECK(game.client_shell.OnMessage(BloodbathStart{static_cast<std::uint16_t>(index)}, game.server_shell));
        const auto& comm = game.client_shell.communication;
        CHECK(comm.started == 1 && comm.voice_manager_requests == 0 &&
              comm.portrait == "interface/commlink/v_comm.pcx");
        CHECK(game.client_shell.OnMessage(BloodbathStart{0}, game.server_shell));
        CHECK(comm.busy == 1 && comm.voice_manager_requests == 0);
        CHECK(game.client_session.announcer.messages == 2 && game.client_session.announcer.fallback_texts == 2);
        game.client_shell.OnExitWorld();
        for (auto client : {first, second})
        {
            game.server_shell.OnClientExitWorld(client);
            CHECK(game.objects.finish_frame() && game.server.disconnect_client(client));
        }
        CHECK(game.unload_world());
        return true;
    }

    bool ammo(const std::filesystem::path& root)
    {
        for (auto level : {PickupLevel::None, PickupLevel::Half, PickupLevel::Normal, PickupLevel::Double})
        {
            GameSession game;
            CHECK(test_player::setup(game, root));
            game.server.net_game_type = GAMETYPE_BLOODBATH;
            game.server.multiplayer_pickups.ammo.level = level;
            auto client = game.server.connect_client();
            auto* player = game.server_shell.OnClientEnterWorld(client);
            CHECK(player && game.server_shell.OnMessage(client, PlayerInit{}));
            const auto& inv = player->m_InventoryMgr;
            std::array<float, 13> expected{};
            if (level == PickupLevel::Half)
            {
                expected[1] = 25;
                expected[2] = 4;
            }
            if (level == PickupLevel::Normal)
                expected = {0, 50, 8, 5, 10, 5, 0, 10, 20, 0, 0, 0, 0};
            if (level == PickupLevel::Double)
                expected = {0, 100, 16, 10, 20, 10, 5, 20, 40, 0, 0, 0, 0};
            for (unsigned i = 0; i < expected.size(); ++i)
                CHECK(inv.GetAmmoCount(DBYTE(i)) == expected[i]);
            CHECK(!inv.m_bInfiniteAmmo);
            game.server_shell.OnClientExitWorld(client);
            CHECK(game.objects.finish_frame() && game.server.disconnect_client(client));
        }
        return true;
    }

    bool private_profiles(const std::filesystem::path& root)
    {
        GameSession game;
        CHECK(game.initialize());
        Config config;
        config.data_dir = root;
        std::string error;
        CHECK(game.resources.mount_game(config, error));
        WorldRequest world;
        world.filename = "worlds/multi/bb_crypt.dat";
        world.game_type = GAMETYPE_BLOODBATH;
        CHECK(game.load_request(world, error));
        // Verify every retail body/skin against mounted resources; no synthetic replacements.
        for (DBYTE character = 0; character < 4; ++character)
            for (DBYTE skin = 0; skin < 7; ++skin)
            {
                const auto client = game.server.connect_client();
                auto* player = game.server_shell.OnClientEnterWorld(client);
                CHECK(player);
                PlayerInit init;
                CHECK(select_character(character, init));
                init.name = "profile";
                init.skin = skin;
                init.weapons = {1, 2, 3, 4, 5,
                                6, 7, 8, 9}; // Original absent-B2C fallback, separate from solo's one Beretta.
                CHECK(game.server_shell.OnMessage(client, init));
                CHECK(player->m_nCharacter == character && player->m_nSkin == skin);
                const auto* object = game.objects.get(player->m_hObject);
                CHECK(object && object->model &&
                      std::string(object->state.m_SkinName) == character_skin(character, skin));
                for (int i = 1; i <= 9; ++i)
                    CHECK(player->m_InventoryMgr.GetWeapon(i) &&
                          player->m_InventoryMgr.GetWeapon(i)->data.m_nType == i);
                std::vector<PlayerEvent> events;
                CHECK(game.server.take_client_events(client, events));
                const auto physics = std::find_if(events.rbegin(), events.rend(), [](const auto& e)
                                                  { return std::holds_alternative<PlayerPhysics>(e); });
                CHECK(physics != events.rend() &&
                      std::get<PlayerPhysics>(*physics).skin == character_skin(character, skin));
                game.server_shell.OnClientExitWorld(client);
                CHECK(game.objects.finish_frame() && game.server.disconnect_client(client));
            }
        game.server.multiplayer_pickups.ammo.level = PickupLevel::Insane;
        const auto client = game.server.connect_client();
        auto* player = game.server_shell.OnClientEnterWorld(client);
        CHECK(player && game.server_shell.OnMessage(client, PlayerInit{}));
        CHECK(player->m_InventoryMgr.m_bInfiniteAmmo);
        for (DBYTE i = AMMO_BULLET; i <= AMMO_FOCUS; ++i)
            CHECK(player->m_InventoryMgr.GetAmmoCount(i) == player->m_InventoryMgr.MaxAmmo(i));
        for (int i = 10; i <= 12; ++i)
            CHECK(player->m_InventoryMgr.GetWeapon(i));
        const auto bullets = player->m_InventoryMgr.GetAmmoCount(AMMO_BULLET);
        player->m_InventoryMgr.SetAmmoCount(AMMO_BULLET, 0);
        CHECK(player->m_InventoryMgr.GetAmmoCount(AMMO_BULLET) == bullets);
        game.server_shell.OnClientExitWorld(client);
        CHECK(game.objects.finish_frame() && game.server.disconnect_client(client));
        CHECK(game.unload_world());
        std::cout << "28 retail profiles and all nine fallback weapons; insane ammo and three bomb slots\n";
        return true;
    }
} // namespace
int main(int argc, char** argv)
{
    if (argc != 3)
        return 2;
    const std::string name = argv[1];
    return (name == "fresh"  ? fresh(argv[2])
            : name == "ammo" ? ammo(argv[2])
                             : name == "private" && private_profiles(argv[2]))
               ? 0
               : 1;
}
