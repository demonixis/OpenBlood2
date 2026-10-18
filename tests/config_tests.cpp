#include "engine/config.h"

#include <array>
#include <fstream>
#include <iostream>

namespace fs = std::filesystem;
using namespace OpenBlood2;

#define CHECK(condition)                                                                                               \
    do                                                                                                                 \
    {                                                                                                                  \
        if (!(condition))                                                                                              \
        {                                                                                                              \
            std::cerr << "FAIL line " << __LINE__ << ": " #condition "\n";                                             \
            return false;                                                                                              \
        }                                                                                                              \
    } while (false)

namespace
{

    bool write(const fs::path& path, std::string_view text)
    {
        std::ofstream file(path, std::ios::binary);
        file << text;
        file.close();
        return !file.fail();
    }

    bool arguments(const fs::path&)
    {
        Options options;
        std::string error;
        const std::array<std::string_view, 6> valid{"--data-dir", "a path with spaces", "--frames",
                                                    "12",         "--no-inspector",     "--check-data"};
        CHECK(parse_options(valid, options, error));
        CHECK(options.frame_limit == 12 && !options.inspector && options.check_data);
        CHECK(path_text(options.data_dir) == "a path with spaces");
        const std::array<std::string_view, 4> log_args{"--log-dir", "logs é with spaces", "--log-level", "debug"};
        CHECK(parse_options(log_args, options, error));
        CHECK(path_text(options.log_dir) == "logs é with spaces" && options.log_level == "debug");
        for (const auto level : {"trace", "debug", "info", "warn", "error"})
        {
            const std::array<std::string_view, 2> args{"--log-level", level};
            CHECK(parse_options(args, options, error) && options.log_level == level);
        }
        for (const auto level : {"", "verbose", "off", "INFO"})
        {
            const std::array<std::string_view, 2> args{"--log-level", level};
            CHECK(!parse_options(args, options, error));
        }
        for (const auto name : {"--log-dir", "--log-level"})
        {
            const std::array<std::string_view, 1> args{name};
            CHECK(!parse_options(args, options, error));
        }
        for (const auto value : {"-1", "0", "12x", "1000001", "999999999999999999999"})
        {
            const std::array<std::string_view, 2> args{"--frames", value};
            CHECK(!parse_options(args, options, error));
        }
        const std::array<std::string_view, 3> container_args{"--containers-smoke", "--world", "02_CityHub"};
        CHECK(parse_options(container_args, options, error));
        CHECK(options.containers_smoke && options.player_smoke && options.world_smoke && options.world == "02_CityHub");
        const std::array<std::string_view, 2> both_scenarios{"--containers-smoke", "--movement-smoke"};
        CHECK(!parse_options(both_scenarios, options, error));
        const std::array<std::string_view, 1> sprites{"--sprites-smoke"};
        CHECK(parse_options(sprites, options, error) && options.sprites_smoke && options.player_smoke &&
              options.world_smoke);
        const std::array<std::string_view, 2> conflict_sprites{"--sprites-smoke", "--world-sounds-smoke"};
        CHECK(!parse_options(conflict_sprites, options, error));
        const std::array<std::string_view, 1> pickups{"--pickups-smoke"};
        CHECK(parse_options(pickups, options, error) && options.pickups_smoke && options.player_smoke &&
              options.world_smoke);
        const std::array<std::string_view, 2> conflicting_pickups{"--pickups-smoke", "--brushes-smoke"};
        CHECK(!parse_options(conflicting_pickups, options, error));
        const std::array<std::string_view, 1> missing{"--data-dir"};
        CHECK(!parse_options(missing, options, error));
        const std::array<std::string_view, 1> unknown{"--typo"};
        CHECK(!parse_options(unknown, options, error));
        const std::array<std::string_view, 3> conflict{"--gpu-smoke", "--frames", "5"};
        CHECK(!parse_options(conflict, options, error));
        const std::array<std::string_view, 4> short_capture{"--capture", "frame.png", "--frames", "1"};
        CHECK(!parse_options(short_capture, options, error));
        const std::array<std::string_view, 3> headless_capture{"--capture", "frame.png", "--check-data"};
        CHECK(!parse_options(headless_capture, options, error));
        const std::array<std::string_view, 2> conflicting_modes{"--gpu-smoke", "--check-data"};
        CHECK(!parse_options(conflicting_modes, options, error));
        return true;
    }

    bool precedence(const fs::path& root)
    {
        Options options;
        options.config_file = root / "config.ini";
        options.explicit_config = true;
        CHECK(write(options.config_file, "data_dir = from-file\n"));
        Config config;
        std::string error;
        CHECK(load_config(options, "", config, error));
        CHECK(config.data_dir == root / "from-file");
        const auto env = path_text(root / "from-environment");
        CHECK(load_config(options, env, config, error));
        CHECK(config.data_dir == root / "from-environment");
        options.data_dir = root / "from-command-line";
        CHECK(load_config(options, env, config, error));
        CHECK(config.data_dir == options.data_dir && config.source == "--data-dir");
        return true;
    }

    bool relative_paths(const fs::path& root)
    {
        Options options;
        options.config_file = root / "config.ini";
        CHECK(write(options.config_file, "# Spaces and UTF-8 survive parsing\r\ndata_dir = \"Blood II \xc3\xa9\"\r\n"));
        Config config;
        std::string error;
        CHECK(load_config(options, "", config, error));
        CHECK(config.data_dir == root / fs::path(u8"Blood II é"));
        return true;
    }

    bool invalid_config(const fs::path& root)
    {
        Options options;
        options.config_file = root / "config.ini";
        options.explicit_config = true;
        Config config;
        std::string error;
        CHECK(!load_config(options, "", config, error));
        for (const auto text :
             {"data_dir =\n", "unknown = path\n", "data_dir = a\ndata_dir = b\n", "data_dir = \"broken\n"})
        {
            CHECK(write(options.config_file, text));
            CHECK(!load_config(options, "", config, error));
        }
        CHECK(write(options.config_file, std::string(65537, 'x')));
        CHECK(!load_config(options, "", config, error));
        CHECK(write(options.config_file, "# Empty config requires another source\n"));
        CHECK(!load_config(options, "", config, error));
        return true;
    }

    bool content_validation(const fs::path& root)
    {
        Config config{root, "test"};
        DataStatus data;
        std::string error;
        CHECK(!inspect_data(config, data, error));
        CHECK(error.find("BLOOD2.REZ is missing") != std::string::npos);
        CHECK(write(root / "BLOOD2.REZ", ""));
        CHECK(!inspect_data(config, data, error));
        CHECK(error.find("empty or unreadable") != std::string::npos);
        CHECK(write(root / "BLOOD2.REZ", "synthetic metadata fixture"));
        CHECK(inspect_data(config, data, error));
        CHECK(data.archives.size() == 5 && data.archives[0].present && !data.archives[1].present);
        config.data_dir = root / "does-not-exist";
        CHECK(!inspect_data(config, data, error));
        return true;
    }

    bool case_lookup(const fs::path& root)
    {
        Config config{root, "test"};
        DataStatus data;
        std::string error;
        CHECK(write(root / "blood2.rez", "synthetic metadata fixture"));
        CHECK(write(root / "sOuNd.ReZ", "synthetic sound metadata"));
        CHECK(inspect_data(config, data, error));
        CHECK(data.archives[0].present && data.archives[1].present);
        // Only case-sensitive filesystems can represent this ambiguity.
        std::error_code ec;
        if (!fs::exists(root / "BLOOD2.REZ", ec))
        {
            CHECK(write(root / "BLOOD2.REZ", "duplicate"));
            CHECK(!inspect_data(config, data, error));
            CHECK(error.find("Ambiguous") != std::string::npos);
        }
        return true;
    }

} // namespace

int main(int argc, char** argv)
{
    if (argc != 3)
        return 2;
    const std::string_view name(argv[1]);
    bool (*test)(const fs::path&) = nullptr;
    if (name == "arguments")
        test = arguments;
    else if (name == "precedence")
        test = precedence;
    else if (name == "relative_paths")
        test = relative_paths;
    else if (name == "invalid_config")
        test = invalid_config;
    else if (name == "content_validation")
        test = content_validation;
    else if (name == "case_lookup")
        test = case_lookup;
    if (!test)
        return 2;
    std::error_code ec;
    const fs::path root = fs::absolute(path_from_utf8(argv[2]) / name, ec);
    if (ec)
        return 2;
    fs::remove_all(root, ec);
    if (ec)
        return 2;
    fs::create_directories(root, ec);
    if (ec)
        return 2;
    const bool ok = test(root);
    fs::remove_all(root, ec);
    if (ok)
        std::cout << "PASS " << name << '\n';
    return ok ? 0 : 1;
}
