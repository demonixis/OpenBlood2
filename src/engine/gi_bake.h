#pragma once
#include "engine/gi_bake_scene.h"
#include "engine/gi_direct.h"
#include "engine/gi_file.h"
#include "engine/gi_lightmap.h"
#include "engine/gi_placement.h"
#include "engine/gi_probes.h"
#include "engine/gi_reflections.h"
#include <atomic>
#include <filesystem>
#include <functional>
#include <span>
#include <string>
#include <string_view>

// One world's bake (gi_contracts.md, lots G5, G7, G9 and GD2): settings, placement, scene, a traced
// probe survey, the sky, the re-baked direct light when asked, the indirect light of every lightmap
// texel and vertex colour, the SH light probes and the .ob2gi file (INFO, PLAC, LMAP, DIR, VTX, PROB).
namespace OpenBlood2
{
    enum class GiQuality : std::uint8_t
    {
        Preview, // 1 bounce, 64 rays.
        Final,   // 2 bounces, 256 rays.
        Custom,  // --gi-bounces 1-4, --gi-rays 16-4096, --gi-sky 0-4 and the lot-GQ1 settings.
        Ultra    // Lot GQ1: 3 bounces, 512 rays, 4 texel samples, denoise 1, soft shadows 8/16,
                 // reflection strata 4 and 256 GGX samples.
    };
    namespace gi_bake_settings_limits
    {
        constexpr std::uint32_t min_bounces = 1, max_bounces = 4, min_rays = 16, max_rays = 4096;
        constexpr std::uint32_t max_sky = 400; // Hundredths.
        constexpr unsigned max_jobs = 256;
        // Lot GQ1.
        constexpr std::uint32_t max_texel_grid = 4; // Texel samples: 1, 4, 9 or 16 (an n x n grid).
        constexpr std::uint32_t max_denoise = 4;    // Filter radius in texels.
        constexpr std::uint32_t max_shadow_radius = 64, max_shadow_rays = 64, default_shadow_rays = 16;
        constexpr std::uint32_t min_reflection_strata = 2, max_reflection_strata = 8;
        constexpr std::uint32_t min_reflection_samples = 32, max_reflection_samples = 1024;
        constexpr std::uint32_t max_emissive = 400; // Lot GE1, hundredths.
        constexpr std::uint32_t default_emissive = 100; // Gate H (2026-11-08): emission on at scale 1.
    } // namespace gi_bake_settings_limits
    // The direct light the bake composes and bounces on lightmapped faces (lot GD2, decision D17);
    // re-baked by default since Gate G (2026-11-07).
    enum class GiDirectSource : std::uint8_t
    {
        Original, // D = O - A_eff, the stored 1998 texel without its ambient fill.
        Rebaked   // D', the re-baked direct light (gi_direct.h), stored in the DIR section.
    };
    // Every field defaults to the behaviour of earlier lots (decision D21): settings with these values
    // keep the bytes and the settings text of earlier files. The default request (no preset, no
    // emission scale) is Ultra with emission at scale 1 since Gate H (make_gi_bake_settings).
    struct GiBakeSettings
    {
        GiQuality quality = GiQuality::Final;
        std::uint32_t bounces = 2, rays = 256;
        std::uint32_t sky = 100; // Decision D9's sky scale, in hundredths.
        GiDirectSource direct = GiDirectSource::Rebaked;
        // Lot GQ1. texel_grid: sample points per texel axis (the texel's rays split between the n x n
        // points, decision D22). denoise: filter radius in texels (0: none). shadow_radius: the disk
        // radius of a ClipLight light in world units (0: hard shadows) and shadow_rays its rays.
        // reflection_strata: capture rays per cube texel axis; reflection_samples: GGX samples per
        // texel (0: 64, or 32 above a 128 face).
        std::uint32_t texel_grid = 1, denoise = 0, shadow_radius = 0, shadow_rays = 0;
        std::uint32_t reflection_strata = 2, reflection_samples = 0;
        std::uint32_t emissive = 0; // Lot GE1: manifest emission scale in hundredths (0: not baked).
    };
    // What the command line gives; zero or -1 where not given. No preset means Ultra and no emission
    // scale means default_emissive (Gate H); --gi-emissive 0 turns emission off.
    struct GiBakeArguments
    {
        std::string_view quality;
        std::uint32_t bounces = 0, rays = 0;
        std::int32_t sky = -1;
        std::uint32_t texel_samples = 0;
        std::int32_t denoise = -1;
        std::uint32_t shadow_radius = 0, shadow_rays = 0, reflection_strata = 0, reflection_samples = 0;
        std::int32_t emissive = -1;
    };
    const char* gi_quality_name(GiQuality);
    const char* gi_direct_source_name(GiDirectSource); // "original", "rebaked".
    bool gi_direct_source(std::string_view name, GiDirectSource&);
    // A preset name ("preview", "final", "ultra" or "custom", empty for Ultra); the quality settings
    // only with "custom", where they default to Final's. The emission scale goes with any preset.
    bool make_gi_bake_settings(const GiBakeArguments&, GiBakeSettings&, std::string& error);
    // The earlier lots' form: the same without emission (scale 0).
    bool make_gi_bake_settings(std::string_view quality, std::uint32_t bounces, std::uint32_t rays, std::int32_t sky,
                               GiBakeSettings&, std::string& error);
    // The canonical INFO text; the thread count is never part of it. A re-baked direct source adds
    // the line "direct rebaked"; original bakes keep the text (and the bytes) of earlier lots. Each
    // lot-GQ1/GE1 setting away from its default adds one line before it ("texel-samples N",
    // "denoise R", "shadows R N", "reflection-strata N", "reflection-samples N", "emissive S M" with
    // M the first 16 hex digits of the built-in manifest's SHA-256).
    std::string gi_settings_text(const GiBakeSettings&);
    // The built-in surface manifest's digest prefix the emissive line names.
    std::string gi_manifest_digest_prefix();

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
        GiSkySetup sky_setup;
        GiSkyReport sky;
        GiLightmapReport lightmaps;
        // The re-baked direct pass (lot GD2); zero for an original bake.
        GiDirectLightReport direct_lights;
        std::size_t direct_triangles = 0, direct_kept = 0, direct_saturated = 0;
        std::uint64_t direct_tests = 0;
        double direct_ms = 0;
        GiProbeReport light_probes;
        GiReflectionReport reflection_probes; // Lot G11.
        GiEmissionReport emission;            // Lot GE1; zero without --gi-emissive.
        std::uint64_t bytes = 0;
        double ms = 0;
    };
    const char* gi_bake_status_name(GiBakeResult::Status);
    // Nothing is written unless the bake completes; a cancelled or failed bake keeps any previous file.
    GiBakeResult bake_gi_world(const Resources&, const GiBakeRequest&);
} // namespace OpenBlood2
