#pragma once
#include "engine/model_view.h"
#include "engine/world_lights.h"
#include "engine/world_shade.h"
#include <unordered_map>

namespace OpenBlood2
{
    class GameSession;
    // Counters of the most recent apply(); world values are 0-255 like the original console.
    struct ModelLightingStats
    {
        std::size_t models = 0, sampled = 0, reused = 0, unlit = 0, flat = 0, sources = 0, occluded = 0;
        DVector world_ambient{}, model_add{}, model_dir_add{}; // ModelDirAdd is read, not applied.
        StaticLightStats lights;
        WorldShadeStats shade;
    };
    // Model light from the world's static light objects (Light/DirLight/ObjectLight with
    // LightObjects), using the original light term verified against baked vertex colors.
    // At each model's bounds center (view models: the camera) every light in range and cone is
    // evaluated; ClipLight lights blocked by textured opaque world faces are skipped. The four
    // strongest light each vertex with N.L (FLAG_MODELGOURAUDSHADE models); the rest join the
    // ambient: AmbientLight + WorldProperties ModelAdd + the object's additive SetObjectColor.
    // Models without FLAG_MODELGOURAUDSHADE take every contribution as ambient; FLAG_NOLIGHT
    // models use their object color alone. Results are reused while the center and the world
    // ranges crossing its light segments do not move. Presentation only.
    class ModelLighting
    {
      public:
        bool build(const WorldMesh&, const World&, std::string& error);
        void clear();
        bool apply(const GameSession&, const WorldMesh&, std::span<const WorldDraw> world_draws, ModelView&,
                   std::string& error);
        ModelLightingStats stats;

      private:
        struct Sample
        {
            DVector center{}, min{}, max{}; // Bounds of the center and every tested light.
            ModelLight light;               // Directional sources and their summed remainder.
            DVector remainder{};
        };
        struct Pose
        {
            bool drawn = false;
            DVector position{};
            DRotation rotation{};
        };
        void sample(DVector center, std::span<const WorldDraw>, Sample&);
        WorldShade shade_;
        std::vector<StaticLight> lights_;
        DVector ambient_{};
        std::unordered_map<std::uint64_t, Sample> samples_, next_;
        std::vector<Pose> poses_;
        std::vector<std::array<DVector, 2>> moved_;
        struct Ranked
        {
            float strength = 0;
            std::size_t light = 0;
            DVector value{};
        };
        std::vector<Ranked> ranked_;
    };
    // Reads "AmbientLight r g b" from the DAT world info string (0-255 components).
    bool world_info_ambient(std::string_view info, DVector& ambient);
} // namespace OpenBlood2
