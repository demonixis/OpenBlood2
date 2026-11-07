#pragma once
#include "engine/gi_bake_scene.h"
#include "engine/sky.h"
#include <array>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

// The sky a GI bake sees (gi_contracts.md lot G7, decision D9): the sky world models drawn as the
// renderer draws them, sampled once per world into a cube map.
namespace OpenBlood2
{
    namespace gi_sky_limits
    {
        constexpr std::size_t slots = 30;           // Original MAX_SKYOBJECTS.
        constexpr unsigned face_size = 64;          // Cube texels per face edge.
        constexpr std::size_t max_triangles = 8192; // Painted sky triangles.
    } // namespace gi_sky_limits

    // The sky the DAT objects set up once their creation, initial updates and first updates ran:
    // - DemoSkyWorldModel (file order) adds its own model to slot Index at its initial update;
    // - SkyPointer (file order, at its first update, so after every initial update) adds the model
    //   of the first object named SkyObjectName to slot Index, replacing the slot's holder;
    // - the last of either class with three nonzero SkyDims defines the sky box and view box.
    // Objects the original precreate refuses (Index outside 0-29, negative or non-finite SkyDims or
    // InnerPercent) add nothing; neither does an object whose sky definition is rejected.
    // SkyFog comes from the last WorldProperties object: it applies only while FogEnable gives a
    // valid fog colour, like the client's ResetGlobalFog.
    struct GiSkyFog
    {
        bool enabled = false;
        DVector color{};      // 0-1: the truncated FogColor channels over 255.
        float start = 0, end = 0; // Truncated SkyFogNearZ and SkyFogFarZ (end > start).
    };
    struct GiSkySetup
    {
        SkyDefinition definition; // The engine default until an object defines one.
        bool defined = false;
        std::array<std::int32_t, gi_sky_limits::slots> slots; // Model index per slot; -1 when empty.
        std::array<bool, gi_sky_limits::slots> visible{};     // FLAG_VISIBLE of the slot holder.
        std::vector<std::uint32_t> models; // Every model drawn as sky (FLAG_SKYOBJECT), sorted.
        // SkyPointers whose target names no object, or an object that binds no world model (a
        // sprite, for example): such targets are not in the bake.
        std::size_t pointers = 0, unresolved_pointers = 0;
        GiSkyFog fog;
        GiSkySetup()
        {
            slots.fill(-1);
        }
    };
    GiSkySetup gi_sky_setup(const World&);

    struct GiSkyReport
    {
        std::size_t slots = 0, triangles = 0, translucent = 0; // Painted slots and their triangles.
        double covered = 0; // Solid-angle fraction of the cube the sky models draw over.
        DVector eye{}, average{};
        double ms = 0;
    };
    // The cube map, painted from the sky eye of the world centre (the view-box centre) like the sky
    // pass: slots in order, back faces culled, no depth test, each range's triangles in submission
    // order (opaque faces replace, translucent faces blend with their texel alpha). A texel holds
    // the drawn colour (albedo times drawn light; sky-portal and unresolved faces show their vertex
    // colour like the renderer), fogged by SkyFog over the radial distance from the eye (the sky
    // pass uses the view depth, which a cube shared by every view cannot follow). Directions no model
    // covers take the solid-angle average of the covered colour (black when the sky draws nothing).
    // 2x2 samples per texel. Like the sky pass, an invalid definition fails only when a visible
    // object holds a slot.
    class GiSky
    {
      public:
        static constexpr unsigned size = gi_sky_limits::face_size;
        // `scale` (D9's sky scale) multiplies radiance(); jobs as run_gi_jobs.
        bool build(const World&, const GiBakeScene&, const GiSkySetup&, float scale, unsigned jobs, GiSkyReport&,
                   std::string& error);
        // Bilinear within the direction's cube face (edges clamped), times the scale.
        DVector radiance(DVector direction) const;
        // The same without the scale: what the sky pass shows.
        DVector displayed(DVector direction) const;
        // One colour in every direction (the analytic tests' uniform sky).
        void fill(DVector displayed, float scale);
        float scale() const
        {
            return scale_;
        }
        std::span<const float> texels() const
        {
            return texels_;
        }

      private:
        std::vector<float> texels_; // 6 faces x size x size x RGB (+X, -X, +Y, -Y, +Z, -Z).
        float scale_ = 1;
    };
    // Cube texel direction (unnormalized) for face f, texel coordinates (x, y) in [0, size).
    DVector gi_cube_direction(unsigned face, float x, float y);
} // namespace OpenBlood2
