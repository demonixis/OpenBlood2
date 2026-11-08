#pragma once

#include "engine/multiplayer_rules.h"
#include <cstdint>
#include <filesystem>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace OpenBlood2
{

    struct Options
    {
        std::filesystem::path config_file = "blood2.local.ini";
        std::filesystem::path data_dir;
        std::filesystem::path gi_dir; // --gi-dir: first GI bake search root (gi_contracts.md lot G2).
        std::filesystem::path gi_placement_dir; // --gi-placement-dir: GI placement files (lot G4).
        std::vector<std::filesystem::path> overlays{}; // Explicit, ordered data-only REZ mods.
        std::filesystem::path capture_file;
        std::filesystem::path log_dir;
        std::string log_level = "info";
        bool crash_capture = true;
        std::uint32_t frame_limit = 0;
        std::string world = "01_SubwayA"; // Demo_01 with --demo unless --world is given.
        std::string connect_address, host_world, network_name = "Player";
        std::uint16_t network_port = 27888;
        std::uint8_t network_mode = 1, network_team = 0,
                     network_character = 0;                          // Bloodbath; original auto/blue/red preference.
        MultiplayerRules network_rules{MatchEnd::Frags, 25, 15, {}}; // NetStart host default: 15 minutes.
        bool demo = false;                                           // Original two-level demo layout (B2DEMO.REZ).
        bool nightmares = false; // Retail data plus the Nightmares expansion (BLOOD2NM.REZ).
        bool inspect_world = false;
        bool inspect_player = false;
        bool player_smoke = false, restore_smoke = false, keepalive_smoke = false;
        bool weapons_smoke = false;
        bool skeleton_smoke = false;
        bool movement_smoke = false, containers_smoke = false, doors_smoke = false, triggers_smoke = false,
             interaction_smoke = false, brushes_smoke = false, pickups_smoke = false, objectives_smoke = false,
             cameras_smoke = false, keyframes_smoke = false, worldpaths_smoke = false, lights_smoke = false,
             cinematics_smoke = false, random_sounds_smoke = false, world_sounds_smoke = false, sprites_smoke = false,
             scene_explosions_smoke = false;
        bool view_world = false, viewport_smoke = false;
        bool start_game = false;
        bool menu_smoke = false;
        bool demo_smoke = false; // Dedicated two-level SDL traversal, never changes normal play.
        bool explicit_config = false;
        bool help = false;
        bool xr_test = false;  // OpenXR first light (BLOOD2_XR builds); needs no game data.
        bool xr_world = false; // Direct world entry rendered in an OpenXR session (BLOOD2_XR builds).
        bool check_data = false;
        // GI tools (gi_contracts.md lots G2, G4): --list-worlds [--json], --gi-verify FILE,
        // --gi-place WORLD|single|all [--gi-reset] [--gi-dry-run] [--json].
        bool list_worlds = false, json = false, gi_reset = false, gi_dry_run = false;
        std::filesystem::path gi_verify;
        std::string gi_place;
        // --bake-gi WORLD [--gi-quality preview|final|ultra|custom] [--gi-bounces N] [--gi-rays N] [--gi-sky S]
        // [--gi-jobs N] [--gi-out DIR] [--gi-if-stale] (lots G5, G7) [--gi-direct original|rebaked] (lot
        // GD2); zero (sky: -1) and an empty direct source mean not given (the direct source then
        // defaults to rebaked).
        std::string bake_gi, gi_quality, gi_direct;
        std::uint32_t gi_bounces = 0, gi_rays = 0, gi_jobs = 0;
        std::int32_t gi_sky = -1; // Sky scale in hundredths (0-400).
        // Lot GQ1 (with --gi-quality custom): --gi-texel-samples 1|4|9|16, --gi-denoise 0-4,
        // --gi-shadow-radius 1-64, --gi-shadow-rays 1-64, --gi-reflection-strata 2-8,
        // --gi-reflection-samples 32-1024; lot GE1 (any preset): --gi-emissive S (0-4, hundredths).
        std::uint32_t gi_texel_samples = 0, gi_shadow_radius = 0, gi_shadow_rays = 0, gi_reflection_strata = 0,
                      gi_reflection_samples = 0;
        std::int32_t gi_denoise = -1, gi_emissive = -1;
        // --gi-preview WORLD [--gi-out DIR] [--gi-preview-dir DIR] [--gi-k K] [--gi-f F] [--gi-jobs N] (lot G7).
        std::string gi_preview;
        std::filesystem::path gi_preview_dir;
        // --gi-direct-audit WORLD|single|all [--gi-jobs N] [--gi-preview-dir DIR] (lot GD1): the re-baked
        // direct light against the stored lightmaps, as JSON; with --gi-preview-dir, Original | Re-baked
        // direct | Difference PNG views of each world.
        std::string gi_direct_audit;
        float gi_k = 2, gi_f = 1; // gi_default_k and gi_default_f (engine/gi_lightmap.h).
        bool gi_composition = false; // --gi-k or --gi-f given.
        // --gi (lot G8): baked GI for --bench and smoke runs (the game uses the Display setting and F8),
        // composed with --gi-k, --gi-f and --gi-view (0 final ... 5 validity, GiView order). Lot G9 adds
        // --gi-source texels|probes (the world's E) and --gi-probes P (model probe intensity 0-4, 0: off).
        bool gi = false, gi_view_given = false, gi_source_given = false, gi_probes_given = false;
        int gi_view = 0, gi_source = 0;
        float gi_probes = 1;
        // --gi-reflections off|low|high (lot G12): reflection probes for --bench and smoke runs, with or
        // without --gi (GiReflectionMode order; the game uses Options > Display REFLECTION PROBES).
        int gi_reflections = 0;
        bool gi_reflections_given = false;
        std::filesystem::path gi_out;
        bool gi_if_stale = false;
        bool inspector = true;
        bool gpu_smoke = false;
        bool world_smoke = false;
        // Headless offscreen presentation measurement; never opens a window.
        bool bench = false, bench_sweep = false;
        bool bench_audio = false;    // Bench renders the game mix offline (streamed reads as with a device).
        bool model_shadows = true;   // FLAG_SHADOW model shadows (original high-detail MaxModelShadows).
        bool pixel_lighting = false; // Model light per pixel (the frontend uses the Display setting).
        // Renderer for the bench and direct paths (the frontend uses the Display settings):
        // 0 original, 1 enhanced standard, 2 enhanced high; generated normal maps.
        int renderer = 0;
        bool normal_maps = false;
        // Post-processing for the same paths: anti-aliasing (0 off, 1 FXAA, 2 MSAA 2x, 3 MSAA 4x),
        // and with the enhanced renderer, bloom, the tonemap (0 off, 1 neutral, 2 filmic) and
        // ambient occlusion and screen-space reflections (0 off, 1 low, 2 high).
        int antialiasing = 0, tonemap = 1, ambient_occlusion = 2, reflections = 2;
        bool bloom = true;
        std::uint32_t bench_width = 1920, bench_height = 1080;
    };

    struct Config
    {
        std::filesystem::path data_dir;
        std::string source;
        bool demo = false; // Selects the demo archive set; never inferred from directory contents.
        // Selects the retail set plus BLOOD2NM.REZ (nightmares_plan.md); never inferred either.
        bool nightmares = false;
        std::vector<std::filesystem::path> overlays{};
        std::filesystem::path gi_dir{}; // --gi-dir, else gi_dir= (relative to the ini); empty when neither.
        // --gi-placement-dir, else gi_placement_dir= (relative to the ini); empty selects
        // user_directory()/gi/placement.
        std::filesystem::path gi_placement_dir{};
    };
    // Original first single-player level: ClientShellDLL sFirstLevel (_DEMO "Demo_01").
    inline const char* first_level(bool demo)
    {
        return demo ? "Demo_01" : "01_SubwayA";
    }

    struct ArchiveStatus
    {
        std::string name;
        bool required = false;
        bool present = false;
        std::uintmax_t bytes = 0;
    };

    struct DataStatus
    {
        std::vector<ArchiveStatus> archives;
        std::string readme_version;
    };

    std::string path_text(const std::filesystem::path& path);
    std::filesystem::path path_from_utf8(std::string_view text);
    bool parse_options(std::span<const std::string_view> args, Options& options, std::string& error);
    // `unconfigured` (when given) is set when the only failure is that no data directory is named.
    bool load_config(const Options& options, std::string_view environment_data_dir, Config& config, std::string& error,
                     bool* unconfigured = nullptr);
    // Checks file presence/readability only. Does not parse or mount an archive.
    bool inspect_data(const Config& config, DataStatus& data, std::string& error);

} // namespace OpenBlood2
