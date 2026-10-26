// Single Player > CHOSEN: the original CMenuDifficulty::StartGame starts GAMETYPE_ACTION, whose
// TypeActionMode=false objects (cameras, cinematic and conversation triggers) are refused by
// B2BaseClass::InitialUpdate, while the server shell (m_GameInfo, no NetGame) stays single player.
#include "app/session_playback.h"
#include "game/game_settings.h"
#include "game/original/CameraObj.h"
#include "game/original/MenuIds.h"
#include "game/original/PlayerObj.h"
#include "game/original/Trigger.h"
#include "game/server_adapter.h"
#include <iostream>
#include <map>
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
    float game_type(GameSession& g)
    {
        return g.server.GetVarValueFloat(g.server.GetGameConVar("GAME_GAMETYPE"));
    }
    // The original menu commands, the world requests they produce and the two game-type views.
    bool requests()
    {
        GameSession game;
        CHECK(game.initialize());
        constexpr int characters[] = {MENU_CMD_START_CALEB, MENU_CMD_START_OPHELIA, MENU_CMD_START_ISHMAEL,
                                      MENU_CMD_START_GABRIELLA};
        for (unsigned c = 0; c < 4; ++c)
        {
            Menu menu;
            menu.open(false);
            CHECK(pick(menu, MENU_CMD_SINGLE_PLAYER) && pick(menu, MENU_CMD_START_ACTION) &&
                  menu.page == MenuPage::Character);
            CHECK(pick(menu, characters[c]) && pick(menu, MENU_CMD_DIFFICULTY_HARD));
            const auto command = menu.take_command();
            CHECK(command.action == MenuAction::NewGame && !command.story && command.character == c &&
                  command.difficulty == DIFFICULTY_HARD);
            const auto start = menu_world_start(game, command, "01_SubwayA");
            CHECK(std::string_view(start.world) == "01_SubwayA" && start.character == c &&
                  start.difficulty == DIFFICULTY_HARD && start.game_type == GAMETYPE_ACTION);
        }
        Menu menu;
        menu.open(false);
        CHECK(pick(menu, MENU_CMD_SINGLE_PLAYER) && pick(menu, MENU_CMD_START_STORY) &&
              pick(menu, MENU_CMD_DIFFICULTY_EASY));
        auto command = menu.take_command();
        CHECK(command.story && command.character == 0);
        auto start = menu_world_start(game, command, "01_SubwayA");
        CHECK(start.game_type == GAMETYPE_SINGLE && start.character == 0);
        // A warp outside a world is a story game.
        command = {};
        command.action = MenuAction::Warp;
        command.world = "02_CityHub";
        CHECK(menu_world_start(game, command, "01_SubwayA").game_type == GAMETYPE_SINGLE);
        // The world request syntax accepts the local types for NEW_GAME and NEW_LEVEL only.
        for (DBYTE type : {GAMETYPE_SINGLE, GAMETYPE_ACTION, GAMETYPE_CUSTOM})
            for (DBYTE load : {LOADTYPE_NEW_GAME, LOADTYPE_NEW_LEVEL})
            {
                WorldMessage message;
                WorldRequest request;
                std::string error;
                CHECK(original_world_request("01_SubwayA", type, load, 1, message) &&
                      parse_world_request(message, request, error) && request.game_type == type &&
                      request.filename == "worlds/01_subwaya.dat");
            }
        for (DBYTE type : {GAMETYPE_BLOODBATH, GAMETYPE_CTF, GAMETYPE_COOP})
        {
            WorldMessage message;
            WorldRequest request;
            std::string error;
            CHECK(original_world_request("01_SubwayA", type, LOADTYPE_NEW_GAME, 1, message) &&
                  !parse_world_request(message, request, error));
        }
        // GAME_GAMETYPE carries the type; the server shell's NetGame type stays single player for
        // the local types and follows a network type.
        for (unsigned type = 0; type <= GAMETYPE_CUSTOM; ++type)
        {
            game.server.set_game_settings(1, type);
            CHECK(game_type(game) == float(type));
            CHECK(local_game_type(type) == !game.server.IsMultiplayerGame());
            CHECK(game.server.net_game_type == (local_game_type(type) ? 0 : type));
        }
        return true;
    }
    struct Census
    {
        std::map<std::string, unsigned> created, filtered;
        unsigned alive_cut_scene = 0;
    };
    bool cut_scene_class(std::string_view name)
    {
        return name == "CameraObj" || name == "CinematicTrigger" || name == "ConversationTrigger";
    }
    Census census(GameSession& g)
    {
        Census c;
        for (const auto& entry : g.startup.entries)
            if (entry.state == EntryState::Filtered)
                ++c.filtered[entry.class_name];
            else if (entry.state == EntryState::Created)
                ++c.created[entry.class_name];
        for (auto id : g.objects.snapshot())
            if (const auto* r = g.objects.get(id); r && r->life == Life::Alive && cut_scene_class(r->type->name))
                ++c.alive_cut_scene;
        return c;
    }
    bool start(std::unique_ptr<GameSession>& game, unsigned character, bool story, std::string& error)
    {
        MenuCommand command;
        command.action = MenuAction::NewGame;
        command.character = character;
        command.difficulty = DIFFICULTY_MEDIUM;
        command.story = story;
        return start_menu_world(game, menu_world_start(*game, command, "01_SubwayA"), error);
    }
    std::unique_ptr<GameSession> mount(const char* root)
    {
        auto game = std::make_unique<GameSession>();
        Config config;
        config.data_dir = root;
        std::string error;
        if (!game->initialize() || !game->resources.mount_game(config, error))
            return nullptr;
        return game;
    }
    // Menu Chosen games reach 01_SubwayA without its intro cameras, for Caleb and another Chosen.
    bool subway(const char* root)
    {
        auto game = mount(root);
        CHECK(game);
        std::string error;
        CHECK(start(game, 0, true, error));
        const auto story = census(*game);
        CHECK(game_type(*game) == GAMETYPE_SINGLE && story.created.at("CameraObj") >= 2 && story.alive_cut_scene >= 2 &&
              !story.filtered.contains("CameraObj"));
        CHECK(game->step(1. / 60) && game->client_shell.view.object); // camera1 holds the story start.
        for (unsigned character : {0u, 1u})
        {
            CHECK(start(game, character, false, error));
            auto& g = *game;
            const auto chosen = census(g);
            CHECK(g.world_path == "worlds/01_subwaya.dat" && game_type(g) == GAMETYPE_ACTION &&
                  !g.server.IsMultiplayerGame());
            CHECK(g.startup.accepted && g.startup.player_entered && g.startup.failed == 0);
            CHECK(chosen.alive_cut_scene == 0 && !chosen.created.contains("CameraObj"));
            CHECK(chosen.filtered.at("CameraObj") == story.created.at("CameraObj"));
            for (const auto& [name, count] : chosen.filtered)
                std::cout << "Chosen SubwayA filtered " << name << ' ' << count << " (story "
                          << (story.filtered.contains(name) ? story.filtered.at(name) : 0) << ")\n";
            auto* player = g.server_shell.Player(g.local_client);
            CHECK(player && player->m_nCharacter == character && player->attributes_received);
            for (int i = 0; i < 120; ++i)
                CHECK(g.step(1. / 60));
            CHECK(!g.client_shell.view.object && !player->camera_movement_blocked && !g_hActiveCamera);
        }
        return true;
    }
    // The audited single-player branches in a Chosen game: action-only lock feedback, cheats,
    // saves and NEW_LEVEL keeping the type.
    bool continuation(const char* root)
    {
        auto game = mount(root);
        CHECK(game);
        std::string error;
        CHECK(start(game, 2, false, error) && game->step(1. / 60));
        auto* g = game.get();
        auto* player = g->server_shell.Player(g->local_client);
        CHECK(player);
        // Trigger.cpp:921 and :802: FormatString(IDS_GENERAL_LOCKED/UNLOCKED) on the player's
        // console replaces the lock sounds; the story game keeps the sounds.
        Properties properties{{"Name", std::string("chosen-lock")},
                              {"Locked", true},
                              {"LockedSound", std::string("sounds/doors/locked.wav")},
                              {"UnlockedSound", std::string("sounds/doors/unlocked.wav")},
                              {"Pos", DVector{0, 0, 0}}};
        const auto lock = g->objects.create(g->objects.find_class("Trigger"), {}, Creation::World, properties);
        CHECK(lock && g->step(1. / 60));
        auto* trigger = dynamic_cast<Trigger*>(g->server.HandleToObject(lock));
        CHECK(trigger && trigger->locked);
        auto send = [&](const char* command, HOBJECT sender)
        {
            auto text = g->server.CreateString(command);
            auto message = g->server.StartMessageToObject(g->server.HandleToObject(sender), lock, 1000);
            const bool sent = text && message && g->server.WriteToMessageHString(message, text) == DE_OK &&
                              g->server.EndMessage(message) == DE_OK;
            g->server.FreeString(text);
            return sent;
        };
        std::string locked_text, unlocked_text;
        CHECK(g->server.GetClientResourceString(268, locked_text) &&
              g->server.GetClientResourceString(269, unlocked_text));
        CHECK(send("TRIGGER", player->m_hObject) && trigger->locked && trigger->console_feedback == 1 &&
              !trigger->last_voice && trigger->sound_starts == 0 && trigger->sound_error.empty());
        CHECK(send("UNLOCK", player->m_hObject) && !trigger->locked && trigger->console_feedback == 2 &&
              !trigger->last_voice);
        CHECK(g->pump_player_messages(error) && g->client_shell.chat_lines.size() >= 2);
        const auto& lines = g->client_shell.chat_lines;
        CHECK(lines[lines.size() - 2] == locked_text && lines.back() == unlocked_text);
        std::cout << "Chosen lock feedback: \"" << locked_text << "\" / \"" << unlocked_text << "\"\n";
        CHECK(g->objects.remove(lock) && g->objects.finish_frame());
        // PlayerObj.cpp:3403 reads the server shell's NetGame type: cheats work in Chosen games.
        const auto applied = player->cheats_applied;
        CHECK(player->ProcessCheat(CHEAT_HEALTH, true) && player->cheats_applied == applied + 1);
        // A Chosen save keeps its type, and the reload still refuses the cut-scene objects.
        GameSave save;
        CHECK(g->save_game(save, error));
        CHECK(GameSession::load_game(game, save, error));
        g = game.get();
        CHECK(game_type(*g) == GAMETYPE_ACTION && !g->server.IsMultiplayerGame());
        CHECK(census(*g).alive_cut_scene == 0);
        player = g->server_shell.Player(g->local_client);
        CHECK(player && player->m_nCharacter == 2);
        // NEW_LEVEL: the client's request carries ACTION and the destination keeps it.
        CHECK(g->server.SendWorldExit(WorldExit{"02_CityHub"}, "") == DE_OK && g->pump_player_messages(error));
        WorldRequest request;
        CHECK(g->pending_level_change && parse_world_request(g->pending_level_change->message, request, error) &&
              request.game_type == GAMETYPE_ACTION && request.load_type == LOADTYPE_NEW_LEVEL);
        CHECK(GameSession::process_level_change(game, error) == LevelChangeResult::Committed);
        g = game.get();
        CHECK(g->world_path == "worlds/02_cityhub.dat" && game_type(*g) == GAMETYPE_ACTION &&
              !g->server.IsMultiplayerGame());
        CHECK(census(*g).alive_cut_scene == 0);
        player = g->server_shell.Player(g->local_client);
        CHECK(player && player->m_nCharacter == 2);
        for (int i = 0; i < 60; ++i)
            CHECK(g->step(1. / 60));
        CHECK(!g->client_shell.view.object);
        return true;
    }
} // namespace
int main(int argc, char** argv)
{
    if (argc < 2)
        return 2;
    const std::string_view name = argv[1];
    if (name == "requests")
        return requests() ? 0 : 1;
    if (argc != 3)
        return 2;
    if (name == "subway")
        return subway(argv[2]) ? 0 : 1;
    if (name == "continuation")
        return continuation(argv[2]) ? 0 : 1;
    return 2;
}
