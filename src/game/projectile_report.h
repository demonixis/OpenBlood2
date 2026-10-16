#pragma once
#include "game/weapon_messages.h"
#include <string>
namespace OpenBlood2
{
    struct ProjectileReport
    {
        std::string source, outcome, error;
        DVector position{};
        double time = 0;
        unsigned delivered = 0;
    };
} // namespace OpenBlood2
