#pragma once
#include "basedefs_de.h"
#include <cstdint>

namespace OpenBlood2
{
    struct PlacementAnchor
    {
        DVector center{}, half{};
    };
    enum class BodyPlacementResult
    {
        Unqueried,
        Placed,
        Adjusted,
        Blocked,
        Invalid,
        Unavailable
    };
    struct BodyPlacement
    {
        BodyPlacementResult result = BodyPlacementResult::Unqueried;
        DVector anchor{}, requested{}, candidate{};
        unsigned corrections = 0;
        double distance = 0;
        std::uint32_t polygon = UINT32_MAX;
    };
    inline bool placement_succeeded(const BodyPlacement& p)
    {
        return p.result == BodyPlacementResult::Placed || p.result == BodyPlacementResult::Adjusted;
    }
    const char* body_placement_name(BodyPlacementResult);
} // namespace OpenBlood2
