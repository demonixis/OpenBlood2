#include "client/collision.h"
#include "engine/shape_collision.h"
namespace OpenBlood2
{
    SegmentResult ClientCollisionWorld::intersect(DVector from, DVector to, ClientObjectId ignore,
                                                  ClientSegmentHit& out, SegmentObjects which, bool ignore_volumes,
                                                  SegmentStart start) const
    {
        out = {};
        if (ignore.world != frame_.world || !physics_vector(from) || !physics_vector(to) ||
            (ignore.object && !get(ignore.object)) ||
            vector_dot(vector_subtract(to, from), vector_subtract(to, from)) < 1e-12f ||
            (which != SegmentObjects::SolidOnly && which != SegmentObjects::SolidOrRayHit &&
             which != SegmentObjects::All) ||
            (start != SegmentStart::IncludeInterior && start != SegmentStart::SkipInterior))
            return SegmentResult::Invalid;
        if (!frame_.primary)
            return SegmentResult::Unavailable;
        CollisionHit nearest;
        const auto world_result = frame_.primary->sweep(from, to, {}, nearest);
        if (world_result == CollisionQuery::Invalid)
            return SegmentResult::Invalid;
        bool found = world_result == CollisionQuery::Hit;
        ObjectId nearest_object;
        for (const auto& object : frame_.objects)
        {
            if (object.id == ignore.object || (ignore_volumes && object.volume_brush) ||
                (which != SegmentObjects::All &&
                 !(object.flags & (which == SegmentObjects::SolidOrRayHit ? FLAG_SOLID | FLAG_RAYHIT : FLAG_SOLID))))
                continue;
            // Ray queries include attached bodies, unlike movement sweeps. Their pose
            // was refreshed by the publisher; no callback or attachment service runs here.
            CollisionHit hit;
            const auto result = sweep_shape(object.shape.get(), object.flags & FLAG_BOXPHYSICS, object.position,
                                            object.dimensions, from, to, {}, hit);
            if (result == CollisionQuery::Invalid)
                return SegmentResult::Invalid;
            if ((which == SegmentObjects::All || start == SegmentStart::SkipInterior) && hit.start_solid)
                continue;
            if (result == CollisionQuery::Hit && (!found || hit.fraction < nearest.fraction))
            {
                nearest = hit;
                nearest_object = object.id;
                found = true;
            }
        }
        if (!found)
            return SegmentResult::Miss;
        out = {nearest.point, nearest.normal, nearest_object, nearest.polygon, nearest.fraction, nearest.texture_flags};
        return SegmentResult::Hit;
    }
} // namespace OpenBlood2
