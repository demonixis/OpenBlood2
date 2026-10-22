#pragma once

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
        std::vector<std::filesystem::path> overlays{}; // Explicit, ordered data-only REZ mods.
        std::filesystem::path capture_file;
        std::filesystem::path log_dir;
        std::string log_level = "info";
        bool crash_capture = true;
        std::uint32_t frame_limit = 0;
        std::string world = "01_SubwayA"; // Demo_01 with --demo unless --world is given.
        bool demo = false;                // Original two-level demo layout (B2DEMO.REZ).
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
        std::uint32_t bench_width = 1920, bench_height = 1080;
    };

    struct Config
    {
        std::filesystem::path data_dir;
        std::string source;
        bool demo = false; // Selects the demo archive set; never inferred from directory contents.
        std::vector<std::filesystem::path> overlays{};
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
    bool load_config(const Options& options, std::string_view environment_data_dir, Config& config, std::string& error);
    // Checks file presence/readability only. Does not parse or mount an archive.
    bool inspect_data(const Config& config, DataStatus& data, std::string& error);

} // namespace OpenBlood2
