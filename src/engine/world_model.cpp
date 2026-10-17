#include "engine/world_model.h"

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
    CollisionQuery sweep_object(const ObjectRecord& object, DVector from, DVector to, DVector half, CollisionHit& hit)
    {
        if ((!object.world_model && !object.container()) || (object.state.m_Flags & FLAG_BOXPHYSICS))
            return sweep_box(from, to, half, object_bounds_center(object), object.dimensions, hit);
        const auto& shape = *(object.world_model ? object.world_model : object.container());
        const auto offset = vector_subtract(object.state.m_Pos, shape.origin);
        const auto result = shape.sweep(vector_subtract(from, offset), vector_subtract(to, offset), half, hit);
        if (result == CollisionQuery::Hit)
            hit.point = vector_add(hit.point, offset);
        return result;
    }
} // namespace OpenBlood2
