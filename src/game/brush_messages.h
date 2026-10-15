#pragma once
#include "basedefs_de.h"
#include <string>
namespace OpenBlood2
{
    // Retained diagnostic requests, not rendered debris or simulated fragments.
    struct BrushDestruction
    {
        std::string name;
        DVector position{}, direction{};
        float damage = 0;
        DDWORD debris_amount = 0;
    };
} // namespace OpenBlood2
