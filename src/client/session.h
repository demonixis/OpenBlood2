#pragma once
#include "client/announcer.h"
#include "client/audio.h"
#include "client/camera.h"
#include "client/effect_runtime.h"
#include "client/effects.h"
#include "client/hud.h"
#include "client/movement.h"
#include "client/world.h"

namespace OpenBlood2
{
    // First separation step: local replica admission/lifetime, independent of GameSession
    // and the original server globals. The optional cosmetic runtime is client-owned;
    // remaining legacy services and the original shell migrate separately.
    class ClientSession
    {
      public:
        explicit ClientSession(std::unique_ptr<EffectRuntime> runtime = {});
        EffectRuntime* effect_runtime() const
        {
            return effect_runtime_.get();
        }
        void reset_local_world();
        bool receive(ClientWorldFrame&& frame, std::string& error)
        {
            return world.apply(std::move(frame), error);
        }
        float time = 0; // Immediate local clock publication, separate from world-relative time.
        ClientAudio audio;
        ClientAnnouncer announcer;
        ClientEffects effects;
        ClientCamera camera;
        ClientHud hud;
        ClientWorld world;
        ClientMovement movement;
        ClientCollisionWorld collision;

      private:
        std::unique_ptr<EffectRuntime> effect_runtime_;
    };
} // namespace OpenBlood2
