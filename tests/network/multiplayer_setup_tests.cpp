#include "app/multiplayer_setup.h"
#include "game/game_settings.h"
#include "game/menu.h"
#include <iostream>
using namespace OpenBlood2;
#define CHECK(x)                                                                                                       \
    do                                                                                                                 \
    {                                                                                                                  \
        if (!(x))                                                                                                      \
        {                                                                                                              \
            std::cerr << __LINE__ << ": " #x ": " << error << '\n';                                                    \
            return false;                                                                                              \
        }                                                                                                              \
    } while (false)
namespace
{
    bool run(const char* data)
    {
        std::string error;
        MultiplayerSetup setup;
        setup.maps = {"worlds/multi/bb_crypt.dat", "worlds/multi/ctf_jam.dat"};
        Options request;
        CHECK(setup.request(true, request, error));
        CHECK(request.host_world == setup.maps[0] && request.network_rules.minutes == 15 &&
              request.network_rules.frags == 25);
        setup.mode = 2;
        setup.character = 3;
        setup.team = 2;
        setup.only_flag_scores = true;
        CHECK(setup.request(true, request, error) && request.network_mode == GAMETYPE_CTF &&
              request.host_world == setup.maps[1] && request.network_character == 3 && request.network_team == 2 &&
              request.network_rules.only_flag_scores);
        setup.rotation = {setup.maps[0]};
        CHECK(!setup.request(true, request, error));
        setup.rotation = {setup.maps[1]};
        CHECK(setup.request(true, request, error));
        setup.port = 0;
        CHECK(!setup.request(true, request, error));
        setup.port = 27889; // LAN discovery's own socket.
        CHECK(!setup.request(true, request, error) && error.find("27889") != std::string::npos);
        setup.port = 27888;
        setup.minutes = 10001; // MultiplayerRules::minute_limit + 1.
        CHECK(!setup.request(true, request, error));
        setup.minutes = 15;
        setup.frags = -1;
        CHECK(!setup.request(true, request, error));
        setup.frags = 25;
        setup.name[0] = '\0';
        CHECK(!setup.request(false, request, error));
        setup.name[0] = 'P';
        CHECK(setup.request(false, request, error) && request.host_world.empty() &&
              request.connect_address == "127.0.0.1:27888");
        Menu menu;
        menu.multiplayer = true;
        menu.open(false);
        menu.selected = 1;
        menu.activate();
        CHECK(menu.take_command().action == MenuAction::Multiplayer && menu.active);
        Menu demo(MenuEdition::Demo);
        demo.open(false);
        demo.selected = 1;
        demo.activate();
        CHECK(demo.promotion == MenuPromotion::Bloodbath && demo.take_command().action == MenuAction::None);
        if (data)
        {
            Config config;
            config.data_dir = path_from_utf8(data);
            Resources resources;
            CHECK(resources.mount_game(config, error) && setup.load(resources, config, error));
            CHECK(setup.maps.size() == 22 && setup.mode_maps().size() == 4);
            setup.mode = 0;
            CHECK(setup.mode_maps().size() == 18);
            config.demo = true;
            CHECK(!setup.load(resources, config, error));
        }
        std::cout << "Multiplayer menus: retail entry/demo promotion, typed host/direct join, profile, "
                     "original host defaults, map/mode rotation validation and invalid requests passed\n";
        return true;
    }
} // namespace
int main(int argc, char** argv)
{
    return argc <= 2 && run(argc == 2 ? argv[1] : nullptr) ? 0 : 1;
}
