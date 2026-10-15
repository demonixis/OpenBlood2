#pragma once
#include "game/server_adapter.h"
#include <iostream>
namespace test_camera
{
    inline bool complete_subway_intro(OpenBlood2::GameSession& game)
    {
        // Actual start-point/trigger entry now reaches camera1. Do not force OFF
        // or remove its permission gate to make pre-camera gameplay tests pass.
        const float began = game.server.GetTime();
        const auto* camera = game.objects.get(game.client_shell.view.object);
        if (!camera || std::string_view(camera->state.m_Name) != "camera1" ||
            game.client_shell.view.settings.player_movement)
            return false;
        bool second_seen = false;
        for (int i = 0; i < 1800 && game.client_shell.view.object; ++i)
        {
            const auto* selected = game.objects.get(game.client_shell.view.object);
            second_seen |= selected && std::string_view(selected->state.m_Name) == "camera2";
            if (!game.step(1.0 / 60))
                return false;
        }
        const float elapsed = game.server.GetTime() - began;
        const auto* player = game.server_shell.Player(game.local_client);
        std::cout << "Original SubwayA camera intro expired after " << elapsed << " simulation seconds\n";
        return second_seen && elapsed >= 12 && elapsed < 12.1f && !game.client_shell.view.object && player &&
               !player->camera_movement_blocked;
    }
} // namespace test_camera
