#pragma once
#include "game/server_adapter.h"
namespace test_flashlight_replica
{
    inline bool publish(OpenBlood2::GameSession& game)
    {
        std::string error;
        return game.publish_client_objects(error);
    }
    inline bool update(OpenBlood2::GameSession& game, HOBJECT source, HOBJECT owner)
    {
        if (!publish(game))
            return false;
        const bool geometry = game.local_movement.publish_collision();
        return game.client_shell.flashlight.Update(
            game.client_session.world, geometry ? &game.client_session.collision : nullptr,
            game.server.effect_reference(source), game.server.effect_reference(owner));
    }
    inline OpenBlood2::PointLight current(OpenBlood2::GameSession& game)
    {
        if (!publish(game))
            return {};
        return game.client_shell.flashlight.Current(game.client_session.world);
    }
} // namespace test_flashlight_replica
