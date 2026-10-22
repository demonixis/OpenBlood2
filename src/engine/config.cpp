#include "engine/config.h"

#include <algorithm>
#include <array>
#include <charconv>
#include <fstream>
#include <system_error>

namespace OpenBlood2
{
    namespace fs = std::filesystem;
    namespace
    {

        std::string_view trim(std::string_view value)
        {
            const auto first = value.find_first_not_of(" \t\r\n");
            if (first == std::string_view::npos)
                return {};
            return value.substr(first, value.find_last_not_of(" \t\r\n") - first + 1);
        }

        std::string ascii_lower(std::string value)
        {
            for (auto& c : value)
                if (c >= 'A' && c <= 'Z')
                    c = static_cast<char>(c + ('a' - 'A'));
            return value;
        }

        bool read_config(const fs::path& path, fs::path& data_dir, std::string& error)
        {
            std::error_code ec;
            const auto bytes = fs::file_size(path, ec);
            if (ec || bytes > 65536)
            {
                error = "Cannot read configuration (maximum 64 KiB): " + path_text(path);
                return false;
            }
            std::ifstream file(path);
            if (!file)
            {
                error = "Cannot open configuration: " + path_text(path);
                return false;
            }
            bool seen_data = false;
            std::string line;
            unsigned line_number = 0;
            while (std::getline(file, line))
            {
                ++line_number;
                const auto text = trim(line);
                if (text.empty() || text.front() == '#')
                    continue;
                const auto equals = text.find('=');
                if (equals == std::string_view::npos || trim(text.substr(0, equals)) != "data_dir" || seen_data)
                {
                    error = "Unknown, malformed or duplicate configuration key at line " + std::to_string(line_number);
                    return false;
                }
                auto value = trim(text.substr(equals + 1));
                if (value.size() >= 2 && value.front() == '"' && value.back() == '"')
                {
                    value = value.substr(1, value.size() - 2);
                }
                if (value.empty() || value.find('"') != std::string_view::npos)
                {
                    error = "Invalid data_dir at line " + std::to_string(line_number);
                    return false;
                }
                data_dir = path_from_utf8(value);
                seen_data = true;
            }
            if (file.bad())
            {
                error = "Failed while reading configuration: " + path_text(path);
                return false;
            }
            return true;
        }

    } // namespace

    std::string path_text(const fs::path& path)
    {
        const auto text = path.generic_u8string();
        return {text.begin(), text.end()};
    }

    fs::path path_from_utf8(std::string_view text)
    {
        return fs::path(std::u8string(text.begin(), text.end()));
    }

    bool parse_options(std::span<const std::string_view> args, Options& options, std::string& error)
    {
        options = {};
        error.clear();
        bool explicit_world = false;
        for (std::size_t i = 0; i < args.size(); ++i)
        {
            const auto arg = args[i];
            if (arg == "--menu-smoke")
                options.menu_smoke = true;
            else if (arg == "--demo")
                options.demo = true;
            else if (arg == "--demo-smoke")
                options.demo_smoke = true;
            else if (arg == "--help" || arg == "-h")
                options.help = true;
            else if (arg == "--inspect-world")
                options.inspect_world = true;
            else if (arg == "--inspect-player")
            {
                options.inspect_world = true;
                options.inspect_player = true;
            }
            else if (arg == "--view-world")
                options.view_world = true;
            else if (arg == "--viewport-smoke")
                options.world_smoke = options.player_smoke = options.movement_smoke = options.viewport_smoke = true;
            else if (arg == "--weapons-smoke")
            {
                options.world_smoke = true;
                options.player_smoke = true;
                options.weapons_smoke = true;
            }
            else if (arg == "--movement-smoke" || arg == "--skeleton-smoke")
            {
                options.world_smoke = true;
                options.player_smoke = true;
                options.movement_smoke = true;
                options.skeleton_smoke = arg == "--skeleton-smoke";
            }
            else if (arg == "--containers-smoke")
            {
                options.world_smoke = true;
                options.player_smoke = true;
                options.containers_smoke = true;
            }
            else if (arg == "--scene-explosions-smoke")
                options.world_smoke = options.player_smoke = options.scene_explosions_smoke = true;
            else if (arg == "--sprites-smoke")
                options.world_smoke = options.player_smoke = options.sprites_smoke = true;
            else if (arg == "--world-sounds-smoke")
                options.world_smoke = options.player_smoke = options.world_sounds_smoke = true;
            else if (arg == "--random-sounds-smoke")
                options.world_smoke = options.player_smoke = options.random_sounds_smoke = true;
            else if (arg == "--cinematics-smoke")
                options.world_smoke = options.player_smoke = options.cinematics_smoke = true;
            else if (arg == "--lights-smoke")
            {
                options.world_smoke = options.player_smoke = options.lights_smoke = true;
            }
            else if (arg == "--worldpaths-smoke")
            {
                options.world_smoke = options.player_smoke = options.keyframes_smoke = options.worldpaths_smoke = true;
            }
            else if (arg == "--keyframes-smoke")
            {
                options.world_smoke = options.player_smoke = options.keyframes_smoke = true;
            }
            else if (arg == "--cameras-smoke")
            {
                options.world_smoke = options.player_smoke = options.cameras_smoke = true;
            }
            else if (arg == "--objectives-smoke")
            {
                options.world_smoke = options.player_smoke = options.objectives_smoke = true;
            }
            else if (arg == "--pickups-smoke")
            {
                options.world_smoke = options.player_smoke = options.pickups_smoke = true;
            }
            else if (arg == "--brushes-smoke")
            {
                options.world_smoke = options.brushes_smoke = true;
            }
            else if (arg == "--interaction-smoke")
            {
                options.world_smoke = options.player_smoke = options.interaction_smoke = true;
            }
            else if (arg == "--triggers-smoke")
            {
                options.world_smoke = options.player_smoke = options.triggers_smoke = true;
            }
            else if (arg == "--doors-smoke")
            {
                options.world_smoke = true;
                options.doors_smoke = true;
            }
            else if (arg == "--keepalive-smoke")
                options.world_smoke = options.player_smoke = options.restore_smoke = options.keepalive_smoke = true;
            else if (arg == "--restore-smoke")
                options.world_smoke = options.player_smoke = options.restore_smoke = true;
            else if (arg == "--player-smoke")
            {
                options.world_smoke = true;
                options.player_smoke = true;
            }
            else if (arg == "--start-game")
                options.start_game = true;
            else if (arg == "--check-data")
                options.check_data = true;
            else if (arg == "--no-inspector")
                options.inspector = false;
            else if (arg == "--no-crash-capture")
                options.crash_capture = false;
            else if (arg == "--world-smoke")
                options.world_smoke = true;
            else if (arg == "--xr-test")
                options.xr_test = true;
            else if (arg == "--xr")
                options.xr_world = true;
            else if (arg == "--gpu-smoke")
                options.gpu_smoke = true;
            else if (arg == "--bench")
                options.bench = true;
            else if (arg == "--no-model-shadows")
                options.model_shadows = false;
            else if (arg == "--pixel-lighting")
                options.pixel_lighting = true;
            else if (arg == "--normal-maps")
                options.normal_maps = true;
            else if (arg == "--renderer")
            {
                const auto value = ++i < args.size() ? args[i] : std::string_view{};
                if (value != "original" && value != "standard" && value != "high")
                {
                    error = "--renderer requires original, standard or high";
                    return false;
                }
                options.renderer = value == "original" ? 0 : value == "standard" ? 1 : 2;
            }
            else if (arg == "--aa")
            {
                const auto value = ++i < args.size() ? args[i] : std::string_view{};
                constexpr std::array<std::string_view, 4> modes{"off", "fxaa", "msaa2", "msaa4"};
                const auto it = std::find(modes.begin(), modes.end(), value);
                if (it == modes.end())
                {
                    error = "--aa requires off, fxaa, msaa2 or msaa4";
                    return false;
                }
                options.antialiasing = int(it - modes.begin());
            }
            else if (arg == "--no-bloom")
                options.bloom = false;
            else if (arg == "--tonemap")
            {
                const auto value = ++i < args.size() ? args[i] : std::string_view{};
                if (value != "off" && value != "neutral" && value != "filmic")
                {
                    error = "--tonemap requires off, neutral or filmic";
                    return false;
                }
                options.tonemap = value == "off" ? 0 : value == "neutral" ? 1 : 2;
            }
            else if (arg == "--bench-sweep")
                options.bench = options.bench_sweep = true;
            else if (arg == "--bench-audio")
                options.bench = options.bench_audio = true;
            else if (arg == "--bench-size")
            {
                options.bench = true;
                const auto value = ++i < args.size() ? args[i] : std::string_view{};
                const auto x = std::min(value.find('x'), value.size());
                std::uint32_t w = 0, h = 0;
                const auto a = std::from_chars(value.data(), value.data() + x, w);
                const auto b = x == value.size()
                                   ? std::from_chars_result{value.data(), std::errc::invalid_argument}
                                   : std::from_chars(value.data() + x + 1, value.data() + value.size(), h);
                if (a.ec != std::errc{} || b.ec != std::errc{} || a.ptr != value.data() + x ||
                    b.ptr != value.data() + value.size() || w < 16 || h < 16 || w > 8192 || h > 8192)
                {
                    error = "--bench-size requires WIDTHxHEIGHT from 16 to 8192";
                    return false;
                }
                options.bench_width = w;
                options.bench_height = h;
            }
            else if (arg == "--data-dir" || arg == "--config" || arg == "--frames" || arg == "--capture" ||
                     arg == "--world" || arg == "--log-dir" || arg == "--log-level" || arg == "--rez")
            {
                if (++i == args.size() || args[i].empty() || args[i].starts_with("--"))
                {
                    error = "Missing value for " + std::string(arg);
                    return false;
                }
                if (arg == "--rez")
                {
                    const auto path = path_from_utf8(args[i]);
                    if (!path.is_absolute() || options.overlays.size() >= 24)
                    {
                        error = "--rez requires an absolute archive path (maximum 24)";
                        return false;
                    }
                    options.overlays.push_back(path);
                }
                else if (arg == "--world")
                {
                    options.world = args[i];
                    explicit_world = true;
                }
                else if (arg == "--log-dir")
                    options.log_dir = path_from_utf8(args[i]);
                else if (arg == "--log-level")
                {
                    if (args[i] != "trace" && args[i] != "debug" && args[i] != "info" && args[i] != "warn" &&
                        args[i] != "error")
                    {
                        error = "--log-level requires trace, debug, info, warn or error";
                        return false;
                    }
                    options.log_level = args[i];
                }
                else if (arg == "--data-dir")
                    options.data_dir = path_from_utf8(args[i]);
                else if (arg == "--capture")
                    options.capture_file = path_from_utf8(args[i]);
                else if (arg == "--config")
                {
                    options.config_file = path_from_utf8(args[i]);
                    options.explicit_config = true;
                }
                else
                {
                    const auto value = args[i];
                    const auto result = std::from_chars(value.data(), value.data() + value.size(), options.frame_limit);
                    if (result.ec != std::errc{} || result.ptr != value.data() + value.size() ||
                        options.frame_limit == 0 || options.frame_limit > 1000000)
                    {
                        error = "--frames requires an integer from 1 to 1000000";
                        return false;
                    }
                }
            }
            else
            {
                error = "Unknown option: " + std::string(arg);
                return false;
            }
        }
        if (options.menu_smoke &&
            (options.view_world || options.inspect_world || options.start_game || options.check_data ||
             options.gpu_smoke || options.world_smoke || options.bench || options.frame_limit))
        {
            error = "--menu-smoke is a separate diagnostic with its own frame count";
            return false;
        }
        if (options.view_world && (options.inspect_world || options.start_game || options.check_data ||
                                   options.gpu_smoke || options.world_smoke))
        {
            error = "--view-world is a separate interactive inspection mode";
            return false;
        }
        if (options.bench && (options.view_world || options.inspect_world || options.start_game || options.check_data ||
                              options.gpu_smoke || options.world_smoke || options.player_smoke))
        {
            error = "--bench is a separate headless measurement mode";
            return false;
        }
        // The scripted smokes assert full-game SubwayA/CityHub content.
        const bool content_smoke =
            options.gpu_smoke || options.world_smoke || options.player_smoke || options.restore_smoke ||
            options.keepalive_smoke || options.weapons_smoke || options.skeleton_smoke || options.movement_smoke ||
            options.containers_smoke || options.doors_smoke || options.triggers_smoke || options.interaction_smoke ||
            options.brushes_smoke || options.pickups_smoke || options.objectives_smoke || options.cameras_smoke ||
            options.keyframes_smoke || options.worldpaths_smoke || options.lights_smoke || options.cinematics_smoke ||
            options.random_sounds_smoke || options.world_sounds_smoke || options.sprites_smoke ||
            options.scene_explosions_smoke;
        if (options.demo && content_smoke)
        {
            error = "--demo supports normal launch, --view-world, --check-data, --inspect-world, --inspect-player, "
                    "--start-game and --bench; smoke diagnostics need the full game";
            return false;
        }
        if (options.demo_smoke &&
            (!options.demo || options.menu_smoke || content_smoke || options.view_world || options.inspect_world ||
             options.start_game || options.check_data || options.bench || options.frame_limit || explicit_world))
        {
            error = "--demo-smoke requires --demo and its default first world; it owns its diagnostic frames";
            return false;
        }
        if (options.demo && options.menu_smoke && (options.frame_limit || explicit_world))
        {
            error = "Demo menu smoke owns its world and frame count";
            return false;
        }
        if (options.demo && !explicit_world)
            options.world = first_level(true);
        if (options.world_smoke && options.gpu_smoke)
        {
            error = "Choose one GPU smoke mode";
            return false;
        }
        if (int(options.weapons_smoke) + int(options.movement_smoke) + int(options.containers_smoke) +
                int(options.doors_smoke) + int(options.triggers_smoke) + int(options.interaction_smoke) +
                int(options.brushes_smoke) + int(options.pickups_smoke) + int(options.objectives_smoke) +
                int(options.cameras_smoke) + int(options.keyframes_smoke) + int(options.lights_smoke) +
                int(options.cinematics_smoke) + int(options.random_sounds_smoke) + int(options.world_sounds_smoke) +
                int(options.sprites_smoke) + int(options.scene_explosions_smoke) >
            1)
        {
            error = "Choose one player smoke scenario";
            return false;
        }
        if ((options.inspect_world && options.start_game) ||
            ((options.inspect_world || options.start_game) &&
             (options.check_data || options.gpu_smoke || options.world_smoke || !options.capture_file.empty() ||
              options.frame_limit)))
        {
            error = "--inspect-world and --start-game are separate headless modes";
            return false;
        }
        if ((options.gpu_smoke || options.world_smoke) && options.frame_limit != 0)
        {
            error = "--gpu-smoke controls its own frame count; omit --frames";
            return false;
        }
        if ((options.gpu_smoke || options.world_smoke) && options.check_data)
        {
            error = "--gpu-smoke and --check-data are separate execution modes";
            return false;
        }
        if (options.check_data && !options.capture_file.empty())
        {
            error = "--capture requires a graphical run; omit --check-data";
            return false;
        }
        if (!options.capture_file.empty() && options.frame_limit != 0 && options.frame_limit < 3)
        {
            error = "--capture needs at least 3 frames for UI layout; increase --frames";
            return false;
        }
        return true;
    }

    bool load_config(const Options& options, std::string_view environment_data_dir, Config& config, std::string& error,
                     bool* unconfigured)
    {
        if (unconfigured)
            *unconfigured = false;
        config = {};
        error.clear();
        config.demo = options.demo;
        config.overlays = options.overlays;
        std::error_code ec;
        const bool exists = fs::exists(options.config_file, ec);
        if (ec || (options.explicit_config && !exists))
        {
            error = "Configuration not found or inaccessible: " + path_text(options.config_file);
            return false;
        }
        if (exists)
        {
            if (!read_config(options.config_file, config.data_dir, error))
                return false;
            if (!config.data_dir.empty() && config.data_dir.is_relative())
            {
                config.data_dir = options.config_file.parent_path() / config.data_dir;
            }
            config.source = path_text(options.config_file);
        }
        if (!environment_data_dir.empty())
        {
            config.data_dir = path_from_utf8(environment_data_dir);
            config.source = "BLOOD2_DATA_DIR";
        }
        if (!options.data_dir.empty())
        {
            config.data_dir = options.data_dir;
            config.source = "--data-dir";
        }
        if (config.data_dir.empty())
        {
            if (unconfigured)
                *unconfigured = true;
            error = "No Blood II data directory. Set --data-dir, BLOOD2_DATA_DIR or data_dir in blood2.local.ini.";
            return false;
        }
        config.data_dir = fs::absolute(config.data_dir, ec).lexically_normal();
        if (ec)
        {
            error = "Cannot resolve data directory: " + ec.message();
            return false;
        }
        return true;
    }

    bool inspect_data(const Config& config, DataStatus& data, std::string& error)
    {
        data = {};
        error.clear();
        // The 1998 demo ships every resource in one archive (B2Demo.rez).
        if (config.demo)
            data.archives = {{"B2DEMO.REZ", true}};
        else
            data.archives = {{"BLOOD2.REZ", true}, {"SOUND.REZ"}, {"BLOOD2P.REZ"}, {"BLOOD2P2.REZ"}, {"BLOOD2NM.REZ"}};
        bool other_edition = false;
        std::error_code ec;
        if (!fs::is_directory(config.data_dir, ec) || ec)
        {
            error = "Blood II data directory not found or inaccessible: " + path_text(config.data_dir);
            return false;
        }
        fs::path readme;
        fs::directory_iterator it(config.data_dir, ec);
        if (ec)
        {
            error = "Cannot enumerate data directory: " + ec.message();
            return false;
        }
        for (const fs::directory_iterator end; it != end; it.increment(ec))
        {
            if (ec)
                break;
            const auto lower = ascii_lower(path_text(it->path().filename()));
            if (lower == "readme.txt")
                readme = it->path();
            other_edition |= lower == (config.demo ? "blood2.rez" : "b2demo.rez");
            for (auto& archive : data.archives)
            {
                if (lower != ascii_lower(archive.name))
                    continue;
                if (archive.present)
                {
                    error = "Ambiguous archive names differing only in case: " + archive.name;
                    return false;
                }
                if (!it->is_regular_file(ec) || ec)
                {
                    error = "Archive is not a readable regular file: " + path_text(it->path());
                    return false;
                }
                archive.bytes = it->file_size(ec);
                std::ifstream probe(it->path(), std::ios::binary);
                if (ec || archive.bytes == 0 || !probe)
                {
                    error = "Archive is empty or unreadable: " + path_text(it->path());
                    return false;
                }
                archive.present = true;
            }
        }
        if (ec)
        {
            error = "Failed enumerating data directory: " + ec.message();
            return false;
        }
        if (!data.archives.front().present)
        {
            error = "Required archive " + data.archives.front().name + " is missing from " + path_text(config.data_dir);
            if (other_edition)
                error += config.demo ? "; this looks like the full game: omit --demo"
                                     : "; this looks like the original demo: add --demo";
            return false;
        }
        // Version is informational: the readme is not proof of the executable version.
        if (!readme.empty())
        {
            std::ifstream file(readme, std::ios::binary);
            std::string prefix(4096, '\0');
            file.read(prefix.data(), static_cast<std::streamsize>(prefix.size()));
            prefix.resize(static_cast<std::size_t>(file.gcount()));
            const auto pos = prefix.find("Blood II: The Chosen Version");
            if (pos != std::string::npos)
            {
                const auto end = prefix.find_first_of("\r\n", pos);
                data.readme_version = prefix.substr(pos, std::min<std::size_t>(end - pos, 120));
            }
        }
        return true;
    }

} // namespace OpenBlood2
