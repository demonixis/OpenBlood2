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
        bool shockwave = false, cancelled = false;
        DVector query_from{}, query_to{}, blast_position{}, blast_normal{};
        SegmentResult query = SegmentResult::Unavailable;
        unsigned candidates = 0, in_radius = 0;
        bool flare = false, tracked = false;
        unsigned burn_ticks = 0, burst_children = 0;
    };
} // namespace OpenBlood2
