#pragma once
#include "engine/objects.h"
#include <array>

namespace OpenBlood2
{
    struct WorldProperty
    {
        std::string name;
        std::uint8_t type = 0;
        std::uint32_t flags = 0;
        PropertyValue value;
    };
    struct WorldObject
    {
        std::string class_name;
        std::vector<WorldProperty> properties;
        const PropertyValue* find(std::string_view name) const;
        // A non-finite authored Pos, retained without exposing it as a usable pose: baked-light
        // metadata for editor lights, an uncreated runtime entry otherwise (session).
        bool invalid_editor_position = false;
    };
    struct Plane
    {
        DVector normal;
        float distance = 0;
    };
    struct Surface
    {
        DVector origin, u, v;
        // Lightmap axes: a texel every 20 units of dot(lightmap_p/q, point). Not unit length;
        // later LithTech derives them from the face normal and a principal plane.
        DVector lightmap_p, lightmap_q;
        std::uint16_t texture = 0;
        std::uint32_t plane = 0, flags = 0;
        std::uint16_t texture_flags = 0; // Serialized v56 surface material, not render flags.
        // First byte after the flags: retail Server.dll leaves nodes whose polygon has a nonzero
        // value out of the visibility tree its leaf lists are encoded against.
        std::uint8_t visibility_skip = 0;
        bool has_effect = false;
        std::string effect_name, effect_parameters; // Raw DAT metadata, including unsupported effects.
    };
    struct Polygon
    {
        std::uint32_t surface = 0;
        std::vector<std::uint16_t> points;
        // v56 base ring size; sentinel for generated geometry. Extras remain in points.
        std::uint16_t base_points = UINT16_MAX;
        // Baked per-vertex light (0-255 RGB, world ambient included), parallel to points.
        // Empty for generated geometry, which has no baked lighting.
        std::vector<std::array<std::uint8_t, 3>> colors{};
        // Grid-rounded lightmap extents along lightmap_p/q, from the polygon prefix.
        std::array<std::uint16_t, 2> lightmap_extent{};
        // Stored lightmap (RGB565 texels at WorldModel::lightmap_texels[lightmap_offset],
        // row-major along lightmap_p); 0x0 when the polygon stores none.
        std::uint8_t lightmap_width = 0, lightmap_height = 0;
        std::uint32_t lightmap_offset = 0;
    };
    // v56 BSP node: plane, splitting polygon, owned leaf and the two sides (retail sides[0] is the
    // back half-space, chosen when the signed distance is below -0.0001).
    struct BspNode
    {
        static constexpr std::int32_t inside = -1, outside = -2;
        std::uint32_t plane = 0, polygon = 0;
        std::uint16_t leaf = UINT16_MAX; // UINT16_MAX: no leaf.
        std::array<std::int32_t, 2> sides{inside, inside};
    };
    // One compressed leaf visibility list (a bitstream over the retail visibility tree).
    // The first list of a leaf is unconditional; later ones apply only through an open portal.
    struct LeafVisibilityList
    {
        std::uint16_t portal = UINT16_MAX;
        std::uint32_t offset = 0, size = 0; // Into WorldModel::visibility.
    };
    struct BspLeaf
    {
        // Lists after resolving the original "uses the lists of leaf N" record at load time:
        // a reference to a later (or the same) leaf copies its still-empty lists, as retail does.
        std::uint32_t first_list = 0, lists = 0;
        std::uint16_t shared = UINT16_MAX; // Raw share record; UINT16_MAX when the leaf owns its lists.
    };
    struct WorldModel
    {
        std::string name;
        std::uint32_t flags = 0, nodes = 0, leaves = 0, portals = 0;
        DVector min, max, translation;
        std::vector<DVector> points;
        std::vector<Plane> planes;
        std::vector<Surface> surfaces;
        std::vector<Polygon> polygons;
        std::vector<std::string> textures;
        // Trailing lightmap block: one entry per SURF_LIGHTMAP polygon, located and validated
        // against the polygon extents. lightmaps is false when no unique block validates.
        bool lightmaps = false;
        std::vector<std::uint16_t> lightmap_texels;
        std::size_t opaque_bytes = 0; // Undecoded tail before the lightmap block (after the root node).
        std::vector<BspNode> bsp_nodes;
        std::int32_t root_node = BspNode::outside;
        std::vector<BspLeaf> bsp_leaves;
        std::vector<LeafVisibilityList> visibility_lists;
        std::vector<std::uint8_t> visibility; // Concatenated raw list bytes.
    };
    struct World
    {
        std::uint32_t version = 0;
        std::string info;
        DVector min, max;
        std::vector<WorldObject> objects;
        std::vector<WorldModel> models;
        std::size_t opaque_bytes = 0;
    };
    // Strict v56 structural reader. Output is empty on failure. Coordinates remain
    // in original units; no mirror, scene transform or guessed property scanning.
    bool read_world(std::span<const std::uint8_t> bytes, World& world, std::string& error);
    // The DAT v56 header's world-info string alone, from a prefix of the file (the original
    // GetWorldInfoString, which reads it without loading the world).
    bool read_world_info(std::span<const std::uint8_t> prefix, std::string& info, std::string& error);
    // The engine's Parse over a world-info string: commands end at ';', tokens are separated by
    // whitespace and a quoted token keeps its spaces.
    std::vector<std::vector<std::string>> world_info_commands(std::string_view info);
} // namespace OpenBlood2
