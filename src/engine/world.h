#pragma once
#include "engine/objects.h"

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
    };
    struct Plane
    {
        DVector normal;
        float distance = 0;
    };
    struct Surface
    {
        DVector origin, u, v;
        std::uint16_t texture = 0;
        std::uint32_t plane = 0, flags = 0;
        std::uint16_t texture_flags = 0; // Serialized v56 surface material, not render flags.
        bool has_effect = false;
        std::string effect_name, effect_parameters; // Raw DAT metadata, including unsupported effects.
    };
    struct Polygon
    {
        std::uint32_t surface = 0;
        std::vector<std::uint16_t> points;
        // v56 base ring size; sentinel for generated geometry. Extras remain in points.
        std::uint16_t base_points = UINT16_MAX;
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
        std::size_t opaque_bytes = 0; // Bounded lightmap/visibility tail, not decoded.
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
} // namespace OpenBlood2
