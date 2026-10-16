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
        std::filesystem::path capture_file;
        std::uint32_t frame_limit = 0;
        std::string world = "01_SubwayA";
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
        bool explicit_config = false;
        bool help = false;
        bool check_data = false;
        bool inspector = true;
        bool gpu_smoke = false;
        bool world_smoke = false;
    };

    struct Config
    {
        std::filesystem::path data_dir;
        std::string source;
    };

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
