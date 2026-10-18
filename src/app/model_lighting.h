#pragma once
#include "engine/model_view.h"
#include "engine/world_shade.h"
#include <unordered_map>

namespace OpenBlood2
{
    class GameSession;
    // Counters of the most recent apply(); world values are 0-255 like the original console.
    struct ModelLightingStats
    {
        std::size_t models = 0, sampled = 0, reused = 0, ambient = 0, unlit = 0;
        DVector world_ambient{}, model_add{}, model_dir_add{}; // ModelDirAdd is read, not applied.
        WorldShadeStats shade;
    };
    // Replacement model lighting from the baked world light: each GPU-skinned model reads the
    // interpolated per-vertex light of the first textured opaque face straight below its
    // bounds center (view models: below the camera). The world AmbientLight is the fallback
    // when nothing is below. The original WorldProperties ModelAdd and the object's additive
    // color (SetObjectColor) are added; FLAG_NOLIGHT models use their object color alone.
    // Samples are reused while the model center and the world ranges its segment crosses do
    // not move. Presentation only: no simulation state or service is read or changed.
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
            DVector center{}, light{}, min{}, max{}; // Segment bounds up to the hit.
        };
        struct Pose
        {
            bool drawn = false;
            DVector position{};
            DRotation rotation{};
        };
        void sample(DVector center, std::span<const WorldDraw>, Sample&);
        WorldShade shade_;
        DVector ambient_{};
        float floor_ = 0; // Lowest traced height: the world's minimum Y.
        std::unordered_map<std::uint64_t, Sample> samples_, next_;
        std::vector<Pose> poses_;
        std::vector<std::array<DVector, 2>> moved_;
    };
    // Reads "AmbientLight r g b" from the DAT world info string (0-255 components).
    bool world_info_ambient(std::string_view info, DVector& ambient);
} // namespace OpenBlood2
