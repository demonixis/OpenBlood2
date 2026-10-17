#pragma once
#include "basedefs_de.h"
namespace OpenBlood2
{
    // One local flashlight for the single-player diagnostic renderer. No handles.
    struct PointLight
    {
        DVector position{}, color{};
        float radius = 0;
    };
} // namespace OpenBlood2
