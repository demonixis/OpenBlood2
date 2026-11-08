#pragma once
#include "game/game_settings.h"
#include "game/menu.h"
#include "game/server_adapter.h"
#include <string>
#include <vector>

// Application half of the replacement Options > CHEATS page (cheat_contracts.md): the campaign
// level list, the original CCheatMgr message lines, and god mode kept on the entered player while
// the persisted switch asks for it. Consumed at the owning frame boundary; headless.
namespace OpenBlood2
{
    // Every mounted world outside worlds/multi/, in index (file-name) order: the 36 single-player
    // worlds of the retail data, Demo_01/Demo_02 in the demo.
    inline std::vector<std::string> cheat_levels(const Resources& resources)
    {
        std::vector<std::string> levels;
        for (const auto& [path, resource] : resources.index())
        {
            (void)resource;
            if (path.starts_with("worlds/") && path.ends_with(".dat") && !path.starts_with("worlds/multi/") &&
                path.find('/', 7) == std::string::npos)
                levels.push_back(path.substr(7, path.size() - 11));
        }
        return levels;
    }
    // Original CCheatMgr::Process message lines (MessageMgr.cpp:886-1000); ALL KEYS is replacement.
    inline std::string cheat_message(int code, bool state, bool applied, unsigned keys = 0,
                                     std::string_view* translation_key = nullptr)
    {
        const auto result = [&](std::string_view key, std::string english)
        {
            if (translation_key)
                *translation_key = key;
            return english;
        };
        if (!applied)
            return result("menu.cheat_unavailable", "Cheats are unavailable in this game.");
        switch (code)
        {
            case CHEAT_GOD:
                return state ? result("menu.cheat_god_on", "You are a GOD.")
                             : result("menu.cheat_god_off", "God mode off. Help!");
            case CHEAT_KFA:
                return result("menu.cheat_weapons", "Monolith Rulez...");
            case CHEAT_AMMO:
                return result("menu.cheat_ammo", "You have full ammo.");
            case CHEAT_HEALTH:
                return result("menu.cheat_health", "You have full health.");
            case CHEAT_KILLALLAI:
                return result("menu.cheat_kill", "...Let Tchernobog sort'em out.");
            case CHEAT_GIVEALLINV:
                return result("menu.cheat_items", "You just bought our entire inventory!");
            case CHEAT_ALLKEYS:
                if (!keys)
                    return result("menu.cheat_no_keys", "No other key in this level.");
                return keys == 1 ? result("menu.cheat_one_key", "1 key added.")
                                 : result("menu.cheat_keys", std::to_string(keys) + " keys added.");
            default:
                return result({}, {});
        }
    }
    struct CheatFlow
    {
        const GameSession* session = nullptr; // With the revision: a new game replaces the session.
        std::uint64_t world_revision = UINT64_MAX;
        HOBJECT refused_player = nullptr; // A refused god-mode request is not repeated every frame.
        bool refused_state = false;
        std::uint64_t sent = 0;
        // A Cheat menu command for the entered player; the result line goes to the Cheats page.
        bool command(GameSession& game, Menu& menu, const MenuCommand& request)
        {
            const bool applied = game.client_shell.SendCheatMessage(game.server_shell, DBYTE(request.cheat), true);
            const auto* player = game.server_shell.Player(game.local_client);
            menu.cheat_status_count = player ? player->cheat_keys_added : 0;
            menu.cheat_status = cheat_message(request.cheat, true, applied, menu.cheat_status_count,
                                              &menu.cheat_status_key);
            ++menu.revision;
            ++sent;
            return applied;
        }
        // Every frame: the LEVEL row follows the current world, and the entered living player's
        // god mode follows the CHEATS and GOD MODE settings (original CHEAT_GOD toggle).
        void update(GameSession& game, Menu& menu)
        {
            if (&game != session || game.world_revision != world_revision)
            {
                session = &game;
                world_revision = game.world_revision;
                refused_player = nullptr;
                for (std::size_t i = 0; i < menu.levels.size(); ++i)
                    if (game.world_path == "worlds/" + menu.levels[i] + ".dat")
                    {
                        menu.level = int(i);
                        ++menu.revision;
                    }
            }
            if (!game.client_shell.m_bInWorld)
                return;
            auto* player = game.server_shell.Player(game.local_client);
            if (!player || player->m_bDead || player->m_damage.IsDead())
                return;
            const bool god = menu.settings.cheats && menu.settings.god_mode;
            if (player->m_damage.m_bGodMode == god || (refused_player == player->m_hObject && refused_state == god))
                return;
            const bool applied = game.client_shell.SendCheatMessage(game.server_shell, CHEAT_GOD, god);
            refused_player = applied ? nullptr : player->m_hObject;
            refused_state = god;
            menu.cheat_status = cheat_message(CHEAT_GOD, god, applied, 0, &menu.cheat_status_key);
            ++menu.revision;
            ++sent;
        }
    };
} // namespace OpenBlood2
