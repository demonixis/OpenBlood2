#pragma once
#include "client/world.h"

namespace OpenBlood2
{
    class GameSession;
    class Objects;
    struct ObjectRecord;
    ClientCollisionValues capture_collision_values(const ObjectRecord&, const Objects&);
    // Caller supplies world/sequence and recipient-specific player/camera values.
    // Capture advances no simulation, callback, sound, animation or random clock.
    bool capture_client_objects(GameSession&, ClientWorldFrame&, std::string& error, bool collision = false);
} // namespace OpenBlood2
