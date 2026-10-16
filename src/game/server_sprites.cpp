#include "game/server_adapter.h"
#include <algorithm>
#include <cmath>
#include <limits>
namespace OpenBlood2
{
    SegmentResult ServerAdapter::CastWorldRay(DVector from, DVector direction, SegmentHit& out)
    {
        trace_service("CastWorldRay");
        out = {};
        if (!physics_vector(from) || !physics_vector(direction))
            return SegmentResult::Invalid;
        const double length = std::sqrt(double(direction.x) * direction.x + double(direction.y) * direction.y +
                                        double(direction.z) * direction.z);
        if (length < 1e-8)
            return SegmentResult::Invalid;
        if (!collision || !collision_ready)
            return SegmentResult::Unavailable;
        // Clip an infinite ray to the same finite domain accepted by collision.
        // The original query omits INTERSECT_OBJECTS, so dynamic objects are excluded.
        double distance = std::numeric_limits<double>::infinity();
        const double d[3]{direction.x / length, direction.y / length, direction.z / length};
        const double f[3]{from.x, from.y, from.z};
        for (unsigned i = 0; i < 3; ++i)
            if (d[i] != 0)
                distance = std::min(distance, ((d[i] > 0 ? 1e7 : -1e7) - f[i]) / d[i]);
        if (!(distance > 0))
            return SegmentResult::Miss;
        DVector to{float(std::clamp(f[0] + distance * d[0], -1e7, 1e7)),
                   float(std::clamp(f[1] + distance * d[1], -1e7, 1e7)),
                   float(std::clamp(f[2] + distance * d[2], -1e7, 1e7))};
        CollisionHit hit;
        const auto result = collision->sweep(from, to, {}, hit);
        if (result == CollisionQuery::Invalid)
            return SegmentResult::Invalid;
        if (result == CollisionQuery::Miss)
            return SegmentResult::Miss;
        out = {hit.point, hit.normal, nullptr, hit.polygon, hit.fraction};
        return SegmentResult::Hit;
    }
} // namespace OpenBlood2
