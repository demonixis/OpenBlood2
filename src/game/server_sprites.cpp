#include "game/server_adapter.h"
#include <algorithm>
#include <cmath>
#include <limits>
namespace OpenBlood2
{
    namespace
    {
        SegmentResult ray_end(DVector from, DVector direction, DVector& to)
        {
            if (!physics_vector(from) || !physics_vector(direction))
                return SegmentResult::Invalid;
            const double length = std::sqrt(double(direction.x) * direction.x + double(direction.y) * direction.y +
                                            double(direction.z) * direction.z);
            if (length < 1e-8)
                return SegmentResult::Invalid;
            // Clip an infinite ray to the same finite domain accepted by collision.
            double distance = std::numeric_limits<double>::infinity();
            const double d[3]{direction.x / length, direction.y / length, direction.z / length};
            const double f[3]{from.x, from.y, from.z};
            for (unsigned i = 0; i < 3; ++i)
                if (d[i] != 0)
                    distance = std::min(distance, ((d[i] > 0 ? 1e7 : -1e7) - f[i]) / d[i]);
            if (!(distance > 0))
                return SegmentResult::Miss;
            to = DVector{float(std::clamp(f[0] + distance * d[0], -1e7, 1e7)),
                         float(std::clamp(f[1] + distance * d[1], -1e7, 1e7)),
                         float(std::clamp(f[2] + distance * d[2], -1e7, 1e7))};
            return SegmentResult::Hit; // A valid bounded segment, not an intersection result.
        }
    } // namespace
    SegmentResult ServerAdapter::CastRay(DVector from, DVector direction, SegmentHit& out, HOBJECT ignore)
    {
        trace_service("CastRay (solid world and objects)");
        out = {};
        DVector to;
        const auto bound = ray_end(from, direction, to);
        if (bound == SegmentResult::Invalid)
            return bound;
        if (!collision || !collision_ready)
            return SegmentResult::Unavailable;
        if (bound != SegmentResult::Hit)
            return bound;
        return IntersectSegment(from, to, ignore, out);
    }
    SegmentResult ServerAdapter::CastWorldRay(DVector from, DVector direction, SegmentHit& out)
    {
        trace_service("CastWorldRay");
        out = {};
        DVector to;
        const auto bound = ray_end(from, direction, to);
        if (bound == SegmentResult::Invalid)
            return bound;
        if (!collision || !collision_ready)
            return SegmentResult::Unavailable;
        if (bound != SegmentResult::Hit)
            return bound;
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
