#pragma once
#include "engine/gi_world.h"
#include "engine/world_view.h"
#include <array>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

// The geometry a GI bake traces (gi_contracts.md, lot G5): the faces WorldMesh renders, at the
// authored pose, with CPU albedo, the drawn baked light and a SAH BVH. Read-only once built, so
// any number of threads may trace it at once; each trace keeps its own stack.
namespace OpenBlood2
{
    namespace gi_bake_limits
    {
        constexpr std::size_t max_triangles = 4u * 1024 * 1024;
        constexpr std::size_t max_layers = 8;   // Translucent layers a ray passes before it stops.
        constexpr std::uint32_t leaf_size = 4;  // Triangles per BVH leaf (more only at max_depth).
        constexpr std::uint32_t sah_depth = 40; // Deeper nodes split at the median (always halves).
        constexpr std::uint32_t max_depth = 64; // Bounds the per-trace stack.
        constexpr std::uint32_t sah_bins = 12;
        constexpr double albedo_grid = 20; // World units per albedo texel the mip choice aims for.
    } // namespace gi_bake_limits

    // What a world range is to the bake.
    enum class GiRangeUse : std::uint8_t
    {
        Skip,
        Main, // Traced by the bake rays.
        Sky   // Kept apart for the sky pass (decision D9, lot G7).
    };
    struct GiBakeFace
    {
        enum Flag : std::uint8_t
        {
            Translucent = 1, // SURF_TRANSPARENT with a resolved material: alpha decides per texel.
            SkyPortal = 2,   // Sky face of a world model: rays end there with Sky.
            Lightmapped = 4, // Light from the face's lightmap page.
            Unresolved = 8   // No material (missing texture): opaque, albedo 0.5.
        };
        std::uint32_t range = 0, material = 0; // WorldMesh range and material (0 when none).
        std::uint32_t page = UINT32_MAX;       // Index into GiBakeScene::pages(), or none.
        std::uint8_t flags = 0, mip = 0;       // Albedo mip level.
        DVector normal{};                      // Unit plane normal.
        std::array<std::array<float, 2>, 3> uv{}, lightmap{};
        std::array<DVector, 3> colors{};
    };
    struct GiLayer
    {
        float t = 0, u = 0, v = 0;
        std::uint32_t face = 0;
        float alpha = 0; // Texel alpha, 0 < alpha < 1.
    };
    struct GiHit
    {
        enum class Kind : std::uint8_t
        {
            Miss,
            Surface,   // An opaque face (or an unresolved one).
            Sky,       // A sky-portal face.
            LayerLimit // The ninth translucent layer: the ray stops there.
        };
        Kind kind = Kind::Miss;
        bool back = false; // dot(direction, face normal) > 0.
        float t = 0, u = 0, v = 0;
        std::uint32_t face = UINT32_MAX;
        std::uint32_t layer_count = 0;
        std::array<GiLayer, gi_bake_limits::max_layers> layers{}; // Nearest first, before the hit.
    };
    struct GiTraceStats
    {
        std::uint64_t rays = 0, nodes = 0, triangles = 0, texels = 0;
    };

    // One triangle set with its SAH BVH. Triangles are world-space points (local + pivot).
    class GiBvh
    {
      public:
        struct Node
        {
            float min[3], max[3];
            std::uint32_t index; // Inner: first child (the second follows). Leaf: first triangle.
            std::uint16_t count; // Triangles in a leaf; zero for an inner node.
            std::uint16_t axis;  // Split axis of an inner node.
        };
        static_assert(sizeof(Node) == 32);
        struct Triangle
        {
            float v0[3], e1[3], e2[3];
            std::uint32_t face; // GiBakeScene face index; also the closest-hit tie break.
        };
        // Builds from triangles listed by face; deterministic, single-threaded.
        bool build(std::vector<Triangle> triangles, std::string& error);
        std::span<const Node> nodes() const
        {
            return nodes_;
        }
        std::span<const Triangle> triangles() const
        {
            return triangles_;
        }
        std::uint32_t depth() const
        {
            return depth_;
        }

      private:
        friend class GiBakeScene;
        std::vector<Node> nodes_;
        std::vector<Triangle> triangles_;
        std::uint32_t depth_ = 0;
    };

    struct GiSceneReport
    {
        std::size_t ranges = 0, main_ranges = 0, sky_ranges = 0, skipped_ranges = 0, excluded_ranges = 0;
        std::size_t main_faces = 0, sky_faces = 0, translucent = 0, sky_portals = 0, unresolved = 0;
        std::size_t lightmapped = 0, nodes = 0, sky_nodes = 0;
        std::uint32_t depth = 0;
        double build_ms = 0;
    };

    class GiBakeScene
    {
      public:
        // From a built presentation mesh (with resources, lightmaps on) and one use per range;
        // takes the mesh. Positions are local + pivot (identity rotation, the authored pose).
        bool build(WorldMesh mesh, std::span<const GiRangeUse> use, GiSceneReport&, std::string& error);
        // Closest hit along origin + t * direction, t in [0, tmax] (t in units of `direction`),
        // two-sided, with translucent layers before it. Thread-safe.
        void trace(DVector origin, DVector direction, float tmax, GiHit&, GiTraceStats&) const;
        void trace_sky(DVector origin, DVector direction, float tmax, GiHit&, GiTraceStats&) const;
        // The face's drawn baked light at barycentric (u, v): the bilinear lightmap page times
        // world_saturate_scale, else the interpolated vertex colors (WorldShade's rule).
        DVector baked_light(std::uint32_t face, float u, float v) const;
        // Raw albedo (0-1, as stored) at the face's mip, nearest texel, repeating; 0.5 grey for an
        // unresolved face. Alpha is the mip-0 texel alpha the traversal used (1 for opaque faces).
        std::array<float, 4> albedo(std::uint32_t face, float u, float v) const;
        std::span<const GiBakeFace> faces() const
        {
            return faces_;
        }
        const GiBvh& main() const
        {
            return main_;
        }
        const GiBvh& sky() const
        {
            return sky_;
        }
        const WorldMesh& mesh() const
        {
            return mesh_;
        }
        std::span<const Texture> pages() const
        {
            return pages_;
        }

      private:
        void trace_tree(const GiBvh&, DVector origin, DVector direction, float tmax, GiHit&, GiTraceStats&) const;
        // Mip-0 alpha byte at (u, v), and the image of a face's material.
        std::uint8_t alpha_byte(const GiBakeFace&, float u, float v) const;
        const Texture* face_image(const GiBakeFace&) const;
        WorldMesh mesh_;
        std::vector<GiBakeFace> faces_;
        std::vector<Texture> pages_;
        GiBvh main_, sky_;
    };
    // One use per world model (WorldMesh range): Primary, Mover and Destructible are Main, Sky is
    // Sky; containers and unbound models are skipped (decision D8: the authored pose). `excluded`
    // names non-primary models (case-insensitive) left out; a name matching none fails.
    bool gi_range_uses(const World&, std::span<const std::string> excluded, std::vector<GiRangeUse>&,
                       GiSceneReport&, std::string& error);
    // Albedo mip for a triangle: round(log2(grid * texels per world unit)), clamped to the image's
    // levels. `uv` are normalized coordinates of an image of the given size.
    std::uint8_t gi_albedo_mip(const std::array<DVector, 3>& points, const std::array<std::array<float, 2>, 3>& uv,
                               std::uint32_t width, std::uint32_t height, std::uint32_t levels);
} // namespace OpenBlood2
