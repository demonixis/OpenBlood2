#include "game/server_adapter.h"
#include <algorithm>
#include <cmath>
#include <limits>
namespace OpenBlood2
{
    SegmentResult ServerAdapter::CastRay(DVector from, DVector direction, SegmentHit& out, HOBJECT ignore)
    {
        trace_service("CastRay");
        out = {};
        DVector to;
        const auto bound = bound_collision_ray(from, direction, to);
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
        const auto bound = bound_collision_ray(from, direction, to);
        if (bound == SegmentResult::Invalid)
            return bound;
        if (!collision || !collision_ready)
            return SegmentResult::Unavailable;
        if (bound != SegmentResult::Hit)
            return bound;
        return IntersectWorldSegment(from, to, out);
    }
    SegmentResult ServerAdapter::IntersectWorldSegment(DVector from, DVector to, SegmentHit& out)
    {
        trace_service("IntersectWorldSegment");
        out = {};
        const auto delta = vector_subtract(to, from);
        if (!physics_vector(from) || !physics_vector(to) || vector_dot(delta, delta) < 1e-12f)
            return SegmentResult::Invalid;
        if (!collision || !collision_ready)
            return SegmentResult::Unavailable;
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
