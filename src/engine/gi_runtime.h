#pragma once
#include "engine/gi_file.h"
#include "engine/gi_lightmap.h"
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
        Direct,   // D = O - min(O, A): the stored light without its ambient fill.
        Indirect, // k E alone (no direct light, no floor).
        Lighting, // Final over white textures (texture alpha kept).
        Validity  // Green where the element's own sample was valid, red where it was filled.
    };
    inline constexpr std::array<GiView, 6> gi_views{GiView::Final,    GiView::Original, GiView::Direct,
                                                    GiView::Indirect, GiView::Lighting, GiView::Validity};
    const char* gi_view_name(GiView);

    // What WorldMesh::build composes. k and f are composition parameters: no new bake is needed.
    struct GiComposition
    {
        float k = gi_default_k, f = gi_default_f;
        GiView view = GiView::Final;
        bool operator==(const GiComposition&) const = default;
    };
    constexpr float gi_max_intensity = 4; // The k and f range (as --gi-k and --gi-f).
    bool valid_gi_composition(const GiComposition&);

    // The decoded, checked indirect light of one Ready file, for one world.
    struct GiWorldLight
    {
        GiLightmaps lightmaps;
        GiVertexLight vertex;
        std::vector<std::uint32_t> block_first, set_first; // First value of each record.
        DVector ambient{}; // world_info_ambient_any_case (0-255); zero when the info has none.
        GiComposition composition;
        // The record of a polygon, or -1 when the file does not list it.
        std::int64_t block(std::uint32_t model, std::uint32_t polygon) const;
        std::int64_t set(std::uint32_t model, std::uint32_t polygon) const;
        // The composed 8-bit light of texel or corner `index` (an index into lightmaps.texels or
        // vertex.corners) whose stored light is `original`, under `composition`.
        std::array<std::uint8_t, 3> compose(bool texel, std::uint32_t index,
                                            std::array<std::uint8_t, 3> original) const;
    };
    // Decodes the LMAP and VTX sections (both required) and checks them against the world. The output
    // is untouched on failure.
    bool gi_world_light(const World&, const GiFile&, GiWorldLight&, std::string& error);

    enum class GiRuntimeState : std::uint8_t
    {
        Off,     // GLOBAL ILLUMINATION is off: nothing is looked up.
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
        GiLookup lookup;                      // Every candidate, without the file's payload.
        std::size_t blocks = 0, texels = 0, sets = 0, corners = 0;
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
