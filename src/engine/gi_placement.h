#pragma once
#include "engine/collision.h"
#include "engine/gi_world.h"
#include "engine/sha256.h"
#include "engine/world.h"
#include <array>
#include <cstdint>
#include <filesystem>
#include <span>
#include <string>
#include <string_view>
#include <vector>

// Probe placement for baked GI (gi_contracts.md, lot G4). A placement file holds the editable
// inputs (lattice settings, include/exclude volumes, reflection probes); the light-probe lattice
// is derived from it and the world deterministically, so the file stays small and reviewable.
namespace OpenBlood2
{
    namespace gi_placement_limits
    {
        constexpr std::uint32_t min_spacing = 32, max_spacing = 1024; // Multiples of 8.
        constexpr std::uint32_t max_budget = 262144;                  // The G2 probe limit.
        constexpr float max_clearance = 64;
        constexpr std::size_t max_volumes = 256;
        constexpr std::uint32_t max_reflections = 42;
        constexpr std::uint32_t min_resolution = 16, max_resolution = 256; // Powers of two.
        constexpr std::int32_t max_priority = 1000;
        constexpr float max_blend = 1024;
        constexpr float max_coordinate = 1e6f;
        constexpr std::uint64_t max_nodes = 16u * 1024 * 1024; // Lattice nodes per run.
        constexpr std::size_t max_text = 256 * 1024;            // Placement JSON bytes.
        constexpr std::size_t max_excluded = 256, max_model_name = 128; // Bake exclusions (D8).
    } // namespace gi_placement_limits
    // Bumped whenever the same placement file and world would resolve to different probes.
    constexpr std::uint32_t gi_placement_algorithm = 1;

    struct GiBox
    {
        DVector min{}, max{};
        bool contains(DVector p) const
        {
            return p.x >= min.x && p.x <= max.x && p.y >= min.y && p.y <= max.y && p.z >= min.z && p.z <= max.z;
        }
    };
    // Include volumes restrict the lattice to their union (when any exists); exclude volumes remove
    // the nodes they contain.
    struct GiProbeVolume
    {
        bool include = false;
        GiBox box;
    };
    // CyberEngine vocabulary: influence box (where it applies), projection box (parallax), blend
    // distance, priority, box projection, capture resolution.
    struct GiReflectionProbe
    {
        bool automatic = true; // Placed by the tool; any edit makes it manual (kept by re-placement).
        DVector capture{};     // Inside the projection box.
        GiBox influence, projection;
        float blend_distance = 32;
        std::int32_t priority = 0;
        bool box_projection = true;
        std::uint32_t resolution = 64;
    };
    struct GiPlacement
    {
        std::string world_path; // Normalized resource path, e.g. "worlds/01_subwaya.dat".
        Sha256Digest world{};   // The DAT the placement was made for.
        std::uint32_t spacing = 128, budget = 16384;
        float clearance = 8; // Half size of the cube a probe needs free of primary collision faces.
        std::vector<GiProbeVolume> volumes;
        std::uint32_t reflection_cap = 42;
        std::vector<GiReflectionProbe> reflections; // Manual probes first, then automatic ones by rank.
        // World models left out of the bake scene (decision D8), by DAT name, case-insensitive:
        // printable ASCII without '"' or '\\', unique. Placement validity ignores them.
        std::vector<std::string> excluded_models;
    };
    GiPlacement default_gi_placement(std::string world_path, const Sha256Digest& world);
    bool validate_gi_placement(const GiPlacement&, std::string& error);
    // Canonical JSON: fixed key order and layout, shortest round-trip floats. Same placement, same bytes.
    bool write_gi_placement(const GiPlacement&, std::string& text, std::string& error);
    // Strict: every key present exactly once, no unknown key, values in range. Output untouched on failure.
    bool read_gi_placement(std::string_view text, GiPlacement&, std::string& error);
    // SHA-256 of the canonical text and gi_placement_algorithm: the .ob2gi placement key.
    bool gi_placement_hash(const GiPlacement&, Sha256Digest&, std::string& error);
    // "worlds/01_subwaya.json" for "Worlds\01_SubwayA.dat".
    bool gi_placement_name(std::string_view world_path, std::string& name);
    // The placement directory: --gi-placement-dir or gi_placement_dir=; else, in GI tool builds, the
    // checkout's data/gi/placement when data/gi exists; else user_directory()/gi/placement.
    bool gi_placement_directory(const std::filesystem::path& configured, std::filesystem::path& directory,
                                std::string& error);
    // GI tool builds of a checkout: its data/gi folder when it exists (decision 8); false otherwise.
    bool gi_project_directory(std::filesystem::path& directory);
    // What a placement file is to the mounted world.
    enum class GiPlacementStatus
    {
        Missing,
        Ready,
        Foreign, // Valid, but made for another world path or DAT digest.
        Failed   // Unreadable or invalid.
    };
    const char* gi_placement_status_name(GiPlacementStatus);
    // Reads `directory/<name>` and checks it against the world. `placement` is set only when Ready.
    GiPlacementStatus load_gi_placement(const std::filesystem::path& directory, std::string_view world_path,
                                        const Sha256Digest& world, GiPlacement& placement, std::string& detail);
    // Atomic write of the canonical text; the previous file survives any failure.
    bool save_gi_placement(const std::filesystem::path& directory, const GiPlacement&, std::string& error);

    // Why a lattice node holds no probe.
    enum class GiNodeRejection : std::uint8_t
    {
        None,
        Excluded,   // Outside every include volume, or inside an exclude volume.
        Solid,      // Root BSP solid (or void).
        Unknown,    // Root BSP undecidable or an invalid query.
        Clearance,  // A primary collision face within `clearance`.
        WorldModel  // Inside a door, destructible or unbound world model (its own BSP, at the authored pose).
    };
    const char* gi_node_rejection_name(GiNodeRejection);

    // The world queries placement needs, built once per world. Keeps references to the world and
    // collision: both must outlive it and stay unchanged.
    class GiPlacementScene
    {
      public:
        bool build(const World&, const CollisionWorld&, std::string& error);
        // Validity of a probe point: root BSP, clearance, then world models (containers and sky
        // never block). Volumes are not applied here.
        GiNodeRejection classify(DVector point, float clearance) const;
        // The free axis-aligned box around a point: six rays against the primary collision faces,
        // shortened by closed doors (Mover world models, by their bounds) and limited to the world
        // bounds. `open` is set when a ray reached the bounds. False on an invalid query.
        bool fit_room(DVector point, GiBox& box, bool& open) const;
        // A segment between two points meets no primary collision face.
        bool visible(DVector from, DVector to) const;
        const GiBox& bounds() const
        {
            return bounds_;
        }

      private:
        const World* world_ = nullptr;
        const CollisionWorld* collision_ = nullptr;
        std::vector<WorldModelBounds> model_bounds_;
        std::vector<std::uint32_t> blockers_, doors_;
        GiBox bounds_;
    };

    struct GiLightProbe
    {
        std::uint32_t node = 0; // x + nx (y + ny z).
        DVector position{};
        bool relocated = false;
    };
    struct GiRejectedNode
    {
        DVector position{};
        GiNodeRejection reason = GiNodeRejection::None;
    };
    struct GiLightLattice
    {
        std::uint32_t spacing = 0;
        DVector origin{};
        std::array<std::uint32_t, 3> dims{};
        std::vector<GiLightProbe> probes; // Ascending node order.
        DVector node_position(std::uint32_t node) const;
    };
    struct GiPlacementStats
    {
        // Final lattice: every node falls in exactly one of these.
        std::uint64_t nodes = 0, probes = 0, relocated = 0;
        std::uint64_t excluded = 0, solid = 0, unknown = 0, clearance = 0, world_model = 0;
        // Spacings tried before the final one, each with the probe count that exceeded the budget
        // (counting stops at budget + 1).
        std::vector<std::pair<std::uint32_t, std::uint64_t>> over_budget;
        // Reflection placement.
        std::uint64_t room_candidates = 0, small_rooms = 0, overlapping = 0, capped = 0;
        std::uint32_t manual = 0, automatic = 0, open = 0;
    };
    // The light-probe lattice: nodes on multiples of the spacing over the primary bounds, kept when
    // valid, otherwise relocated to the nearest valid offset within 3/8 of the spacing (a node in empty
    // space only to a point it sees). Over the budget, the spacing grows along a fixed ladder
    // (x1.25 ... x4). `rejected` receives the Unknown, Clearance and WorldModel rejections.
    bool resolve_light_probes(const GiPlacementScene&, const GiPlacement&, GiLightLattice&, GiPlacementStats&,
                              std::vector<GiRejectedNode>* rejected, std::string& error);
    // Replaces the automatic reflection probes: rooms fitted around lattice probes, enclosed rooms
    // first, larger first, skipping those overlapping an accepted probe by more than half, up to the
    // cap with manual probes kept first.
    bool place_reflection_probes(const GiPlacementScene&, const GiLightLattice&, GiPlacement&, GiPlacementStats&,
                                 std::string& error);
    // A manual probe fitted to the room around a point (F8 "Add at camera" / "Fit to room").
    bool fit_reflection_probe(const GiPlacementScene&, DVector capture, GiReflectionProbe&, std::string& error);

    // The PLAC section of an .ob2gi file: the resolved lattice and reflection probes a bake used.
    bool encode_gi_placement_section(const GiLightLattice&, std::span<const GiReflectionProbe>,
                                     std::vector<std::uint8_t>& payload, std::string& error);
    bool decode_gi_placement_section(std::span<const std::uint8_t>, GiLightLattice&, std::vector<GiReflectionProbe>&,
                                     std::string& error);
} // namespace OpenBlood2
