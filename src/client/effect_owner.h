#pragma once
#include "client/world.h"
namespace OpenBlood2
{
    // Creation-time values copied at the publisher boundary. Updates resolve the
    // world-qualified identity only in ClientWorld; no server handle is retained.
    struct EffectOwner
    {
        ClientObjectId id;
        NodePose pose;
        std::uint32_t user_flags = 0;
        std::string name;
        explicit operator bool() const
        {
            return id.world && bool(id.object) && id.object.generation;
        }
    };
} // namespace OpenBlood2
