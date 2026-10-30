#include "client/effect_queries.h"
#include <algorithm>
#include <cmath>
#include <limits>
namespace OpenBlood2
{
    SegmentResult bound_collision_ray(DVector from, DVector direction, DVector& to)
    {
        if (!physics_vector(from) || !physics_vector(direction))
            return SegmentResult::Invalid;
        const double length = std::sqrt(double(direction.x) * direction.x + double(direction.y) * direction.y +
                                        double(direction.z) * direction.z);
        if (length < 1e-8)
            return SegmentResult::Invalid;
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
        return SegmentResult::Hit;
    }
    SegmentResult ClientEffectQueries::effect_segment(DVector from, DVector to, ClientSegmentHit& hit,
                                                      SegmentObjects objects)
    {
        hit = {};
        if (!physics_vector(from) || !physics_vector(to) ||
            vector_dot(vector_subtract(to, from), vector_subtract(to, from)) < 1e-12f)
            return SegmentResult::Invalid;
        if (!scene_)
            return SegmentResult::Unavailable;
        return scene_->intersect(from, to, {scene_->frame().world, {}}, hit, objects);
    }
    SegmentResult ClientEffectQueries::effect_world_segment(DVector from, DVector to, ClientSegmentHit& out)
    {
        out = {};
        const auto delta = vector_subtract(to, from);
        if (!physics_vector(from) || !physics_vector(to) || vector_dot(delta, delta) < 1e-12f)
            return SegmentResult::Invalid;
        if (!scene_ || !scene_->frame().primary)
            return SegmentResult::Unavailable;
        CollisionHit hit;
        const auto result = scene_->frame().primary->sweep(from, to, {}, hit);
        if (result == CollisionQuery::Invalid)
            return SegmentResult::Invalid;
        if (result == CollisionQuery::Miss)
            return SegmentResult::Miss;
        // Preserve the original primary-only service, including its zero texture flags.
        out = {hit.point, hit.normal, {}, hit.polygon, hit.fraction};
        return SegmentResult::Hit;
    }
    SegmentResult ClientEffectQueries::effect_world_ray(DVector from, DVector direction, ClientSegmentHit& out)
    {
        out = {};
        DVector to;
        const auto bound = bound_collision_ray(from, direction, to);
        if (bound == SegmentResult::Invalid)
            return bound;
        if (!scene_ || !scene_->frame().primary)
            return SegmentResult::Unavailable;
        return bound == SegmentResult::Hit ? effect_world_segment(from, to, out) : bound;
    }
    bool ClientEffectQueries::effect_container(DVector point, ContainerCode& code, bool visible_only)
    {
        if (!physics_vector(point) || !scene_ || !scene_->frame().sequence)
            return false;
        code = CC_NOTHING;
        for (const auto& volume : scene_->frame().objects)
            if (volume.environment && volume.shape && volume.shape->contains(point))
            {
                if (!visible_only || (volume.user_flags & FLAG_VISIBLE))
                    code = volume.environment->code;
                return true; // Hidden/zero-code first overlap still masks later volumes.
            }
        return true;
    }
} // namespace OpenBlood2
