#pragma once
#include "engine/container_types.h"
#include <array>

namespace OpenBlood2
{
    struct ContainerInfo
    {
        HOBJECT object = nullptr;
        ContainerCode code = CC_NOTHING;
        DVector current{};
        float gravity = -200, viscosity = 0;
        bool hidden = false;
    };
    struct ObjectRecord;
    // Shared authoritative extraction for server queries and replica publication.
    bool container_info(const ObjectRecord&, ContainerInfo&);
    struct ContainerState
    {
        std::array<ContainerInfo, 4> body{}; // Original MAX_TRACKED_CONTAINERS.
        std::size_t count = 0, total = 0;
        ContainerInfo head{};
    };
    // Stack-owned payload, synchronous original MID_AFFECTPHYSICS dispatch only.
    struct ContainerPhysics
    {
        HOBJECT object = nullptr;
        DVector velocity{}, acceleration{};
        float seconds = 0;
        bool gravity = false, player = false, damage_only = false;
    };
} // namespace OpenBlood2
