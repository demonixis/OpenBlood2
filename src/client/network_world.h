#pragma once
#include "engine/model_store.h"
#include "engine/world.h"
#include "net/world_snapshot.h"

namespace OpenBlood2
{
    // Decode resources from this client's mounted data, never construct ObjectDLL
    // objects or borrow the publishing server's asset caches. Commits atomically.
    bool receive_network_world(const Net::WorldSnapshot&, const World&, ModelStore&, SpriteStore&, ClientWorld&,
                               std::string& error);
} // namespace OpenBlood2
