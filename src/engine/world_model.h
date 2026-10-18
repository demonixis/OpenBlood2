#pragma once
#include "engine/container.h"
#include "engine/objects.h"

namespace OpenBlood2
{
    DVector object_bounds_center(const ObjectRecord&);
    CollisionQuery sweep_object(const ObjectRecord&, DVector from, DVector to, DVector half, CollisionHit&,
                                StartOverlap start = StartOverlap::Block);
} // namespace OpenBlood2
