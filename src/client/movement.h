#pragma once
#include "client/world.h"
#include "engine/movement_state.h"

namespace OpenBlood2
{
    struct ClientMotionState : MovementValues
    {
        ObjectId ground_object;
    };
    // A value body owned by the client, never an ObjectDLL instance. The original
    // CMoveMgr integrates it; the immediate local bridge supplies collision queries
    // and authoritative corrections. No scheduling or callbacks live here.
    struct ClientMovement
    {
        ClientObjectId owner;
        ClientMotionState body;
        std::uint64_t predictions = 0, prediction_failures = 0, acknowledgements = 0, rejected = 0;
    };
} // namespace OpenBlood2
