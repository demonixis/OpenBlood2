#pragma once
#include "engine/world.h"

namespace OpenBlood2
{
    // Preprocessor light objects that also light models (LightObjects): Light, DirLight
    // (a cone despite its name) and ObjectLight. The game never creates them (CF_NORUNTIME);
    // their DAT properties are read directly.
    struct StaticLight
    {
        DVector position{}, color{}; // Color: 0-1 RGB already scaled by BrightScale.
        float radius = 0;
        DVector direction{0, 0, 1}; // Cone axis: the Rotation forward vector.
        float cos_half = -2;        // Cosine of half the FOV; below -1 for point lights.
        bool clip = false;          // ClipLight: occluded by world geometry.
    };
    struct StaticLightStats
    {
        std::size_t lights = 0, without_objects = 0, outer = 0, invalid = 0;
    };
    // Reads every Light/DirLight/ObjectLight with LightObjects. Invalid property values are
    // counted and skipped; nonzero OuterColor is counted (no retail world uses one).
    bool world_static_lights(const World&, std::vector<StaticLight>&, StaticLightStats&, std::string& error);
    // The original light term, matching DAT vertex colors exactly on retail worlds:
    // color * (1 - d/R) * cone, cone = (cos - cos_half) / (1 - cos_half); no N.L, no occlusion.
    // Zero outside the radius or cone.
    DVector static_light_at(const StaticLight&, DVector point);
} // namespace OpenBlood2
