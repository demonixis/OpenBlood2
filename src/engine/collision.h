#pragma once
#include "engine/world.h"
#include <array>

namespace OpenBlood2
{
    struct CollisionHit
    {
        double fraction = 1;
        DVector normal{}, point{};
        std::uint32_t polygon = UINT32_MAX;
        HOBJECT object = nullptr;
        bool start_solid = false;
    };
    enum class CollisionQuery
    {
        Invalid,
        Miss,
        Hit
    };
    // Continuous axis-aligned box/triangle queries against the primary DAT model.
    // Dynamic world-model transforms, stair stepping and BSP contents are not
    // inferred here. The caller can inspect precisely which geometry is indexed.
    class CollisionWorld
    {
      public:
        bool build(const WorldModel& model, std::string& error);
        bool transform(DRotation, DVector origin, DVector destination);
        void clear()
        {
            triangles_.clear();
            nodes_.clear();
        }
        CollisionQuery sweep(DVector from, DVector to, DVector half, CollisionHit& hit) const;
        std::size_t triangles() const
        {
            return triangles_.size();
        }
        std::size_t nodes() const
        {
            return nodes_.size();
        }

      private:
        struct Triangle
        {
            std::array<DVector, 3> points;
            DVector min, max;
            std::uint32_t polygon;
        };
        struct Node
        {
            DVector min, max;
            std::uint32_t begin = 0, end = 0, left = 0, right = 0;
        };
        std::vector<Triangle> triangles_;
        std::vector<Node> nodes_;
        std::uint32_t partition(std::uint32_t begin, std::uint32_t end);
        void query(std::uint32_t node, DVector from, DVector to, DVector half, CollisionHit& hit, bool& found) const;
    };
    DVector rotate_vector(DVector, DRotation);
    DRotation multiply_rotation(DRotation, DRotation);
    DRotation inverse_rotation(DRotation);
    bool physics_vector(DVector v);
    CollisionQuery sweep_box(DVector from, DVector to, DVector half, DVector center, DVector other_half,
                             CollisionHit& hit);
    DVector vector_add(DVector a, DVector b);
    DVector vector_subtract(DVector a, DVector b);
    DVector vector_scale(DVector a, float scale);
    float vector_dot(DVector a, DVector b);
} // namespace OpenBlood2
