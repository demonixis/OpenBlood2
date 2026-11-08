#pragma once
#include "engine/gi_probes.h"
#include "engine/model_view.h"
#include "engine/world_lights.h"
#include "engine/world_shade.h"
#include <unordered_map>

namespace OpenBlood2
{
    class GameSession;
    class ClientWorld;
    // Counters of the most recent apply(); world values are 0-255 like the original console.
    struct ModelLightingStats
    {
        std::size_t models = 0, sampled = 0, reused = 0, unlit = 0, flat = 0, sources = 0, occluded = 0;
        // Light probes (GI lot G9): lit models given a probe sample, samples taken and reused, models
        // with no valid probe around them (original light), pseudo-lights in the top four, and the
        // CPU time of the probe work.
        std::size_t probe_lit = 0, probe_sampled = 0, probe_reused = 0, probe_missing = 0, probe_sources = 0;
        double probe_ms = 0;
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
    // Light probes for model light (GI lot G9, decision 3). With a field, every lit model takes the
    // validity-weighted trilinear sample at its center (cached per object until it changes cell or
    // moves farther than gi_probe_limits::reuse_distance) and gi_probe_model_light with scale
    // k * intensity / 2 and floor f: the ambient term replaces AmbientLight, and the dominant-direction
    // pseudo-light competes in the top four (a static light of equal strength ranks first). A
    // displaced static light joins the remainder as before; a pseudo-light outside the top four, or
    // on a model without Gouraud shading, adds a quarter of its colour (its mean over directions).
    // No valid probe around a model, or no field: the original light, unchanged.
    struct ModelProbes
    {
        const GiProbeField* field = nullptr;
        float k = gi_default_k, f = gi_default_f, intensity = 1;
    };
    class ModelLighting
    {
      public:
        ModelProbes probes; // Set by the presenter before apply().
        bool build(const WorldMesh&, const World&, std::string& error);
        void clear();
        bool apply(GameSession&, const WorldMesh&, std::span<const WorldDraw> world_draws, ModelView&,
                   std::string& error);
        bool apply(const ClientWorld&, DVector eye, bool night_goggles, const WorldMesh&, std::span<const WorldDraw>,
                   ModelView&, std::string& error);
        ModelLightingStats stats;

      private:
        struct Sample
        {
            DVector center{}, min{}, max{}; // Bounds of the center and every tested light.
            ModelLight light;               // Directional sources and their summed remainder.
            DVector remainder{};
            // Ranking of the sources: max channel and the value at the center (for a displacement).
            std::array<float, ModelLight::source_limit> strength{};
            std::array<DVector, ModelLight::source_limit> value{};
        };
        struct ProbeSample
        {
            DVector center{};
            std::array<std::uint32_t, 3> cell{};
            bool valid = false;
            std::array<float, 27> sh{};
        };
        struct Pose
        {
            bool drawn = false;
            DVector position{};
            DRotation rotation{};
        };
        void sample(DVector center, std::span<const WorldDraw>, Sample&);
        std::uint64_t world_epoch_ = 0;
        WorldShade shade_;
        std::vector<StaticLight> lights_;
        DVector ambient_{};
        std::unordered_map<std::uint64_t, Sample> samples_, next_;
        std::unordered_map<std::uint64_t, ProbeSample> probe_samples_, probe_next_;
        const GiProbeField* probe_field_ = nullptr; // The field probe_samples_ belong to.
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
