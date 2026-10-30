#pragma once
#include "client/audio.h"
#include "client/camera.h"
#include "client/hud.h"
#include "client/movement.h"
#include "client/world.h"

namespace OpenBlood2
{
    // First separation step: local replica admission/lifetime, independent of GameSession
    // and the original server globals. FX and the original shell migrate later.
    class ClientSession
    {
      public:
        ClientSession();
        void reset_local_world();
        bool receive(ClientWorldFrame&& frame, std::string& error)
        {
            return world.apply(std::move(frame), error);
        }
        float time = 0; // Immediate local clock publication, separate from world-relative time.
        ClientAudio audio;
        ClientCamera camera;
        ClientHud hud;
        ClientWorld world;
        ClientMovement movement;
        ClientCollisionWorld collision;
    };
} // namespace OpenBlood2
