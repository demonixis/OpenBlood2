// Nightmares lot N2 (nightmares_edition_contracts.md): WORLDS_AO world paths, the _ADDON
// NIGHTMARES Single Player row and new game, add-on menu strings, and with the installed
// expansion the inspection of every WORLDS_AO world, the bumper and the CREDITS_AO roll.
#include "app/session_playback.h"
#include "engine/string_resources.h"
#include "game/game_settings.h"
#include "game/menu_resources.h"
#include "game/original/Credits.h"
#include "game/original/LoadingScreen.h"
#include "game/original/MenuIds.h"
#include <iostream>
#include <map>
#include <set>
using namespace OpenBlood2;
#define CHECK(x)                                                                                                       \
    do                                                                                                                 \
    {                                                                                                                  \
        if (!(x))                                                                                                      \
        {                                                                                                              \
            std::cerr << __LINE__ << ": " #x << "\n";                                                                  \
            return false;                                                                                              \
        }                                                                                                              \
    } while (false)
namespace
{
    bool pick(Menu& menu, int command)
    {
        for (std::size_t i = 0; i < menu.items.size(); ++i)
            if (menu.items[i].command == command)
            {
                menu.selected = int(i);
                menu.activate();
                return true;
            }
        return false;
    }
    // Original _ADDON LoadWorld/GetWorldTitle: a name containing WORLDS_AO is used as given.
    bool paths()
    {
        std::string path;
        CHECK(addon_world_name("worlds_ao\\caleb1_cc_c") && addon_world_name("x/Worlds_Ao/y") &&
              !addon_world_name("worlds/01_subwaya") && !addon_world_name("worlds_a0"));
        CHECK(world_resource_path("worlds_ao\\Caleb1_CC_C", true, path) && path == "worlds_ao/caleb1_cc_c.dat");
        CHECK(world_resource_path("WORLDS_AO\\END.DAT", true, path) && path == "worlds_ao/end.dat");
        CHECK(world_resource_path("Worlds_ao\\Multi_ao\\BB_Techno", true, path) &&
              path == "worlds_ao/multi_ao/bb_techno.dat");
        // The retail build always prefixes Worlds\ (the add-on path then does not exist).
        CHECK(world_resource_path("worlds_ao\\caleb1_cc_c", false, path) && path == "worlds/worlds_ao/caleb1_cc_c.dat");
        for (const bool addon : {false, true})
        {
            CHECK(world_resource_path("01_SubwayA", addon, path) && path == "worlds/01_subwaya.dat");
            CHECK(world_resource_path("Worlds\\02_CityHub.dat", addon, path) && path == "worlds/02_cityhub.dat");
            CHECK(!world_resource_path("worlds_ao\\..\\..\\x", addon, path) && path.empty());
        }
        WorldMessage message;
        WorldRequest request;
        std::string error;
        CHECK(original_world_request("worlds_ao\\ophelia_cc_o", 0, LOADTYPE_NEW_LEVEL, 2, message));
        CHECK(parse_world_request(message, request, error, true) && request.filename == "worlds_ao/ophelia_cc_o.dat" &&
              request.load_type == LOADTYPE_NEW_LEVEL && request.difficulty == 2);
        CHECK(parse_world_request(message, request, error) && request.filename == "worlds/worlds_ao/ophelia_cc_o.dat");
        // The normalized path survives the NEW_LEVEL re-encoding into a fresh-game request.
        CHECK(original_world_request("worlds_ao/ophelia_cc_o.dat", 0, LOADTYPE_NEW_GAME, 2, message));
        CHECK(decode_world_request(message, request, error, true) && request.filename == "worlds_ao/ophelia_cc_o.dat");
        return true;
    }
    // MenuSinglePlayer/MenuDifficulty _ADDON: NIGHTMARES first, the difficulty page, then
    // MenuNewNightmaresGame (Caleb, worlds_ao\caleb1_cc_c, a SINGLE game); story and Chosen clear it.
    bool menu()
    {
        Menu retail;
        retail.open(false);
        CHECK(pick(retail, MENU_CMD_SINGLE_PLAYER) && retail.page == MenuPage::Solo && !retail.addon());
        CHECK(retail.items.size() == 5 && retail.items[0].command == MENU_CMD_START_STORY);
        Menu menu(MenuEdition::Nightmares);
        CHECK(menu.addon() && !menu.demo());
        menu.open(false);
        CHECK(pick(menu, MENU_CMD_SINGLE_PLAYER) && menu.items.size() == 6);
        CHECK(menu.items[0].command == MENU_CMD_START_NIGHTMARES &&
              menu.items[0].string_id == IDS_MENU_SINGLEPLAYER_NIGHTMARES && menu.enabled(menu.items[0]) &&
              menu.items[1].command == MENU_CMD_START_STORY);
        CHECK(pick(menu, MENU_CMD_START_NIGHTMARES) && menu.page == MenuPage::Difficulty && menu.selected == 1);
        menu.back();
        CHECK(menu.page == MenuPage::Solo); // Its parent is Single Player, as for story mode.
        CHECK(pick(menu, MENU_CMD_START_NIGHTMARES) && pick(menu, MENU_CMD_DIFFICULTY_HARD));
        auto command = menu.take_command();
        CHECK(command.action == MenuAction::NewGame && command.nightmares && !command.story && command.character == 0 &&
              command.difficulty == DIFFICULTY_HARD);
        GameSession game;
        CHECK(game.initialize());
        auto start = menu_world_start(game, command, "01_SubwayA");
        CHECK(std::string_view(start.world) == "worlds_ao\\caleb1_cc_c" && start.character == 0 &&
              start.difficulty == DIFFICULTY_HARD && start.game_type == GAMETYPE_SINGLE);
        // Story mode (SetStoryMode) and the Chosen character page clear the Nightmares mode.
        menu.open(false);
        CHECK(pick(menu, MENU_CMD_SINGLE_PLAYER) && pick(menu, MENU_CMD_START_NIGHTMARES));
        menu.back();
        CHECK(pick(menu, MENU_CMD_START_STORY) && pick(menu, MENU_CMD_DIFFICULTY_EASY));
        command = menu.take_command();
        CHECK(command.story && !command.nightmares &&
              std::string_view(menu_world_start(game, command, "01_SubwayA").world) == "01_SubwayA");
        menu.open(false);
        CHECK(pick(menu, MENU_CMD_SINGLE_PLAYER) && pick(menu, MENU_CMD_START_NIGHTMARES));
        menu.back();
        CHECK(pick(menu, MENU_CMD_START_ACTION) && pick(menu, MENU_CMD_START_OPHELIA) &&
              pick(menu, MENU_CMD_DIFFICULTY_MEDIUM));
        command = menu.take_command();
        CHECK(!command.story && !command.nightmares && command.character == 1);
        menu.back();
        CHECK(menu.page == MenuPage::Main || menu.page == MenuPage::Character || !menu.active);
        return true;
    }
    // The add-on menus need IDS_MENU_SINGLEPLAYER_NIGHTMARES; the add-on background replaces the retail one.
    bool strings()
    {
        StringResources text;
        std::string error;
        // Every retail menu string (a superset of the demo's), then the add-on row.
        for (unsigned value : {500u, 501u, 502u, 504u, 505u, 509u, 510u, 511u, 512u, 513u, 514u, 515u, 516u, 517u,
                               518u, 521u, 522u, 523u, 524u, 525u, 526u, 527u, 528u, 529u, 530u, 531u, 532u, 533u,
                               534u, 535u, 536u, 537u, 538u, 539u, 540u, 541u, 547u, 548u, 561u, 600u})
            text.strings[value] = "X";
        for (unsigned id = 705; id <= 749; ++id)
            text.strings[id] = "X";
        CHECK(validate_menu_strings(MenuEdition::Retail, text, error));
        CHECK(!validate_menu_strings(MenuEdition::Nightmares, text, error) && error.find("549") != error.npos);
        text.strings[IDS_MENU_SINGLEPLAYER_NIGHTMARES] = "NIGHTMARES";
        CHECK(validate_menu_strings(MenuEdition::Nightmares, text, error));
        const auto& nightmares = menu_resources(MenuEdition::Nightmares);
        const auto& retail = menu_resources(MenuEdition::Retail);
        CHECK(nightmares.edition == MenuEdition::Nightmares &&
              std::string_view(nightmares.background) == "interface_ao/background.pcx" &&
              std::string_view(retail.background) == "interface/mainmenus/background.pcx" &&
              nightmares.titles == retail.titles && nightmares.ambience == retail.ambience);
        Resources resources;
        CHECK(menu_edition(resources) == MenuEdition::Retail);
        resources.set_nightmares(true);
        CHECK(menu_edition(resources) == MenuEdition::Nightmares);
        return true;
    }
    std::unique_ptr<GameSession> mount(const char* root, bool nightmares)
    {
        auto game = std::make_unique<GameSession>();
        Config config;
        config.data_dir = root;
        config.nightmares = nightmares;
        std::string error;
        if (!game->initialize() || !game->resources.mount_game(config, error))
        {
            std::cerr << error << "\n";
            return nullptr;
        }
        return game;
    }
    // The installed expansion: every WORLDS_AO world inspects with exactly the unported add-on
    // classes and the known not-created entries; the bumper, the menu strings, CREDITS_AO and
    // the END credits roll.
    bool installed(const char* root)
    {
        auto game = mount(root, true);
        CHECK(game);
        std::string error;
        const std::map<std::string, std::set<std::string>> unported{
            {"caleb1_cc_c", {}},
            {"ophelia_cc_o", {"FlayerPU", "GremlinAI"}},
            {"ishmael_cc_i", {"FlayerPU"}},
            {"gabby_cc_g", {"CombatShotgunPU", "GremlinAI"}},
            {"caleb2_cc_c", {}},
            {"endboss_cc_c", {"Nightmare"}},
            {"end", {}},
            {"multi_ao\\bb_techno", {}},
            {"multi_ao\\bb_sewers", {}},
            {"multi_ao\\bb_dungeon", {}},
            {"multi_ao\\bb_grayskull", {}},
            {"multi_ao\\soc_kickthehead1", {"SoccerBall", "SoccerGoal"}},
            {"multi_ao\\soc_kickthehead2", {"SoccerBall", "SoccerGoal"}}};
        // Known not-created entries, each explicit (robe Cultists are lot N8).
        const auto known_failure = [](std::string_view detail)
        {
            return detail.find("Add-on robe Cultist") != detail.npos ||
                   detail.find("Non-finite authored Pos") != detail.npos ||
                   detail.find("Chrome environment mapping") != detail.npos ||
                   detail.find("scorch1.dtx") != detail.npos;
        };
        for (const auto& [name, expected] : unported)
        {
            const auto world = "worlds_ao\\" + name;
            CHECK(game->request_world(world, error));
            CHECK(game->world_path == "worlds_ao/" +
                                          [&]
                                          {
                                              auto p = name;
                                              for (auto& c : p)
                                                  c = c == '\\' ? '/' : c;
                                              return p;
                                          }() +
                                          ".dat");
            std::set<std::string> found;
            std::map<std::string, unsigned> failed;
            CHECK(game->entries.size() == game->world.objects.size());
            for (std::size_t i = 0; i < game->entries.size(); ++i)
            {
                const auto& entry = game->entries[i];
                const auto& cls = game->world.objects[i].class_name;
                if (entry.state == EntryState::Unsupported)
                    found.insert(cls);
                else if (entry.state == EntryState::Failed)
                {
                    CHECK(known_failure(entry.detail));
                    ++failed[cls];
                }
            }
            CHECK(found == expected);
            std::cout << world << ": " << game->entries.size() << " entries";
            for (const auto& [cls, count] : failed)
                std::cout << ", " << count << " " << cls << " not created";
            std::cout << "\n";
        }
        // _ADDON StartNewWorld: the add-on bumper for a nightmare level, whatever the DAT names.
        LoadingScreen screen(game->resources);
        CHECK(screen.begin("worlds_ao\\caleb1_cc_c", false, error) &&
              screen.info().load_screen == "Screens_ao\\Bumper.pcx" && screen.info().title == "Cold, Cold Grave");
        CHECK(screen.begin("01_SubwayA", false, error) && screen.info().load_screen != "Screens_ao\\Bumper.pcx");
        CHECK(game->resources.find("screens_ao/bumper.pcx") && game->resources.find("screens_ao/title.pcx") &&
              game->resources.find("interface_ao/background.pcx"));
        StringResources strings;
        CHECK(load_menu_strings(game->resources, strings, error) &&
              strings.strings.at(IDS_MENU_SINGLEPLAYER_NIGHTMARES) == "NIGHTMARES");
        // Credits.cpp _ADDON: TEXT/CREDITS_AO, a different roll from the retail CREDITS.
        CHECK(game->client_shell.InitCredits(false, error));
        const int nightmares_credits = game->client_shell.m_Credits.GetNumCredits();
        // One session per process: parse the retail TEXT/CREDITS of the same cres.dll directly.
        std::vector<std::uint8_t> dll, text;
        CHECK(game->resources.read("cres.dll", dll, error) && read_named_resource(dll, "TEXT", "CREDITS", text, error));
        CCredits retail;
        CHECK(retail.Init(text, CM_CREDITS, false, 640, nullptr));
        const int retail_credits = retail.GetNumCredits();
        CHECK(nightmares_credits > 0 && retail_credits > 0 && nightmares_credits != retail_credits);
        std::cout << "Credits: CREDITS_AO " << nightmares_credits << " entries, CREDITS " << retail_credits << "\n";
        // The END world plays and rolls the credits over itself.
        CHECK(game->request_world("worlds_ao\\end", error));
        if (!game->run_world(error))
        {
            std::cerr << "END: " << error << "\n";
            return false;
        }
        CHECK(game->client_shell.m_bInWorld && game->client_shell.m_bRollCredits);
        // Saving in a nightmare level is refused until the add-on classes save (lot N9).
        GameSave save;
        CHECK(!game->save_game(save, error) && error.find("Nightmares levels") != error.npos);
        // The menu's new game reaches CALEB1_CC_C, whose startup still refuses its 19 robe Cultists
        // (lot N8) and the NaN-positioned ObjectivesTrigger.
        MenuCommand command;
        command.action = MenuAction::NewGame;
        command.nightmares = true;
        command.story = false;
        command.difficulty = DIFFICULTY_MEDIUM;
        const bool started = start_menu_world(game, menu_world_start(*game, command, "01_SubwayA"), error);
        std::cout << "Nightmares new game: " << (started ? "started" : error) << "\n";
        CHECK(!started && error.find("20 failed entries") != error.npos);
        return true;
    }
} // namespace
int main(int argc, char** argv)
{
    if (argc < 2)
        return 2;
    const std::string_view name = argv[1];
    if (name == "paths")
        return paths() ? 0 : 1;
    if (name == "menu")
        return menu() ? 0 : 1;
    if (name == "strings")
        return strings() ? 0 : 1;
    if (name == "installed" && argc == 3)
        return installed(argv[2]) ? 0 : 1;
    return 2;
}
