#pragma once
#include "basedefs_de.h"

namespace OpenBlood2
{
    struct LineEffectGeometry
    {
        DVector from{}, to{}, start_color{}, end_color{};
        float start_alpha = 1, end_alpha = 1;
    };

    bool valid_line_effect(const LineEffectGeometry&);

    struct DetachedEffectLine
    {
        std::uint64_t id = 0;
        double expires = 0;
        LineEffectGeometry line;
    };
}
