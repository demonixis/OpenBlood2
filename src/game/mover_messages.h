#pragma once
#include "basedefs_de.h"
namespace OpenBlood2
{
    struct WorldBodyProperties
    {
        float mass = 100, friction = 7;
        bool pushable = false;
    };
    struct RotationMove
    {
        DVector from_angles{}, to_angles{}, original_position{}, pivot{};
    };
    enum class MoverResult
    {
        Moved,
        Blocked,
        Invalid,
        Unavailable
    };
} // namespace OpenBlood2
