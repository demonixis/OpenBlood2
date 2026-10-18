#pragma once
#include "engine/container.h"
#include "engine/objects.h"

namespace OpenBlood2
{
    DVector object_bounds_center(const ObjectRecord&);
    CollisionQuery sweep_object(const ObjectRecord&, DVector from, DVector to, DVector half, CollisionHit&,
                                double allowed_penetration = 0);
} // namespace OpenBlood2
