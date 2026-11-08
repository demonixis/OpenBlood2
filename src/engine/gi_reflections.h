#pragma once
#include "engine/gi_lightmap.h"
#include "engine/gi_placement.h"
#include <atomic>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

// Reflection probes for baked GI (gi_contracts.md, lot G11): each PLAC reflection probe captures a
// cube of the world around it, which a CPU GGX prefilter turns into an octahedral chain of
// roughness levels, stored in the REFL section for the enhanced renderer's environment term (G12).
//
// Two sets per probe: the world as GI off draws it (the stored lightmaps and vertex colours) and as
// GI on draws it (the composition at gi_default_k and gi_default_f, the re-baked direct light
// included). Values are display radiance (albedo times drawn light, 0-1 per channel, unbounded), the
// units of the light probes' rays.
//
// The prefilter follows CyberEngine (reference only, reimplemented): roughness = level / (levels -
// 1), alpha = max(roughness^2, 0.001), Hammersley samples (64, or 32 above a 128 face), N = V = R
// weighted by N.L, bilinear base-cube samples within a face.
namespace OpenBlood2
{
    namespace gi_reflection_limits
    {
        constexpr std::uint32_t format = 1;         // REFL payload format.
        constexpr std::uint32_t sets = 2;           // Original, then composed.
        constexpr std::uint32_t subsamples = 2;     // Capture rays per cube texel and axis (default strata).
        constexpr std::uint32_t smallest_level = 4; // The chain ends at 4 x 4 (2 x 2 inside the border).
        constexpr std::uint32_t max_levels = 7;     // 256 down to 4.
        constexpr double back_limit = .5;           // Invalid when more than half the rays hit back faces.
        constexpr std::uint32_t samples = 64, wide_samples = 32, wide_face = 128; // GGX samples per texel.
        constexpr double min_alpha = .001;
    } // namespace gi_reflection_limits
    enum class GiReflectionSet : std::uint32_t
    {
        Original = 0, // The stored light: what GI off draws.
        Composed = 1  // gi_compose at the default k and f: what GI on draws.
    };

    // Octahedral map with +y as the pole: a unit direction's L1-normalized x and z on the upper
    // hemisphere, folded into the corners below. Coordinates in [-1, 1].
    void gi_octahedral_coordinates(double x, double y, double z, double& u, double& v);
    DVector gi_octahedral_direction(double u, double v); // Normalized.
    // Levels of a chain whose top level is `resolution` (a power of two, 16-256): down to 4 x 4.
    std::uint32_t gi_reflection_levels(std::uint32_t resolution);
    // Texels before `level` and in the whole chain (every level square, borders included).
    std::size_t gi_reflection_level_offset(std::uint32_t resolution, std::uint32_t level);
    std::size_t gi_reflection_chain_texels(std::uint32_t resolution);

    // The REFL section: one chain per set and PLAC reflection probe, at one resolution (the largest
    // PLAC resolution).
    struct GiReflections
    {
        std::uint32_t resolution = 0, levels = 0; // Both 0 without probes.
        std::vector<std::uint8_t> valid;          // 0/1 per probe.
        // RGB9E5: set, then probe, then level, then rows of (resolution >> level) texels. A level of size
        // s holds the octahedral map in its (s - 2)^2 interior and a one-texel border that repeats the
        // map's folded neighbours, so bilinear filtering never crosses into another level or probe.
        std::vector<std::uint32_t> texels;
        std::size_t size() const
        {
            return valid.size();
        }
        std::size_t first(GiReflectionSet set, std::size_t probe, std::uint32_t level) const;
    };
    // Payload: u32 format (1), probes, resolution, levels, sets (2), zero; u32 flags per probe (bit 0:
    // valid); the texels. The reader is strict: format, sets, resolution (a power of two, 16-256, or 0
    // without probes), levels, reserved field, flags, the probe limit and the size.
    bool write_gi_reflections(const GiReflections&, std::vector<std::uint8_t>&, std::string& error);
    bool read_gi_reflections(std::span<const std::uint8_t>, GiReflections&, std::string& error);
    // One chain per PLAC reflection probe, at their largest resolution.
    bool check_gi_reflections(std::span<const GiReflectionProbe>, const GiReflections&, std::string& error);

    // A cube of RGB values: faces +X, -X, +Y, -Y, +Z, -Z, rows of `size` texels (CyberEngine's
    // layout: face 0 (1, -v, -u), 1 (-1, -v, u), 2 (u, 1, v), 3 (u, -1, -v), 4 (u, -v, 1), 5 (-u, -v,
    // -1), with u, v in [-1, 1] from the texel's left and top edges).
    struct GiReflectionCube
    {
        std::uint32_t size = 0;
        std::vector<float> texels;
    };
    DVector gi_reflection_cube_direction(unsigned face, double u, double v); // Normalized.
    // Bilinear within the direction's face, edges clamped (CyberEngine SampleBaseCubemap).
    void gi_reflection_cube_sample(const GiReflectionCube&, DVector direction, double out[3]);
    // The GGX half vector of Hammersley point (sx, sy) around a unit normal (CyberEngine
    // ImportanceSampleGgx; sine and cosine through gi_sin_cos).
    DVector gi_ggx_half_vector(double sx, double sy, DVector normal, double roughness);
    // The prefiltered radiance around `normal` (N = V = R): `samples` Hammersley points weighted by
    // N.L, the cube at `normal` when no sample has weight.
    void gi_ggx_prefilter(const GiReflectionCube&, DVector normal, double roughness, std::uint32_t samples,
                          double out[3]);
    // The chain of one cube at `resolution`, RGB9E5, borders filled: level 0 averages 2 x 2 cube
    // samples per texel, level l > 0 is the GGX prefilter at roughness l / (levels - 1).
    // `ggx_samples` (lot GQ1) replaces the 64 / 32 rule when nonzero.
    void gi_reflection_chain(const GiReflectionCube&, std::uint32_t resolution, std::span<std::uint32_t> out,
                             std::uint32_t ggx_samples = 0);

    // The two light states the capture reads (per receiver element, RGB, 0-1 as drawn): the stored
    // light O / 255, and the composition the runtime's Final view draws at gi_default_k and
    // gi_default_f (gi_compose, or gi_compose_direct for the texels of a re-baked bake: `direct` holds
    // its DIR texels, empty otherwise), from the section values as the runtime decodes them.
    bool gi_reflection_light(const GiReceivers&, const GiLightmaps&, const GiVertexLight&,
                             std::span<const std::uint32_t> direct, std::vector<float>& original,
                             std::vector<float>& composed, std::string& error);

    struct GiReflectionReport
    {
        std::size_t probes = 0, valid = 0;
        std::uint32_t resolution = 0, levels = 0;
        std::uint64_t rays = 0, back = 0, sky = 0, untextured = 0, layer_limit = 0;
        GiTraceStats trace;
        unsigned threads = 0;
        double capture_ms = 0, prefilter_ms = 0; // Summed over the jobs (thread time).
        double ms = 0;
        bool cancelled = false;
    };
    // Captures every probe: six faces at its PLAC resolution, 2 x 2 fixed rays per texel from the
    // capture point, each traced once and evaluated against both light states (gi_hit_radiance) and
    // `sky` (the displayed sky: scale 1); then the two chains. One job per probe; identical for any
    // thread count. Lot GQ1: `subsamples` (2-8) rays per cube texel axis, `ggx_samples` as above.
    bool bake_gi_reflections(const GiBakeScene&, const GiSky& sky, const GiReceivers&, std::span<const float> original,
                             std::span<const float> composed, std::span<const GiReflectionProbe>, unsigned jobs,
                             const std::atomic<bool>* cancel, const GiProgress&, GiReflections&,
                             GiReflectionReport&, std::string& error, std::uint32_t subsamples = 2,
                             std::uint32_t ggx_samples = 0);
} // namespace OpenBlood2
