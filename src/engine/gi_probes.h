#pragma once
#include "engine/gi_lightmap.h"
#include "engine/gi_placement.h"
#include <array>
#include <atomic>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

// Light probes for baked GI (gi_contracts.md, lot G9): SH L2 irradiance captured at every probe of
// the placement lattice from the light state the lightmap passes leave and the sky, stored in the
// PROB section, sampled per model instance on the CPU (validity-weighted trilinear) and split into
// an ambient term and one dominant-direction pseudo-light.
//
// Units (decision D1): a probe stores E(n), the cosine-weighted mean of the display radiance
// arriving at it for a surface normal n, in the units of the LMAP/VTX E: the irradiance SH divided
// by pi, so the coefficients carry the band factors 1, 2/3 and 1/4 (CyberEngine's pi, 2pi/3, pi/4
// over pi).
namespace OpenBlood2
{
    namespace gi_probe_limits
    {
        constexpr std::uint32_t format = 1;          // PROB payload format.
        constexpr std::uint32_t coefficients = 9;    // SH L2.
        constexpr std::uint32_t values = 27;         // Coefficients x RGB.
        constexpr std::uint32_t max_probes = 262144; // The G2 limit (gi_placement_limits::max_budget).
        constexpr std::uint32_t chunk = 16;          // Probes per job.
        // A probe is invalid when more than half its rays hit back faces ("mostly", decision 3).
        constexpr double back_limit = .5;
        // Model light (decision 3): a cached sample is reused until its instance changes cell or
        // moves farther than this.
        constexpr double reuse_distance = 16;
        // The pseudo-light is a point far along its direction with a range that keeps its
        // attenuation at 1 for any vertex: a directional light for the original light term.
        constexpr double pseudo_distance = 1e5;
        constexpr float pseudo_range = 1e20f;
    } // namespace gi_probe_limits
    // The random stream of probe rotations, disjoint from the lightmap passes (1-3).
    constexpr std::uint64_t gi_probe_stream = 0x50524F4200000000ull; // "PROB" << 32.

    // IEEE binary16, round to nearest even; NaN encodes 0 and magnitudes above 65504 saturate.
    std::uint16_t gi_half(float);
    float gi_half_decode(std::uint16_t); // Exponent 31 (infinity, NaN) decodes as 0.

    // Real SH basis (l <= 2) in the usual order: Y00, Y1-1 (y), Y10 (z), Y11 (x), Y2-2 (xy),
    // Y2-1 (yz), Y20 (3z^2 - 1), Y21 (xz), Y22 (x^2 - y^2).
    void gi_sh_basis(DVector direction, double out[9]);
    // E(n) of a probe's 27 values (coefficient-major, then RGB).
    DVector gi_sh_irradiance(std::span<const float, 27> sh, DVector normal);
    // Point i of an n-point Fibonacci sphere: z = 1 - (2i + 1)/n, golden-angle azimuth through
    // gi_quarter_sin_cos (+ - * / and sqrt only).
    DVector gi_fibonacci_direction(std::uint32_t i, std::uint32_t n);
    // A uniform random rotation per probe (GiRandom(seed, probe, gi_probe_stream), rejection in
    // the unit 4-ball): w, x, y, z.
    std::array<double, 4> gi_probe_rotation(std::uint64_t seed, std::uint64_t probe);
    // Ray i of n of a probe: its Fibonacci point turned by its rotation.
    DVector gi_probe_direction(const std::array<double, 4>& rotation, std::uint32_t i, std::uint32_t n);

    // The PROB section: one entry per lattice probe, in PLAC order.
    struct GiProbes
    {
        std::vector<float> sh;           // 27 per probe: E(n) coefficients, coefficient-major, then RGB.
        std::vector<std::uint8_t> valid; // 0/1 per probe.
        std::size_t size() const
        {
            return valid.size();
        }
    };
    // Payload: u32 format (1), u32 probes, u32 zero; 27 binary16 per probe; validity bits LSB-first
    // with zero padding. The reader is strict (format, counts, size, reserved field, finite halves,
    // padding) and the writer quantizes.
    bool write_gi_probes(const GiProbes&, std::vector<std::uint8_t>&, std::string& error);
    bool read_gi_probes(std::span<const std::uint8_t>, GiProbes&, std::string& error);
    // One entry per lattice probe.
    bool check_gi_probes(const GiLightLattice&, const GiProbes&, std::string& error);

    struct GiProbeReport
    {
        std::size_t probes = 0, valid = 0;
        std::uint64_t rays = 0, back = 0, sky = 0, untextured = 0, layer_limit = 0;
        std::array<std::uint64_t, 10> back_fraction{}; // Probes by tenth of back-face rays.
        GiTraceStats trace;
        unsigned threads = 0;
        double ms = 0;
        bool cancelled = false;
    };
    // Captures every probe with `rays` Fibonacci directions turned by its rotation, from `light`
    // (gi_light_state) and the sky: SH projection with weight 4 pi / rays, then the cosine
    // convolution. Back-face rays bring 0 and decide validity. Identical for any thread count.
    bool bake_gi_probes(const GiBakeScene&, const GiSky&, const GiReceivers&, std::span<const float> light,
                        std::span<const GiLightProbe>, std::uint32_t rays, std::uint64_t seed, unsigned jobs,
                        const std::atomic<bool>* cancel, const GiProgress&, GiProbes&, GiProbeReport&,
                        std::string& error);

    // A blended probe sample at a point.
    struct GiProbeSample
    {
        bool valid = false;           // At least one valid probe has weight.
        std::array<float, 27> sh{};   // Weighted mean of the valid probes.
        std::array<std::uint32_t, 3> cell{};
        // The eight lattice corners: probe index (-1: no probe at that node) and the normalized
        // weight (0 for missing and invalid probes).
        std::array<std::int64_t, 8> probes{};
        std::array<float, 8> weights{};
    };
    // The probes of one Ready file over their lattice (from the PLAC section).
    class GiProbeField
    {
      public:
        bool build(GiLightLattice, GiProbes, std::string& error);
        // Trilinear over the cell holding the point (clamped to the lattice), weights of missing and
        // invalid probes dropped and the rest renormalized. False when no valid probe has weight.
        bool sample(DVector point, GiProbeSample&) const;
        // The cell sample() would blend (clamped to the lattice).
        void cell(DVector point, std::array<std::uint32_t, 3>& cell) const;
        std::int64_t probe_at(std::uint32_t node) const; // Binary search; -1 when the node has none.
        const GiLightLattice& lattice() const
        {
            return lattice_;
        }
        const GiProbes& probes() const
        {
            return probes_;
        }
        std::size_t valid() const
        {
            return valid_;
        }
        bool empty() const
        {
            return probes_.size() == 0;
        }

      private:
        GiLightLattice lattice_;
        GiProbes probes_;
        std::size_t valid_ = 0;
    };

    // Decision 3's split of a sample for the original model light term, in model light units (the
    // page units AmbientLight / 255 uses): with s = scale (k p / 2), e0 the mean of s E and g its
    // linear term, the dominant direction d is g's Rec. 709 luminance direction, c = min(2 max(0,
    // g.d), 4 e0) and a = max(0, e0 - c/4) per channel (a lone directional light is recovered
    // exactly). Then ambient = max(a, f A) and color = max(a + c, f A) - ambient: the floor of
    // decision D1 holds facing the light and away from it, and s E = 0 with f = 1 gives back A.
    struct GiProbeLight
    {
        DVector ambient{}, color{}, direction{0, 0, 1};
    };
    GiProbeLight gi_probe_model_light(std::span<const float, 27> sh, float scale, float floor, DVector ambient_light);
} // namespace OpenBlood2
