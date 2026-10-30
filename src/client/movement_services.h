#pragma once
#include "client/environment.h"
#include "game/player_messages.h"

namespace OpenBlood2
{
    // CMoveMgr owns the original movement arithmetic. Implementations provide
    // either the immediate solo bridge or client-only replica prediction.
    class MovementServices
    {
      public:
        virtual ~MovementServices() = default;
        virtual ClientMotionState& body() = 0;
        virtual void clear() = 0;
        virtual bool valid() const = 0;
        virtual bool synchronize() = 0;
        virtual bool force_position(const PlayerPosition&) = 0;
        virtual bool controls(const PlayerControls&, bool send, bool& crouched) = 0;
        virtual bool environment(ClientContainerState&, int& character) = 0;
        virtual bool finish(float seconds, ClientContainerState&) = 0;
        virtual bool jump_voice(int character, bool external_camera) = 0;
        // The destination and velocity are already integrated. No implementation
        // may integrate elapsed time again when accepting this prediction.
        virtual bool advance(const PlayerMove&) = 0;
    };
} // namespace OpenBlood2
