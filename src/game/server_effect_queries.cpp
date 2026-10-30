#include "game/server_adapter.h"
namespace OpenBlood2
{
    SegmentResult ServerAdapter::effect_segment(DVector from, DVector to, ClientSegmentHit& out, SegmentObjects which)
    {
        SegmentHit hit;
        const auto result = IntersectSegment(from, to, nullptr, hit, false, which);
        const auto* object = objects_.get(hit.object);
        out = {hit.point, hit.normal, object ? object->id : ObjectId{}, hit.polygon, hit.fraction, hit.texture_flags};
        return result;
    }
    SegmentResult ServerAdapter::effect_world_segment(DVector from, DVector to, ClientSegmentHit& out)
    {
        SegmentHit hit;
        const auto result = IntersectWorldSegment(from, to, hit);
        out = {hit.point, hit.normal, {}, hit.polygon, hit.fraction, hit.texture_flags};
        return result;
    }
    SegmentResult ServerAdapter::effect_world_ray(DVector from, DVector direction, ClientSegmentHit& out)
    {
        SegmentHit hit;
        const auto result = CastWorldRay(from, direction, hit);
        out = {hit.point, hit.normal, {}, hit.polygon, hit.fraction, hit.texture_flags};
        return result;
    }
    bool ServerAdapter::effect_container(DVector point, ContainerCode& code, bool visible_only)
    {
        return GetPointContainerCode(point, code, visible_only);
    }
} // namespace OpenBlood2
