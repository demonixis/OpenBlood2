#pragma once
#include "basedefs_de.h"
#include <array>

// Original Shared/ContainerCodes.h values, including FreeFall's liquid classification.
enum ContainerCode : D_WORD
{
    CC_NOTHING,
    CC_WATER,
    CC_BLOOD,
    CC_ACID,
    CC_WIND,
    CC_LADDER,
    CC_CONVEYOR,
    CC_CRANECONTROL,
    CC_DAMAGE,
    CC_FREEFALL,
    CC_MINEFIELD
};
inline bool IsLiquid(ContainerCode code)
{
    return code == CC_WATER || code == CC_BLOOD || code == CC_ACID || code == CC_FREEFALL;
}
inline bool IsFreeMovement(ContainerCode code)
{
    return IsLiquid(code) || code == CC_LADDER;
}
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
