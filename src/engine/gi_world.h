#pragma once
#include "engine/collision.h"
#include "engine/resources.h"
#include "engine/sha256.h"
#include "engine/world.h"
#include <array>
#include <span>
#include <string>
#include <vector>

// World queries shared by the GI baker and the presentation mesh (gi_contracts.md, lot G1).
// Read-only over the DAT; nothing here creates objects, touches the GPU or changes a frame.
namespace OpenBlood2
{
    // Texel layout of one stored polygon lightmap. Texel (sx, sy) is centered where
    // dot(lightmap_p, x) = origin[0] + 20 sx and dot(lightmap_q, x) = origin[1] + 20 sy on the
    // polygon plane; the axes are not unit length.
    struct LightmapFrame
    {
        static constexpr double grid = 20;
        DVector p{}, q{}, normal{};
        double distance = 0; // Plane: dot(normal, x) = distance.
        std::array<double, 2> origin{};
        unsigned width = 0, height = 0;
        // Axes whose origin no grid-line candidate fitted (origin = snapped minimum).
        unsigned unfitted = 0;
        // Continuous texel coordinates of a point: texel centers sit at i + 0.5.
        std::array<double, 2> coordinate(DVector point) const;
        // World position of a texel center (fractional texels allowed) on the polygon plane.
        // False when the axes and normal do not span space.
        bool position(double sx, double sy, DVector& out) const;
    };
    // The frame WorldMesh::build uses for a SURF_LIGHTMAP polygon: origin per axis from the first
    // `count` (base ring) points with lightmap_origin. False when the polygon stores no lightmap
    // or references invalid data.
    bool lightmap_frame(const WorldModel&, const Polygon&, std::size_t count, LightmapFrame&);

    // Root-BSP point classification: the side sentinel the original descent ends on.
    // Empty is BspNode::inside, Solid is BspNode::outside; Unknown when the model has no tree
    // or the descent does not terminate.
    enum class PointContents
    {
        Empty,
        Solid,
        Unknown
    };
    PointContents point_contents(const WorldModel&, DVector point);

    // What a world model is, from the DAT objects of the classes that create OT_WORLDMODEL or
    // OT_CONTAINER objects and name it (case-insensitive, first model in file order wins like
    // object creation). Objects of other classes never bind a model, even when their Name matches.
    // WorldModelDebris fragments past the exact Name stay Unbound here.
    enum class WorldModelRole
    {
        Primary,      // Model 0, the root BSP.
        Sky,          // DemoSkyWorldModel.
        Container,    // VolumeBrush family and the built-in Container.
        Mover,        // Door, RotatingDoor, RotatingBrush.
        Destructible, // CDestructableBrush, WorldModelDebris.
        Unbound       // Named by no binding object (or shadowed by an earlier same-named model).
    };
    const char* world_model_role_name(WorldModelRole);
    struct WorldModelRoleInfo
    {
        WorldModelRole role = WorldModelRole::Unbound;
        std::string object_class; // First binding object's class; empty when none.
        std::size_t objects = 0;  // Objects binding this model.
    };
    std::vector<WorldModelRoleInfo> world_model_roles(const World&);

    // Axis-aligned bounds of each world model's points (DAT coordinates, the authored pose);
    // empty bounds (min > max) for a model without points.
    struct WorldModelBounds
    {
        DVector min{1, 1, 1}, max{-1, -1, -1};
    };
    std::vector<WorldModelBounds> world_model_bounds(const World&);
    // First non-primary, non-sky model whose bounds, grown by `margin`, contain the point; -1 when
    // none. World models are not carved into the root BSP, so a probe there would sit inside a
    // door, crate or volume. Bounds are conservative: a point in a concave model's hull counts.
    std::int32_t world_model_containing(std::span<const WorldModelBounds>, std::span<const WorldModelRoleInfo>,
                                        DVector point, float margin);
    // A cube of half-size `half` around the point overlaps no primary collision triangle
    // (zero-length sweep, StartOverlap::Block). False when the query is invalid.
    bool point_clear(const CollisionWorld&, DVector point, float half, bool& clear);

    // "AmbientLight r g b" from the world info string, matching the key case-insensitively
    // (8 retail worlds spell it "ambientlight"). The original renderer's case-sensitive reader
    // (world_info_ambient) is unchanged.
    bool world_info_ambient_any_case(std::string_view info, DVector& ambient);
    // Per-world measurement of the ambient fill the preprocessor added: the info value and the
    // darkest stored lightmap texel / baked vertex color per channel (0-255, RGB565 expanded like
    // WorldMesh), over lit faces (not invisible, sky or SURF_BRIGHT).
    struct AmbientAudit
    {
        bool has_ambient = false, case_mismatch = false; // case_mismatch: only the any-case read finds it.
        DVector ambient{};
        std::array<int, 3> darkest_texel{255, 255, 255}, darkest_vertex{255, 255, 255};
        std::size_t texels = 0, vertices = 0;
        // Samples with any channel below the ambient value (texels: below its RGB565 quantization).
        std::size_t texels_below = 0, vertices_below = 0;
    };
    AmbientAudit audit_ambient(const World&);

    // SHA-256 of the world DAT bytes, as the save fingerprint computes it: the identity of the
    // data a bake is derived from.
    bool world_dat_digest(const Resources&, std::string_view world_path, Sha256Digest&, std::string& error);
} // namespace OpenBlood2
