#pragma once
#include "engine/container.h"
namespace OpenBlood2
{
    DVector shape_bounds_center(DVector position, const ContainerShape*);
    CollisionQuery sweep_shape(const ContainerShape*, bool box, DVector position, DVector dimensions, DVector from,
                               DVector to, DVector half, CollisionHit&, double allowed_penetration = 0,
                               StartOverlap start = StartOverlap::Block);
} // namespace OpenBlood2
