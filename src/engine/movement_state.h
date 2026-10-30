#pragma once
#include "game/legacy/basedefs_de.h"

namespace OpenBlood2
{
    // Collision/motion values shared by local prediction and authoritative queries.
    // Identity is supplied separately by each side; this is not a wire layout.
    struct MovementValues
    {
        DVector position{}, velocity{}, acceleration{}, dimensions{}, ground_normal{};
        bool standing = false;
        std::uint32_t ground_surface = 0;
        std::uint64_t stair_attempts = 0, stair_steps = 0, overlap_contacts = 0, start_departures = 0;
        float last_step_height = 0, last_overlap_depth = 0;
        std::uint32_t ground_polygon = UINT32_MAX;
    };
} // namespace OpenBlood2
