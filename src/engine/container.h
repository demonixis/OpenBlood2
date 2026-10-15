#pragma once
#include "engine/collision.h"

namespace OpenBlood2
{
    // Closed, consistently oriented DAT polygon shell. AABBs only reject broad-phase misses.
    class ContainerShape
    {
      public:
        bool build(const WorldModel&, DRotation, std::string& error);
        bool contains(DVector point) const;
        bool overlaps(DVector center, DVector half) const;
        DVector min{}, max{}, origin{};
        bool convex = false;
        std::vector<std::array<DVector, 3>> triangles;

      private:
        std::vector<Plane> planes_;
        CollisionWorld boundary_;
    };
} // namespace OpenBlood2
