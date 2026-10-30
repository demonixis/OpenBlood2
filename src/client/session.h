#pragma once
#include "client/world.h"

namespace OpenBlood2
{
    // First separation step: local replica admission/lifetime, independent of GameSession
    // and the original server globals. Movement, FX and the original shell migrate later.
    class ClientSession
    {
      public:
        ClientSession();
        void reset_local_world();
        bool receive(ClientWorldFrame&& frame, std::string& error)
        {
            return world.apply(std::move(frame), error);
        }
        ClientWorld world;
    };
} // namespace OpenBlood2
