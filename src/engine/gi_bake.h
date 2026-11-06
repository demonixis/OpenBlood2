#pragma once
#include "engine/gi_bake_scene.h"
#include "engine/gi_file.h"
#include "engine/gi_placement.h"
#include <atomic>
#include <filesystem>
#include <functional>
#include <span>
#include <string>
#include <string_view>

// One world's bake (gi_contracts.md, lot G5): settings, placement, scene, a traced probe survey and
// the .ob2gi file. Lot G5 writes INFO and PLAC only; the lighting sections come with G7 and G9.
namespace OpenBlood2
{
    enum class GiQuality : std::uint8_t
    {
        Preview, // 1 bounce, 64 rays.
        Final,   // 2 bounces, 256 rays.
        Custom   // --gi-bounces 1-3, --gi-rays 16-4096.
    };
    namespace gi_bake_settings_limits
    {
        constexpr std::uint32_t min_bounces = 1, max_bounces = 3, min_rays = 16, max_rays = 4096;
        constexpr unsigned max_jobs = 256;
    } // namespace gi_bake_settings_limits
    struct GiBakeSettings
    {
        GiQuality quality = GiQuality::Final;
        std::uint32_t bounces = 2, rays = 256;
    };
    const char* gi_quality_name(GiQuality);
    // A preset name; bounces and rays (0 = not given) only with "custom", where both default to Final's.
    bool make_gi_bake_settings(std::string_view quality, std::uint32_t bounces, std::uint32_t rays, GiBakeSettings&,
                               std::string& error);
    // The canonical INFO text; the thread count is never part of it.
    std::string gi_settings_text(const GiBakeSettings&);

    // Every light probe casts `rays` uniform directions, seeded by (probe index, ray index): what
    // the rays hit, a per-probe hash of every hit and a digest over all of them, identical for any
    // thread count.
    struct GiSurvey
    {
        std::uint64_t probes = 0, rays = 0;
        std::uint64_t front = 0, back = 0, sky = 0, miss = 0, layer_limit = 0, layers = 0;
        std::uint64_t inside = 0; // Probes where more than half the rays hit a back face.
        Sha256Digest digest{};
        GiTraceStats trace;
        unsigned threads = 0;
        double ms = 0;
        bool cancelled = false;
    };
    // A uniform unit direction for ray `index` of stream `stream` (a probe): the same bits on every
    // build and thread count.
    DVector gi_uniform_direction(std::uint64_t stream, std::uint64_t index);
    using GiProgress = std::function<void(std::string_view stage, std::uint64_t done, std::uint64_t total)>;
    // `progress` is called (serialized, increasing `done`) about every 5% of the probes.
    GiSurvey survey_gi_probes(const GiBakeScene&, std::span<const GiLightProbe>, std::uint32_t rays, unsigned jobs,
                              const std::atomic<bool>* cancel, const GiProgress& progress);

    struct GiBakeRequest
    {
        std::string world;    // Mounted world path (normalized resource path).
        GiBakeSettings settings;
        unsigned jobs = 0;    // 0: gi_default_jobs().
        std::filesystem::path out_dir, placement_dir; // Empty: user_directory()/gi, gi_placement_directory.
        bool if_stale = false; // Report Current, without tracing, when a Ready file with these settings exists.
        const std::atomic<bool>* cancel = nullptr;
        GiProgress progress;
    };
    struct GiBakeResult
    {
        enum class Status : std::uint8_t
        {
            Baked,
            Current,
            Failed,
            Cancelled
        };
        Status status = Status::Failed;
        // 0, 2 (arguments/data), 4 (placement, bake or write), 5 (world load), 12 (cancelled).
        int exit_code = 4;
        std::string error;
        std::filesystem::path file;
        GiPlacementStatus placement = GiPlacementStatus::Missing;
        std::size_t probes = 0, reflections = 0;
        GiSceneReport scene;
        GiSurvey survey;
        std::uint64_t bytes = 0;
        double ms = 0;
    };
    const char* gi_bake_status_name(GiBakeResult::Status);
    // Nothing is written unless the bake completes; a cancelled or failed bake keeps any previous file.
    GiBakeResult bake_gi_world(const Resources&, const GiBakeRequest&);
} // namespace OpenBlood2
