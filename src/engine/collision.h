#pragma once
#include "engine/body_placement.h"
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
        double penetration = 0;
        std::uint16_t texture_flags = 0;
        // Initially overlapped faces ignored under StartOverlap::Depart.
        std::uint32_t departed = 0;
    };
    // Block: any initial overlap is start_solid. Depart: an initially overlapped
    // face does not block a nonzero displacement that does not go deeper into it
    // (outward normal . delta >= 0; DAT winding agrees with the plane normal).
    // Zero-length queries and deeper moves still block.
    enum class StartOverlap
    {
        Block,
        Depart
    };
    enum class CollisionQuery
    {
        Invalid,
        Miss,
        Hit
    };
    struct CollisionWork
    {
        std::uint64_t node_tests = 0, node_rejections = 0;
        std::uint64_t triangle_bounds_tests = 0, triangle_rejections = 0, triangle_tests = 0;
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
            work_ = {};
        }
        CollisionQuery sweep(DVector from, DVector to, DVector half, CollisionHit& hit,
                             StartOverlap start = StartOverlap::Block) const;
        BodyPlacement place_body(DVector anchor, DVector anchor_half, DVector requested, DVector half) const;
        std::size_t triangles() const
        {
            return triangles_.size();
        }
        std::size_t nodes() const
        {
            return nodes_.size();
        }
        // Diagnostic work since build/clear, including body-placement queries.
        // Like the collision index's transforms, access is simulation-thread only.
        const CollisionWork& work() const
        {
            return work_;
        }

      private:
        struct Triangle
        {
            std::array<DVector, 3> points;
            DVector min, max;
            std::uint32_t polygon;
            std::uint16_t texture_flags = 0;
        };
        struct Node
        {
            DVector min, max;
            std::uint32_t begin = 0, end = 0, left = 0, right = 0;
        };
        std::vector<Triangle> triangles_;
        std::vector<Node> nodes_;
        mutable CollisionWork work_;
        std::uint32_t partition(std::uint32_t begin, std::uint32_t end);
        void query(std::uint32_t node, DVector from, DVector to, DVector half, CollisionHit& hit, bool& found,
                   const PlacementAnchor* anchor = nullptr, const Triangle** selected = nullptr,
                   StartOverlap start = StartOverlap::Block, std::uint32_t* departed = nullptr) const;
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
