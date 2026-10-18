#pragma once
#include "engine/collision.h"

namespace OpenBlood2
{
    // Closed, consistently oriented DAT polygon components, possibly touching at edges.
    // AABBs only reject broad-phase misses.
    class ContainerShape
    {
      public:
        bool build(const WorldModel&, DRotation, std::string& error);
        bool transform(DRotation, DVector destination);
        bool contains(DVector point) const;
        bool overlaps(DVector center, DVector half) const;
        CollisionQuery sweep(DVector from, DVector to, DVector half, CollisionHit&, double allowed_penetration = 0,
                             StartOverlap start = StartOverlap::Block) const;
        DVector min{}, max{}, origin{};
        bool convex = false;
        std::size_t closed_components = 0, shared_edges = 0;
        std::vector<std::array<DVector, 3>> triangles;

      private:
        std::vector<Plane> planes_;
        CollisionWorld boundary_;
    };
} // namespace OpenBlood2
