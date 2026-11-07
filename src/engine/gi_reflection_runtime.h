#pragma once
#include "engine/gi_reflections.h"
#include <array>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

// Reflection probes in the enhanced renderer (gi_contracts.md, lot G12): the REFL chains of the current
// world's file as an RGBA16F octahedral 2D array (one layer per valid probe, decision D12), the probe
// records and a cell grid of candidate probes, in the layouts the GI variant of pbr.frag reads. Pure CPU
// data: the presenter uploads it, and gi_reflection_environment mirrors the shader for tests.
namespace OpenBlood2
{
    // Options > Display REFLECTION PROBES (setting 32).
    enum class GiReflectionMode : std::uint8_t
    {
        Off,  // Not drawn: the enhanced shaders keep the baked light as their environment.
        Low,  // One probe per pixel, chains from their second level (a quarter of the memory).
        High  // Up to two probes per pixel, whole chains.
    };
    const char* gi_reflection_mode_name(GiReflectionMode);
    namespace gi_reflection_runtime_limits
    {
        constexpr std::uint32_t cell_probes = 8;      // Candidates per cell (the shader reads at most 8).
        constexpr std::uint32_t axis_cells = 128;     // Cells per axis.
        constexpr std::uint32_t cells = 65536;        // Cells in all.
        constexpr double min_cell = 64;               // Smallest cell edge (world units), doubled until both hold.
        constexpr double margin = 1;                  // Probe boxes grow by this when listed in cells.
        constexpr std::size_t header = 3, record = 5; // float4 per header and per probe record.
        constexpr float max_intensity = 4;
    } // namespace gi_reflection_runtime_limits
    // The F8 Reflection probes panel. Changing it never rebuilds the data, only the header.
    struct GiReflectionView
    {
        float intensity = 1; // Multiplies the probes' radiance (0-4).
        bool only = false;   // "Reflections only": the probes' radiance alone, black where none covers.
        bool operator==(const GiReflectionView&) const = default;
    };
    bool valid_gi_reflection_view(const GiReflectionView&);
    // The presenter's reflection state after its last update (inspection only).
    struct GiReflectionStats
    {
        bool gi_variant = false; // The enhanced pipelines are the GI variants (GI pages or probes).
        bool active = false;     // Probes are drawn (REFLECTION PROBES on and a file with a valid probe).
        GiReflectionMode mode = GiReflectionMode::Off;
        GiReflectionSet set = GiReflectionSet::Original;
        std::uint32_t layers = 0, size = 0, levels = 0, dropped = 0;
        std::array<std::uint32_t, 3> cells{};
        std::size_t occupied = 0, overflow = 0, lists = 0, texture_bytes = 0, buffer_bytes = 0;
        double cell_size = 0, build_ms = 0;
        std::uint64_t builds = 0;
        std::string detail; // Why probes are not drawn while REFLECTION PROBES is on.
    };

    // The GPU data of one set of one file at one mode.
    //   records (float4): [0] probes, mode (1 Low, 2 High), chain levels, intensity; [1] dropped levels,
    //   array levels, array size, Reflections only (0/1); [2] grid origin, 1 / cell size; then per probe
    //   (influence min, blend distance), (influence max, priority), (projection min, flags: 1 box
    //   projection), (projection max, layer), (capture point, 0).
    //   cells (u32): cells per axis x, y, z, cell count; one word per cell, x fastest (first index << 4 |
    //   count, the index relative to the list start); then the lists of probe indices (records order).
    //   texels: RGBA16F (alpha 1), layer, then array level, then rows of (size >> level) texels.
    struct GiReflectionGpu
    {
        GiReflectionSet set = GiReflectionSet::Original;
        GiReflectionMode mode = GiReflectionMode::Off;
        std::uint32_t layers = 0, size = 1, levels = 1, dropped = 0, chain_levels = 0;
        std::vector<std::uint16_t> texels;
        std::vector<std::array<float, 4>> records;
        std::vector<std::uint32_t> cells;
        std::vector<std::uint32_t> probe_of_layer; // The PLAC index of each layer (valid probes only).
        DVector origin{};
        double cell_size = 0;
        std::array<std::uint32_t, 3> dimensions{};
        std::size_t occupied = 0, overflow = 0, lists = 0; // Cells with probes, cells over cell_probes, shared lists.
        std::size_t texture_bytes() const
        {
            return texels.size() * 2;
        }
        std::size_t buffer_bytes() const
        {
            return records.size() * 16 + cells.size() * 4;
        }
        // First RGBA16F texel (in texels, four halves each) of an array level of a layer.
        std::size_t level_offset(std::uint32_t layer, std::uint32_t level) const;
    };
    // Data for `mode` (Low or High) from the REFL section and its PLAC probes. Invalid probes are left
    // out; without a valid probe there are no layers (a 1 x 1 placeholder level, probes = 0). The
    // header starts with intensity 1 and Reflections only off.
    bool gi_reflection_gpu(const GiReflections&, std::span<const GiReflectionProbe>, GiReflectionSet, GiReflectionMode,
                           GiReflectionGpu&, std::string& error);
    // The data the GI variant reads while no probe is drawn (GI applied with REFLECTION PROBES off, or
    // a file without REFL): probes = 0 and mode 0, so the shader keeps the baked light.
    GiReflectionGpu gi_reflection_placeholder();
    // Header fields from the panel: records[0].w (intensity) and records[1].w (Reflections only).
    void gi_reflection_header(GiReflectionGpu&, const GiReflectionView&);
    // CPU mirror of pbr.frag's probeEnvironment over the same records, cells and texels (bilinear,
    // clamped to the level's edge, linear between levels), in double precision.
    DVector gi_reflection_environment(const GiReflectionGpu&, DVector position, DVector reflected, double roughness,
                                      DVector fallback);
} // namespace OpenBlood2
