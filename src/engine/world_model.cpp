#include "engine/world_model.h"
#include "engine/shape_collision.h"
namespace OpenBlood2
{
    DVector object_bounds_center(const ObjectRecord& object)
    {
        return shape_bounds_center(object.state.m_Pos,
                                   (object.world_model ? object.world_model : object.container()).get());
    }
    CollisionQuery sweep_object(const ObjectRecord& object, DVector from, DVector to, DVector half, CollisionHit& hit,
                                double allowed_penetration, StartOverlap start)
    {
        return sweep_shape((object.world_model ? object.world_model : object.container()).get(),
                           object.state.m_Flags & FLAG_BOXPHYSICS, object.state.m_Pos, object.dimensions, from, to,
                           half, hit, allowed_penetration, start);
    }
} // namespace OpenBlood2
