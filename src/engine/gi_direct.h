#pragma once
#include "engine/gi_lightmap.h"
#include <array>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

// The re-baked direct light (gi_contracts.md, lot GD1 of the D1 (c) plan): the 1998 preprocessor's
// lightmap term recomputed per stored texel from the DAT's Light and DirLight objects, and the audit
// that measures it against the stored lightmaps. Diagnostic only in GD1: nothing is written to a GI
// file and no frame uses it.
//
// Term (decision D16, measured on the 36 single-player worlds):
//   texel = AmbientLight + sum over lights within R of BrightScale x T, then clamped to 0-255, with
//   f = 1 - d / R (d from the texel centre) and no N.L:
//   Light: T = LightColor x f + OuterColor x (1 - f);
//   DirLight inside its cone: T = (InnerColor x a + OuterColor x (1 - a)) x f,
//   a = (cos - cos(FOV / 2)) / (1 - cos(FOV / 2)); outside it, nothing.
// These are DEdit's lightmap rules (tools/DEdit/Lightmap/LMSampleGen.cpp) with the linear falloff.
// Visibility (decision D19): a hard test per light from the receiver sample (the lot-G7 moved sample)
// raised `lift` more along the normal, to `margin` short of the light, against the SOLID polygons of
// the receiver's own BSP only (each BSP shadows itself), SURF_INVISIBLE ones excepted; ClipLight
// false skips the test. The test is a two-sided any-hit segment trace over a GiBvh of those polygons'
// fan triangles (WorldShade's Moller-Trumbore arithmetic, built with -ffp-contract=off), so its
// result does not depend on threads or builds (lot GD2).
namespace OpenBlood2
{
    namespace gi_direct_limits
    {
        constexpr double lift = .5;              // Beyond gi_lightmap_limits::sample_offset.
        constexpr double margin = 1;             // The segment stops this far short of the light.
        constexpr double touch = .01;            // Hits this close to the sample are ignored.
        constexpr std::size_t max_lights = 65536;
        constexpr std::uint32_t chunk = 16;      // Blocks per job.
        // Audit tolerances per channel: one RGB565 step (8, 4, 8) plus 3.
        constexpr std::array<int, 3> tolerance{11, 7, 11};
        constexpr std::uint32_t format = 1; // DIR payload format (lot GD2).
    } // namespace gi_direct_limits

    struct GiDirectLight
    {
        DVector position{};
        std::array<double, 3> inner{}, outer{}; // LightColor/InnerColor, OuterColor: 0-255 x BrightScale.
        double radius = 0;
        std::array<double, 3> direction{0, 0, 1}; // DirLight: Rotation applied to +Z.
        double cos_half = -2;                     // Below -1: no cone (Light, or FOV >= 360).
        bool clip = true;                         // ClipLight (absent: true).
        bool in_solid = false;                    // Its position lies in solid root-BSP space.
        std::uint32_t object = 0;                 // Index in World::objects.
    };
    struct GiDirectLightReport
    {
        std::size_t lights = 0, omni = 0, cones = 0, unclipped = 0, in_solid = 0, negative = 0;
        std::size_t cone_outer = 0, omni_outer = 0; // DirLights and Lights with a nonzero OuterColor.
        std::size_t object_lights = 0, global_dir_lights = 0; // Classes that do not light the world.
        std::size_t invalid = 0;     // Missing or non-finite Pos/LightRadius/colour, R <= 0, bad FOV.
    };
    // Lights in object order. Rotation uses gi_quarter_sin_cos (no library trigonometry).
    bool gi_direct_lights(const World&, std::vector<GiDirectLight>&, GiDirectLightReport&, std::string& error);
    // The term of one light at a point (0-255 units, before ambient and clamping), visibility aside.
    std::array<double, 3> gi_direct_term(const GiDirectLight&, DVector point);
    // sin and cos of any finite angle (quarter-turn reduction, then gi_quarter_sin_cos).
    void gi_sin_cos(double angle, double& sine, double& cosine);

    // One BVH of SOLID polygons per world model (model 0: the root BSP), without SURF_INVISIBLE
    // polygons: fan triangles of each polygon's points, degenerate ones (float |cross|^2 < 1e-12)
    // skipped, `face` the polygon index.
    struct GiDirectOccluders
    {
        std::vector<GiBvh> models;
        std::size_t triangles = 0;
        bool build(const World&, std::string& error);
    };
    // Whether the segment from `from` to `to` meets a triangle of `tree` (two-sided, the far end
    // included); hits within `touch` of `from` are ignored, so a start lying on a face never blocks.
    bool gi_segment_blocked(const GiBvh&, DVector from, DVector to);
    // Whether `light` reaches `sample` on a receiver of `model`.
    bool gi_direct_visible(const GiDirectOccluders&, std::uint16_t model, DVector sample, const GiDirectLight&);

    // The re-baked direct light of every texel of the receivers, in LMAP order.
    struct GiDirectTexels
    {
        std::vector<float> light;          // texels x 3: AmbientLight + sum (0-255 units, unclamped).
        std::vector<std::uint8_t> reaching; // Lights whose term is nonzero there (saturating at 255).
        std::vector<std::uint64_t> visible; // Hash of the set of visible lights (shadow-edge test).
        std::vector<std::uint8_t> placed;   // 0 where the texel centre could not be placed (light 0).
        std::uint64_t tests = 0;            // Visibility tests run.
        double ms = 0;
        bool cancelled = false;
        // The 8-bit value of texel t, channel c: round half up, clamped to 0-255.
        std::uint8_t value(std::size_t t, int c) const;
    };
    // `progress` reports stage "direct" in blocks; a set `cancel` stops it (false, cancelled set).
    bool gi_direct_texels(const World&, const GiReceivers&, std::span<const GiDirectLight>, const GiDirectOccluders&,
                          unsigned jobs, GiDirectTexels&, std::string& error, const std::atomic<bool>* cancel = nullptr,
                          const GiProgress& progress = {});

    // The audit of one world: the re-baked 8-bit texels against the stored ones (RGB565 expanded),
    // over texels whose sample is not in solid space.
    struct GiDirectFit
    {
        std::size_t texels = 0, fit = 0, fit_step = 0, brighter = 0, darker = 0;
        double abs_sum = 0; // Sum of the mean absolute channel difference.
        void add(const std::array<std::uint8_t, 3>& rebaked, const std::array<std::uint8_t, 3>& original);
        double share(std::size_t n) const
        {
            return texels ? double(n) / double(texels) : 0;
        }
        double mean_abs() const
        {
            return texels ? abs_sum / double(texels) : 0;
        }
    };
    struct GiDirectAudit
    {
        GiDirectFit all, interior, edge, primary, world_models;
        std::array<GiDirectFit, 4> by_lights; // 0, 1, 2-3, 4+ lights reaching the texel.
        // By surface flags (SURF_ bits, SOLID included), ascending.
        std::vector<std::pair<std::uint32_t, GiDirectFit>> by_flags;
        std::size_t solid_texels = 0; // Excluded: sample in solid space.
    };
    void gi_direct_audit(const World&, const GiReceivers&, const GiDirectTexels&, GiDirectAudit&);

    // The re-baked direct source of a GI bake (lot GD2, decision D17): D' per LMAP texel, the
    // re-baked texel light without its ambient fill, in the units of D (0-1):
    //   D' = max(0, min(T, 255) - A_eff) / 255, T = AmbientLight + sum (gi_direct_texels), and
    //   A_eff = clamp(min(O, A), 0, O) from the stored texel O, as gi_compose takes it,
    // so that k = 0, f = 1 composes min(T, 255) up to the RGB9E5 step. Texels whose sample lies in solid
    // space or whose centre cannot be placed keep D = O - A_eff. Vertex corners keep their stored
    // colours (decision D18). The values are rounded through RGB9E5, so the bounce reads what the
    // file stores.
    float gi_direct_value(float light, std::uint8_t original, float ambient);
    struct GiDirectSection
    {
        std::vector<std::uint32_t> texels; // RGB9E5 D', LMAP order.
    };
    // Payload: u32 format, u32 values, then the values. The reader is strict (format, count, size).
    bool write_gi_direct(const GiDirectSection&, std::vector<std::uint8_t>&, std::string& error);
    bool read_gi_direct(std::span<const std::uint8_t>, GiDirectSection&, std::string& error);
    struct GiDirectBake
    {
        std::vector<float> direct; // texels x 3: D' after the RGB9E5 round trip (bake_gi_lightmaps' input).
        GiDirectSection section;
        GiDirectLightReport lights;
        std::size_t occluder_triangles = 0, kept = 0, saturated = 0; // kept: solid or unplaced texels.
        std::uint64_t tests = 0;
        double ms = 0;
        bool cancelled = false;
    };
    bool gi_rebake_direct(const World&, const GiReceivers&, unsigned jobs, const std::atomic<bool>* cancel,
                          const GiProgress&, GiDirectBake&, std::string& error);
} // namespace OpenBlood2
