#pragma once
#include "basedefs_de.h"

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
    // Observable volume values; no original object, callback, or damage state.
    struct ContainerEnvironment
    {
        ContainerCode code = CC_NOTHING;
        DVector current{};
        float gravity = -200, viscosity = 0;
        bool hidden = false;
    };
} // namespace OpenBlood2
