#pragma once
#include "game/server_adapter.h"

namespace OpenBlood2
{
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
} // namespace OpenBlood2
