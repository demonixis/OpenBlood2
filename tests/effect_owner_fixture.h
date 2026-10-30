#pragma once
#include "game/server_adapter.h"
#include <iostream>
namespace test_effect_owner
{
    // Existing effect tests explicitly publish a replica when they bypass GameSession::step.
    template <class Pool> bool advance(OpenBlood2::GameSession& game, Pool& pool, double now)
    {
        std::string error;
        if (!game.publish_client_objects(error))
        {
            std::cerr << error << '\n';
            return false;
        }
        return pool.advance(now, game.client_session.world);
    }
} // namespace test_effect_owner
