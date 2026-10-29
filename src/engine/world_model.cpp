#include "engine/world_model.h"
#include <algorithm>

namespace OpenBlood2
{
    DVector object_bounds_center(const ObjectRecord& object)
    {
        if (!object.world_model && !object.container())
            return object.state.m_Pos;
        const auto& shape = *(object.world_model ? object.world_model : object.container());
        return vector_add(vector_scale(vector_add(shape.min, shape.max), .5f),
                          vector_subtract(object.state.m_Pos, shape.origin));
    }
    CollisionQuery sweep_object(const ObjectRecord& object, DVector from, DVector to, DVector half, CollisionHit& hit,
                                double allowed_penetration, StartOverlap start)
    {
        if ((!object.world_model && !object.container()) || (object.state.m_Flags & FLAG_BOXPHYSICS))
            return sweep_box(from, to, half, object_bounds_center(object), object.dimensions, hit, allowed_penetration);
        const auto& shape = *(object.world_model ? object.world_model : object.container());
        const auto offset = vector_subtract(object.state.m_Pos, shape.origin);
        auto local_from = vector_subtract(from, offset), local_to = vector_subtract(to, offset);
        double t0 = 0, t1 = 1;
        if (!physics_vector(local_from) || !physics_vector(local_to))
        {
            // A valid segment reaching the coordinate-domain edge (CastRay clips infinite
            // rays there) leaves the domain in a displaced shape's frame. Only its part
            // inside the shape's bounds, padded by the swept half, can hit: sweep that
            // part and remap the fraction instead of refusing the whole query.
            if (!physics_vector(from) || !physics_vector(to) || !physics_vector(half))
                return CollisionQuery::Invalid;
            const double o[3]{double(from.x) - offset.x, double(from.y) - offset.y, double(from.z) - offset.z};
            const double d[3]{double(to.x) - from.x, double(to.y) - from.y, double(to.z) - from.z};
            const double low[3]{double(shape.min.x) - half.x - 1, double(shape.min.y) - half.y - 1,
                                double(shape.min.z) - half.z - 1};
            const double high[3]{double(shape.max.x) + half.x + 1, double(shape.max.y) + half.y + 1,
                                 double(shape.max.z) + half.z + 1};
            for (unsigned i = 0; i < 3; ++i)
            {
                if (d[i] == 0)
                {
                    if (o[i] < low[i] || o[i] > high[i])
                        return CollisionQuery::Miss;
                    continue;
                }
                double a = (low[i] - o[i]) / d[i], b = (high[i] - o[i]) / d[i];
                if (a > b)
                    std::swap(a, b);
                t0 = std::max(t0, a);
                t1 = std::min(t1, b);
            }
            if (!(t0 <= t1))
                return CollisionQuery::Miss;
            local_from = {float(o[0] + d[0] * t0), float(o[1] + d[1] * t0), float(o[2] + d[2] * t0)};
            local_to = {float(o[0] + d[0] * t1), float(o[1] + d[1] * t1), float(o[2] + d[2] * t1)};
            if (!physics_vector(local_from) || !physics_vector(local_to))
                return CollisionQuery::Invalid;
            if (t1 - t0 < 1e-9)
                return CollisionQuery::Miss; // Grazes the padded bounds only.
        }
        const auto result = shape.sweep(local_from, local_to, half, hit, allowed_penetration, start);
        if (result == CollisionQuery::Hit)
        {
            hit.point = vector_add(hit.point, offset);
            hit.fraction = t0 + hit.fraction * (t1 - t0);
        }
        return result;
    }
} // namespace OpenBlood2
