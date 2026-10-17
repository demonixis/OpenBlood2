#pragma once
#include "game/server_adapter.h"
#include <algorithm>
#include <cmath>

namespace OpenBlood2
{
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
} // namespace OpenBlood2
