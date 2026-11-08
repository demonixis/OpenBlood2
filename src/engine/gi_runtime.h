#pragma once
#include "engine/gi_direct.h"
#include "engine/gi_file.h"
#include "engine/gi_lightmap.h"
#include "engine/gi_probes.h"
#include "engine/gi_reflections.h"
#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

// Baked GI in the world (gi_contracts.md, lot G8): the LMAP and VTX sections of the current world's
// Ready file, composed (decision D1) into the lightmap pages and vertex colours WorldMesh::build emits.
// Nothing is baked at load: a missing, stale, foreign or invalid file leaves the original light.
namespace OpenBlood2
{
    // F8 Runtime views. Each one is a recomposition of the pages and corners; Lighting only also draws
    // the world's textures white.
    enum class GiView : std::uint8_t
    {
        Final,    // gi_compose with k and f.
        Original, // The stored light, byte for byte (the same pages as GI off).
        Direct,   // D = O - min(O, A): the stored light without its ambient fill (D' for a re-baked file).
        Indirect, // k E alone (no direct light, no floor).
        Lighting, // Final over white textures (texture alpha kept).
        Validity  // Green where the element's own sample was valid, red where it was filled.
    };
    inline constexpr std::array<GiView, 6> gi_views{GiView::Final,    GiView::Original, GiView::Direct,
                                                    GiView::Indirect, GiView::Lighting, GiView::Validity};
    const char* gi_view_name(GiView);
    // Where the world's E comes from (lot G9): the baked texels and corners (default), or the light
    // probes evaluated at each texel centre and corner with its face normal (an F8 comparison).
    enum class GiWorldSource : std::uint8_t
    {
        Texels, // "Texel gather": the LMAP and VTX sections.
        Probes  // "Light probes": the PROB section over the PLAC lattice.
    };
    const char* gi_world_source_name(GiWorldSource);

    // What WorldMesh::build composes. k and f are composition parameters: no new bake is needed.
    struct GiComposition
    {
        float k = gi_default_k, f = gi_default_f;
        GiView view = GiView::Final;
        GiWorldSource source = GiWorldSource::Texels;
        bool operator==(const GiComposition&) const = default;
    };
    constexpr float gi_max_intensity = 4; // The k and f range (as --gi-k and --gi-f).
    bool valid_gi_composition(const GiComposition&);
    // Light probes on models (lot G9): the F8 Runtime switch and the probe intensity p (0-4, the
    // model term is k p E / 2). Changing them never rebuilds the world mesh.
    struct GiProbeSettings
    {
        bool models = true;
        float intensity = 1;
        bool operator==(const GiProbeSettings&) const = default;
    };

    // The decoded, checked indirect light of one Ready file, for one world.
    struct GiWorldLight
    {
        GiLightmaps lightmaps;
        GiVertexLight vertex;
        // The DIR section of a re-baked file (lot GD2): D' per LMAP texel, which replaces D in every view
        // but Original and Validity. Empty for an original bake.
        GiDirectSection direct;
        bool rebaked = false;
        std::vector<std::uint32_t> block_first, set_first; // First value of each record.
        DVector ambient{}; // world_info_ambient_any_case (0-255); zero when the info has none.
        GiComposition composition;
        // The PROB section over the PLAC lattice (lot G9); empty when the file has none.
        GiProbeField probes;
        bool has_probes = false;
        // The Light probes world source in LMAP/VTX order (RGB9E5 E and validity), built on demand by
        // prepare_probe_source; compose uses it while composition.source is Probes.
        std::vector<std::uint32_t> probe_texels, probe_corners;
        std::vector<std::uint8_t> probe_texel_valid, probe_corner_valid;
        bool probe_source_ready = false;
        double probe_source_ms = 0;
        // The REFL section and the PLAC reflection probes it was baked for (lots G11, G12); empty when the
        // file has none. The enhanced renderer draws them (REFLECTION PROBES), never WorldMesh::build.
        GiReflections reflections;
        std::vector<GiReflectionProbe> reflection_probes;
        bool has_reflections = false;
        // E from the probes at every listed texel centre and corner, with the face normal. Elements
        // without a valid probe get 0 (the original light at f = 1). False without probes.
        bool prepare_probe_source(const World&, std::string& error);
        // The record of a polygon, or -1 when the file does not list it.
        std::int64_t block(std::uint32_t model, std::uint32_t polygon) const;
        std::int64_t set(std::uint32_t model, std::uint32_t polygon) const;
        // The composed 8-bit light of texel or corner `index` (an index into lightmaps.texels or
        // vertex.corners) whose stored light is `original`, under `composition`. `share` (lot G12, the
        // page alpha) receives the part of that light that is not GI indirect light beyond the ambient
        // fill it replaces: 255 - round(255 L(min(X, P)) / L(P)), with P the composed value, X =
        // max(0, k E s - f A_eff) per channel and L the shader's luminance (.299, .587, .114); 255 when X
        // is 0 or P is black, and in the Original, Direct and Validity views; 0 in the Indirect view.
        std::array<std::uint8_t, 3> compose(bool texel, std::uint32_t index, std::array<std::uint8_t, 3> original,
                                            std::uint8_t* share = nullptr) const;
    };
    // Decodes the LMAP and VTX sections (both required) and checks them against the world, DIR when
    // present (one value per LMAP texel), then PLAC with PROB and REFL when the file has them (a PROB
    // that does not match its lattice, or a REFL that does not match the PLAC reflection probes, fails
    // the file). The output is untouched on failure.
    bool gi_world_light(const World&, const GiFile&, GiWorldLight&, std::string& error);

    enum class GiRuntimeState : std::uint8_t
    {
        Off,     // GLOBAL ILLUMINATION and REFLECTION PROBES are off: nothing is looked up.
        NoWorld, // No world loaded.
        Missing, // No Ready file (every candidate is Missing, Stale, Foreign or Failed).
        Failed,  // The content keys, the lookup or the sections failed.
        Applied  // The Ready file is composed into the world.
    };
    const char* gi_runtime_state_name(GiRuntimeState);
    struct GiRuntimeStatus
    {
        GiRuntimeState state = GiRuntimeState::Off;
        std::string world_path, file, detail; // file: the chosen candidate; detail: why it is not applied.
        std::string settings;                 // The applied file's bake settings (INFO text).
        bool rebaked = false;                 // The applied file has a re-baked direct source (DIR).
        // GLOBAL ILLUMINATION composes the file (lot G12: false when it was read for reflection probes only).
        bool lighting = true;
        GiLookup lookup;                      // Every candidate, without the file's payload.
        std::size_t blocks = 0, texels = 0, sets = 0, corners = 0;
        std::size_t probes = 0, valid_probes = 0; // PROB entries (0 when the file has none).
        std::uint32_t probe_spacing = 0;
        std::size_t reflection_probes = 0, valid_reflections = 0; // REFL chains (0 when the file has none).
        std::uint32_t reflection_resolution = 0, reflection_levels = 0;
        // Load cost: content keys (DAT and albedo digests), file lookup, section decode and checks.
        double expectation_ms = 0, lookup_ms = 0, decode_ms = 0;
        std::uint64_t loads = 0; // Lookups since construction; identifies the decoded data.
    };
    // One log line: what was applied with its load cost, or why nothing was.
    std::string gi_status_line(const GiRuntimeStatus&);
    // Looks up and decodes the GI of a mounted world through the search roots. Never bakes or writes.
    void load_gi_world_light(const Resources&, const World&, std::string_view world_path, std::span<const GiRoot>,
                             std::optional<GiWorldLight>&, GiRuntimeStatus&);
} // namespace OpenBlood2
