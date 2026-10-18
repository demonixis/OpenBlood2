#pragma once
#include "engine/world_view.h"

namespace OpenBlood2
{
    struct WorldShadeStats
    {
        std::size_t triangles = 0, nodes = 0;
        std::uint64_t traces = 0, hits = 0, node_tests = 0, triangle_tests = 0;
    };
    // Baked per-vertex light of the textured opaque faces of every world range, for reading
    // the light around a point. Built from the presentation mesh, so its faces, colors and
    // local positions are exactly the drawn ones; traces use the current range transforms.
    // Translucent, sky-portal and unresolved-material faces are excluded.
    class WorldShade
    {
      public:
        bool build(const WorldMesh&, std::string& error);
        void clear();
        // Interpolated face light at the nearest face (either side) crossed by the segment
        // from `from` to `to` among the given range draws. False when nothing is crossed.
        bool trace(DVector from, DVector to, std::span<const WorldDraw>, DVector& light, float& fraction);
        WorldShadeStats stats;
        static constexpr std::size_t triangle_limit = 1000000;

      private:
        struct Triangle
        {
            std::array<DVector, 3> points, colors;
            DVector min, max;
        };
        struct Node
        {
            DVector min, max;
            std::uint32_t begin = 0, end = 0, left = 0, right = 0;
        };
        struct Tree
        {
            std::vector<Triangle> triangles;
            std::vector<Node> nodes;
        };
        std::uint32_t partition(Tree&, std::uint32_t begin, std::uint32_t end);
        std::vector<Tree> ranges_; // Indexed by WorldMesh range.
        std::vector<std::uint32_t> stack_;
    };
} // namespace OpenBlood2
