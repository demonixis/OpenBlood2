#pragma once
#include "engine/gi_bake_scene.h"
#include "engine/gi_jobs.h"
#include "engine/gi_sky.h"
#include "engine/gi_world.h"
#include <array>
#include <atomic>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

// Indirect light on the world's own light carriers (gi_contracts.md, lot G7): one value per stored
// lightmap texel and per baked vertex colour, gathered with cosine rays over the bake scene in one
// pass per bounce, dilated over invalid samples and stored in the LMAP and VTX sections.
//
// Units (decision D1): E is the cosine-weighted mean of the incoming display radiance (raw albedo
// times drawn light, 0-1 per channel, unbounded). A page draws as 2 x Saturate, so a texel adds
// E * 255 / 2 to its 8-bit value; vertex light draws unscaled and a corner adds E * 255.
namespace OpenBlood2
{
    namespace gi_lightmap_limits
    {
        constexpr std::uint32_t format = 1; // LMAP and VTX payload format.
        constexpr std::size_t max_texels = 4u * 1024 * 1024, max_corners = 4u * 1024 * 1024;
        constexpr double sample_offset = .5;    // World units along the receiver normal.
        constexpr double edge_inset = 1;        // At most this far toward the centroid (half the distance).
        constexpr double back_limit = .1;       // A sample seeing more back faces than this is invalid.
        constexpr std::uint32_t fill_rounds = 8; // Receiver-to-receiver fill across shared edges.
        constexpr double fill_normal = .9;       // Minimum unit-normal dot between filled neighbours.
        constexpr std::uint32_t chunk = 32;      // Elements per job.
        // Lot GQ1 denoise: hit distances are clamped to this range (world units) for the guide.
        constexpr double guide_near = 1, guide_far = 4096;
    } // namespace gi_lightmap_limits

    // Shared-exponent RGB (N = 9 mantissa bits, bias 15), the GL_EXT_texture_shared_exponent rule:
    // negative and NaN channels encode 0, values above 65408 saturate. frexp/ldexp only (exact).
    std::uint32_t gi_rgb9e5(DVector);
    DVector gi_rgb9e5_decode(std::uint32_t);

    // Decision D1 for one 8-bit channel: A_eff = clamp(min(O, A), 0, O), D = O - A_eff and
    // out = clamp(round(D + max(k E s, f A_eff)), 0, 255), s = 255 / 2 for a texel, 255 for a corner.
    // A NaN or negative E counts as 0; E = 0 with f = 1 gives back O exactly.
    std::uint8_t gi_compose(std::uint8_t original, float ambient, float energy, float k, float f, bool texel);
    // A texel of a re-baked file (lot GD2): D' (0-1, as decoded) replaces D:
    // out = clamp(round(255 D' + max(k E 255 / 2, f A_eff)), 0, 255). A NaN or negative D' counts as 0;
    // D' = D gives gi_compose's value.
    std::uint8_t gi_compose_direct(float direct, std::uint8_t original, float ambient, float energy, float k, float f);
    // The default intensity and ambient-floor weight, chosen at Gate B (2026-11-07). They are composition
    // parameters, not bake settings: changing them needs no new bake.
    constexpr float gi_default_k = 2, gi_default_f = 1;

    struct GiLightmapBlock
    {
        std::uint16_t model = 0, polygon = 0;
        std::uint8_t width = 0, height = 0; // The polygon's stored lightmap size.
    };
    struct GiLightmaps
    {
        std::vector<GiLightmapBlock> blocks;  // Sorted by (model, polygon), unique.
        std::vector<std::uint32_t> texels;    // RGB9E5 E, block order, then row-major.
        std::vector<std::uint8_t> valid;      // 0/1 per texel: the sample's own validity.
    };
    struct GiCornerSet
    {
        std::uint16_t model = 0, polygon = 0, corners = 0; // corners: the polygon's base ring.
    };
    struct GiVertexLight
    {
        std::vector<GiCornerSet> sets;      // Sorted by (model, polygon), unique.
        std::vector<std::uint32_t> corners; // RGB9E5 E, set order, then corner order.
        std::vector<std::uint8_t> valid;
    };
    // Payloads: u32 format, u32 records, u32 values; 8-byte records (u16 model, u16 polygon, then
    // u8 width, u8 height or u16 corners, then a zero u16); u32 values; validity bits LSB-first with
    // zero padding. The readers are strict: order, counts, sizes, reserved fields and padding.
    bool write_gi_lightmaps(const GiLightmaps&, std::vector<std::uint8_t>&, std::string& error);
    bool read_gi_lightmaps(std::span<const std::uint8_t>, GiLightmaps&, std::string& error);
    bool write_gi_vertex_light(const GiVertexLight&, std::vector<std::uint8_t>&, std::string& error);
    bool read_gi_vertex_light(std::span<const std::uint8_t>, GiVertexLight&, std::string& error);
    // The sections fit this world: blocks name polygons with a stored lightmap of that size, sets name
    // polygons with baked colours and that base ring.
    bool check_gi_lightmaps(const World&, const GiLightmaps&, std::string& error);
    bool check_gi_vertex_light(const World&, const GiVertexLight&, std::string& error);

    // The receivers of a world in LMAP/VTX order. Elements are the texels of every block, then the
    // corners of every set; element indices are global over both.
    struct GiReceivers
    {
        struct Block
        {
            GiLightmapBlock id;
            std::uint32_t first = 0; // First element.
            LightmapFrame frame;
            DVector normal{};
        };
        struct Set
        {
            GiCornerSet id;
            std::uint32_t first = 0;
            bool low = false; // A lightmapped polygon: its vertex light shows only at DETAIL LOW.
            DVector normal{};
        };
        // How a scene face carries light, for radiance lookups.
        struct Link
        {
            enum Kind : std::uint8_t
            {
                Black,    // Untextured (or no receiver): 0.
                Lightmap, // Bilinear over its block, doubled.
                Vertex,   // Interpolated over its set's corners.
                Bright,   // Its albedo.
                Sky       // A sky portal: the sky.
            };
            Kind kind = Black;
            std::uint32_t receiver = 0;               // Block or set index.
            std::array<std::array<float, 2>, 3> st{}; // Block texel coordinates of the three vertices.
            std::array<std::uint16_t, 3> corners{};   // Set corners of the three vertices.
        };
        std::vector<Block> blocks;
        std::vector<Set> sets;
        std::size_t texels = 0, corners = 0, low_sets = 0, low_corners = 0;
        std::vector<Link> links;          // One per scene face.
        // Receivers (blocks, then sets) sharing a base-ring edge (point indices) with a receiver of
        // the same kind (block, vertex set or DETAIL LOW set) and a unit-normal dot of at least
        // fill_normal: neighbours[neighbour_first[r]] up to neighbour_first[r + 1].
        std::vector<std::uint32_t> neighbour_first, neighbours;
        std::vector<std::uint32_t> owner; // Per element: block index, or blocks.size() + set index.
        std::vector<DVector> samples;     // Per element: the ray origin.
        std::vector<std::uint8_t> solid;  // Per element: the sample lies in solid root-BSP space.
        std::vector<std::array<std::uint8_t, 3>> original; // Per element: stored texel or colour (0-255).
        DVector ambient{};                // AmbientLight (0-255); zero when the info has none.
        // Lot GQ1 texel supersampling: with texel_grid n > 1, every texel also has n x n points at the
        // centres of an n x n grid over the texel (texel t, point k = row * n + column at t * n^2 + k),
        // placed like `samples` (moved into the base ring, raised), and their root-BSP contents.
        // `samples` and `solid` keep the texel centre. Empty when n = 1.
        std::uint32_t texel_grid = 1;
        std::vector<DVector> texel_points;
        std::vector<std::uint8_t> texel_solid;
        std::size_t elements() const
        {
            return texels + corners;
        }
    };
    // Blocks: polygons of Main ranges whose triangles WorldMesh lights with their lightmap. Sets:
    // polygons it lights with vertex colours, and lightmapped polygons with colours (DETAIL LOW).
    // Texel samples sit on texel centres; a centre outside the base ring or nearer than edge_inset to
    // its boundary moves to the closest boundary point, then toward the centroid by min(edge_inset,
    // half the distance); corners move toward the centroid the same way; every sample is then raised
    // sample_offset along the normal. texel_grid (1-4, lot GQ1) adds the texel points.
    bool gi_receivers(const World&, const GiBakeScene&, std::span<const WorldTriangleSource>,
                      std::span<const GiRangeUse>, unsigned jobs, GiReceivers&, std::string& error,
                      std::uint32_t texel_grid = 1);
    // The texel-space offset of point k of an n x n texel grid from the texel centre (lot GQ1):
    // ((k % n + 0.5) / n - 0.5, (k / n + 0.5) / n - 0.5).
    std::array<double, 2> gi_texel_point_offset(std::uint32_t k, std::uint32_t n);

    // The indirect light of every element.
    struct GiIndirect
    {
        std::vector<float> energy;       // E per element, RGB.
        std::vector<std::uint8_t> valid; // 0/1 per element.
    };
    struct GiLightmapReport
    {
        std::size_t blocks = 0, texels = 0, sets = 0, corners = 0, low_sets = 0, low_corners = 0;
        std::size_t solid = 0, back_invalid = 0;          // Invalid elements by cause.
        std::size_t points = 0, solid_points = 0, back_points = 0; // Lot GQ1 texel points, invalid by cause.
        std::uint32_t denoise = 0;                        // Lot GQ1 filter radius.
        std::uint64_t denoised = 0;                       // Texels the filter changed (last pass).
        double denoise_ms = 0;                            // All passes.
        std::size_t filled = 0;                           // Invalid elements filled inside their receiver.
        std::size_t neighbour_filled = 0, unfilled = 0;   // Receivers with no valid element.
        std::uint64_t rays = 0, back = 0, sky = 0, untextured = 0, layer_limit = 0;
        std::array<std::uint64_t, 10> back_fraction{};    // Traced elements by tenth of back-face rays.
        GiTraceStats trace;
        unsigned threads = 0;
        std::vector<double> pass_ms;
        double ms = 0;
        bool cancelled = false;
    };
    // Decision D9/D10 radiance at a hit: lightmapped faces albedo x 2 x bilinear min(D + E/2, 1),
    // vertex-lit faces albedo x min(D + E, 1), bright faces their albedo, untextured faces and back
    // faces 0, misses and sky portals the sky; translucent layers composite in front of the hit.
    // Pass b reads the state of pass b - 1; invalid elements are traced once, DETAIL LOW sets only in
    // the last pass. `seed` keys every element's lattice shift with (element, pass). `direct` (texels
    // x 3, 0-1) replaces the texels' D = (O - A_eff) / 255 when given (lot GD2); corners keep D.
    //
    // Lot GQ1. With receivers built at texel_grid n > 1, each texel splits ceil(rays / n^2) rays per
    // point among its points (shift keyed by (element << 4 | point, pass + 256)); points in solid
    // space are never traced, a point decides its validity on its first trace like an element, and
    // the texel takes the mean of its valid points (valid when one is). `denoise` R > 0 filters the
    // texels' E after each pass's dilation (gi_denoise_texels).
    bool bake_gi_lightmaps(const GiBakeScene&, const GiSky&, const GiReceivers&, std::uint32_t bounces,
                           std::uint32_t rays, std::uint64_t seed, unsigned jobs, const std::atomic<bool>* cancel,
                           const GiProgress&, GiIndirect&, GiLightmapReport&, std::string& error,
                           std::span<const float> direct = {}, std::uint32_t denoise = 0);
    // Lot GQ1 denoise of the texels' E (elements x 3, texels first), in place. Each texel q within R
    // texels of texel p, in p's block and in its neighbouring blocks (neighbour_first, texels located
    // with the block frame), weighs w = (1 - d^2 / h^2)^2 (d: distance between the samples, h = 20 (R
    // + 1/2)) times rho^2 (rho: the smaller over the larger of the two hit-distance guides), and only
    // when q is valid (`valid`, texels). p takes the weighted mean, or keeps its value when no weight
    // remains. Only + - * /; the result does not depend on `jobs`. Returns the texels changed.
    std::uint64_t gi_denoise_texels(const GiReceivers&, std::span<const std::uint8_t> valid,
                                    std::span<const float> guide, std::span<float> energy, std::uint32_t radius,
                                    unsigned jobs);
    // The LMAP and VTX contents of a bake.
    void gi_lightmap_sections(const GiReceivers&, const GiIndirect&, GiLightmaps&, GiVertexLight&);

    // What the passes count per ray (summed per thread).
    struct GiRayCounters
    {
        GiTraceStats trace;
        std::uint64_t back = 0, sky = 0, untextured = 0, layer_limit = 0;
    };
    // The radiance a traced ray brings back (decisions D9/D10), from a light state (per element RGB,
    // 0-1, as the passes keep it): the rule bake_gi_lightmaps applies, shared with the light probes
    // (lot G9). `back` is set for a back-face hit (radiance 0).
    void gi_hit_radiance(const GiBakeScene&, const GiSky&, const GiReceivers&, const float* light, const GiHit&,
                         DVector direction, GiRayCounters&, double out[3], bool& back);
    // The light state bake_gi_lightmaps leaves after its last pass: min(D + E/2, 1) per texel and
    // min(D + E, 1) per corner, D alone for DETAIL LOW corners (they feed no lookup); `direct` as there.
    void gi_light_state(const GiReceivers&, const GiIndirect&, std::vector<float>& light,
                        std::span<const float> direct = {});

    // Per-element values at a face point: bilinear over a lightmap block (clamped, centres at i + 0.5)
    // or interpolated over a set's corners. False for faces without a receiver.
    bool gi_receiver_sample(const GiReceivers&, std::span<const float> values, std::uint32_t face, float u, float v,
                            DVector& out);

    // Direction i of n for an element: the rank-1 lattice (i + 0.5) / n, i / golden ratio shifted by
    // (s1, s2) and wrapped, through the concentric disk to the cosine-weighted hemisphere of the unit
    // normal (Duff et al. basis). + - * / and sqrt only.
    DVector gi_cosine_direction(DVector normal, std::uint32_t i, std::uint32_t n, double s1, double s2);
    // Shirley-Chiu concentric map of (a, b) in [-1, 1]^2 to the unit disk, the arithmetic of
    // gi_cosine_direction (lot GQ1 shares it with the soft shadow disk).
    void gi_concentric_disk(double a, double b, double& x, double& y);
    // sin and cos on [-pi/4, pi/4] (Taylor to the 13th / 14th power).
    void gi_quarter_sin_cos(double angle, double& sine, double& cosine);
} // namespace OpenBlood2
