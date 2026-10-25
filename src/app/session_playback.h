#pragma once
#include "game/game_settings.h"
#include "game/menu.h"
#include "game/server_adapter.h"
#include <algorithm>
#include <cmath>

namespace OpenBlood2
{
    // Ownership changes release command levels and edge latches without ticking the world.
    inline void neutralize_app_input(CBloodClientShell& shell)
    {
        shell.movement_input = {};
        shell.weapon_input = {};
        shell.grab_down = shell.taunt_down = shell.drop_weapon_requested = false;
        shell.use_down = shell.use_was_down = false;
        shell.medkit_down = shell.medkit_was_down = false;
        shell.flashlight_down = shell.flashlight_was_down = false;
        shell.proximity_down = shell.proximity_was_down = false;
        shell.remote_down = shell.remote_was_down = false;
        shell.detonate_down = shell.detonate_was_down = false;
        shell.move_mgr.m_bLastJumpCommand = false;
    }

    // Keep a slow interactive frame from amplifying its own simulation catch-up.
    // Excess wall time is reported by main and discarded, never queued for later.
    // Diagnostic stepping retains explicit requested time; invalid deltas still fail.
    inline double app_frame_delta(double wall_seconds, bool diagnostic = false)
    {
        return diagnostic || !std::isfinite(wall_seconds) || wall_seconds < 0 ? wall_seconds
                                                                              : std::min(wall_seconds, .1);
    }

    // Interactive inspection must not run the original single-player world
    // before its player exists. Explicit diagnostic simulations remain opt-in.
    inline bool waiting_for_player(const GameSession& game, bool diagnostic_simulation = false)
    {
        return !diagnostic_simulation && !game.world_path.empty() && !game.client_shell.m_bInWorld;
    }
    inline bool step_app_session(GameSession& game, double seconds, bool diagnostic_simulation = false)
    {
        if (waiting_for_player(game, diagnostic_simulation))
            return game.objects.finish_frame(); // Explicit editor removals still drain; no clock/update advance.
        return game.step(seconds);
    }

    struct MenuWorldStart
    {
        const char* world = nullptr;
        DBYTE difficulty = DIFFICULTY_MEDIUM, character = 0, game_type = GAMETYPE_SINGLE;
    };
    // Original MenuNewGame/MenuCredits world requests for a NewGame/Credits menu command.
    // MenuCredits (outside a world) is StartNewGame("Credits", GAMETYPE_CUSTOM, DIFFICULTY_MEDIUM)
    // with the current configured character: it never calls SetCharacterInfo, so the last
    // entered profile (Caleb before any game) is kept.
    inline MenuWorldStart menu_world_start(const GameSession& game, const MenuCommand& command, const char* first_level)
    {
        if (command.action == MenuAction::Credits)
            return {"Credits", DIFFICULTY_MEDIUM, game.client_shell.entry_profile.character, GAMETYPE_CUSTOM};
        // The replacement Cheats warp: the owner fills the character and difficulty.
        if (command.action == MenuAction::Warp)
            return {command.world.c_str(), DBYTE(command.difficulty), DBYTE(command.character), GAMETYPE_SINGLE};
        return {first_level, DBYTE(command.difficulty), DBYTE(command.story ? 0 : command.character), GAMETYPE_SINGLE};
    }
    inline bool start_menu_world(std::unique_ptr<GameSession>& game, const MenuWorldStart& start, std::string& error)
    {
        return GameSession::start_new_game(game, start.world ? start.world : "", start.difficulty, start.character,
                                           error, false, start.game_type);
    }
} // namespace OpenBlood2
