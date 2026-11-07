#include "engine/config.h"

#include "scratch_dir.h"
#include <array>
#include <vector>
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
        CHECK(options.crash_capture);
        const std::array<std::string_view, 1> no_crash{"--no-crash-capture"};
        CHECK(parse_options(no_crash, options, error) && !options.crash_capture);
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

    bool demo(const fs::path& root)
    {
        Options options;
        std::string error;
        const std::array<std::string_view, 1> plain{"--demo"};
        CHECK(parse_options(plain, options, error) && options.demo && options.world == first_level(true));
        const std::array<std::string_view, 3> world{"--world", "Demo_02", "--demo"};
        CHECK(parse_options(world, options, error) && options.demo && options.world == "Demo_02");
        const std::array<std::string_view, 2> view{"--demo", "--view-world"};
        CHECK(parse_options(view, options, error) && options.view_world);
        const std::array<std::string_view, 2> demo_smoke{"--demo", "--demo-smoke"};
        CHECK(parse_options(demo_smoke, options, error) && options.demo_smoke);
        for (const auto mode : {"--view-world", "--inspect-world", "--inspect-player", "--start-game", "--check-data",
                                "--bench", "--menu-smoke"})
        {
            const std::array<std::string_view, 3> args{"--demo", "--demo-smoke", mode};
            CHECK(!parse_options(args, options, error));
        }
        const std::array<std::string_view, 1> no_edition{"--demo-smoke"};
        CHECK(!parse_options(no_edition, options, error));
        for (const auto smoke : {"--weapons-smoke", "--gpu-smoke", "--player-smoke"})
        {
            const std::array<std::string_view, 2> args{"--demo", smoke};
            CHECK(!parse_options(args, options, error) && error.find("--demo") != std::string::npos);
        }
        const std::array<std::string_view, 2> demo_menu{"--demo", "--menu-smoke"};
        CHECK(parse_options(demo_menu, options, error) && options.menu_smoke && options.demo);
        const std::array<std::string_view, 4> demo_menu_world{"--demo", "--menu-smoke", "--world", "Demo_02"};
        CHECK(!parse_options(demo_menu_world, options, error));
        const std::array<std::string_view, 0> none{};
        CHECK(parse_options(none, options, error) && !options.demo && options.world == first_level(false));

        options.config_file = root / "absent.ini";
        options.data_dir = root;
        options.demo = true;
        Config config;
        CHECK(load_config(options, "", config, error) && config.demo);
        DataStatus data;
        CHECK(write(root / "BLOOD2.REZ", "synthetic full metadata"));
        CHECK(!inspect_data(config, data, error));
        CHECK(error.find("B2DEMO.REZ is missing") != std::string::npos &&
              error.find("omit --demo") != std::string::npos);
        CHECK(write(root / "B2Demo.rez", "synthetic demo metadata"));
        CHECK(inspect_data(config, data, error));
        CHECK(data.archives.size() == 1 && data.archives[0].name == "B2DEMO.REZ" && data.archives[0].present);
        std::error_code ec;
        CHECK(fs::remove(root / "BLOOD2.REZ", ec) && !ec);
        config.demo = false;
        CHECK(!inspect_data(config, data, error));
        CHECK(error.find("BLOOD2.REZ is missing") != std::string::npos &&
              error.find("add --demo") != std::string::npos);
        return true;
    }

    // The Nightmares edition (nightmares_plan.md): the retail set plus a required BLOOD2NM.REZ and an
    // optional BLOOD2NM2.REZ; it is exclusive with the demo and never inferred from the folder.
    bool nightmares(const fs::path& root)
    {
        Options options;
        std::string error;
        const std::array<std::string_view, 1> plain{"--nightmares"};
        CHECK(parse_options(plain, options, error) && options.nightmares && !options.demo &&
              options.world == first_level(false));
        const std::array<std::string_view, 2> both{"--demo", "--nightmares"};
        CHECK(!parse_options(both, options, error) && error == "--demo and --nightmares select different editions");
        const std::array<std::string_view, 0> none{};
        CHECK(parse_options(none, options, error) && !options.nightmares);

        options.config_file = root / "absent.ini";
        options.data_dir = root;
        options.nightmares = true;
        Config config;
        CHECK(load_config(options, "", config, error) && config.nightmares && !config.demo);
        DataStatus data;
        CHECK(write(root / "BLOOD2.REZ", "synthetic full metadata"));
        CHECK(!inspect_data(config, data, error));
        CHECK(error.find("BLOOD2NM.REZ is missing") != std::string::npos &&
              error.find("Nightmares expansion is not installed") != std::string::npos);
        CHECK(write(root / "Blood2NM.rez", "synthetic expansion metadata"));
        CHECK(inspect_data(config, data, error));
        CHECK(data.archives.size() == 6 && data.archives[4].name == "BLOOD2NM.REZ" && data.archives[4].required &&
              data.archives[4].present && data.archives[5].name == "BLOOD2NM2.REZ" && !data.archives[5].required &&
              !data.archives[5].present);
        // Retail lists the expansion as optional information only.
        config.nightmares = false;
        CHECK(inspect_data(config, data, error) && data.archives.size() == 5 && !data.archives[4].required &&
              data.archives[4].present);
        std::error_code ec;
        CHECK(fs::remove(root / "BLOOD2.REZ", ec) && !ec);
        config.nightmares = true;
        CHECK(!inspect_data(config, data, error) && error.find("BLOOD2.REZ is missing") != std::string::npos);
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

    // GI lots G2/G4: gi_dir= / --gi-dir, gi_placement_dir= and the --list-worlds / --gi-verify /
    // --gi-place tools.
    bool gi(const fs::path& root)
    {
        Options options;
        std::string error;
        const std::array<std::string_view, 2> list{"--list-worlds", "--json"};
        CHECK(parse_options(list, options, error) && options.list_worlds && options.json);
        CHECK(options.log_level == "error"); // JSON owns standard output.
        const std::array<std::string_view, 4> list_level{"--list-worlds", "--json", "--log-level", "debug"};
        CHECK(parse_options(list_level, options, error) && options.log_level == "debug");
        const std::array<std::string_view, 3> verify{"--gi-verify", "a.ob2gi", "--gi-dir"};
        CHECK(!parse_options(verify, options, error)); // --gi-dir needs a value.
        const std::array<std::string_view, 4> verify_dir{"--gi-verify", "a.ob2gi", "--gi-dir", "bakes"};
        CHECK(parse_options(verify_dir, options, error));
        CHECK(path_text(options.gi_verify) == "a.ob2gi" && path_text(options.gi_dir) == "bakes");
        for (const auto& args : std::initializer_list<std::vector<std::string_view>>{
                 {"--json"},
                 {"--gi-verify", "a.ob2gi", "--json"},
                 {"--list-worlds", "--gi-verify", "a.ob2gi"},
                 {"--list-worlds", "--world", "02_CityHub"},
                 {"--list-worlds", "--check-data"},
                 {"--gi-verify", "a.ob2gi", "--bench"},
                 {"--list-worlds", "--view-world"},
                 {"--gi-verify", "a.ob2gi", "--frames", "5"}})
            CHECK(!parse_options(args, options, error));

        options = {};
        options.config_file = root / "config.ini";
        options.explicit_config = true;
        Config config;
        CHECK(write(options.config_file, "data_dir = data\ngi_dir = \"GI bakes\"\n"));
        CHECK(load_config(options, "", config, error));
        CHECK(config.gi_dir == root / "GI bakes"); // Relative to the configuration file.
        options.gi_dir = root / "from-command-line";
        CHECK(load_config(options, "", config, error) && config.gi_dir == options.gi_dir);
        options.gi_dir.clear();
        CHECK(write(options.config_file, "gi_dir = /absolute/gi\ndata_dir = data\n"));
        CHECK(load_config(options, "", config, error) && config.gi_dir == fs::path("/absolute/gi"));
        CHECK(write(options.config_file, "data_dir = data\n"));
        CHECK(load_config(options, "", config, error) && config.gi_dir.empty());
        for (const auto text : {"data_dir = data\ngi_dir =\n", "data_dir = data\ngi_dir = a\ngi_dir = b\n",
                                "data_dir = data\nGI_DIR = a\n", "data_dir = data\ngi_dir = \"broken\n"})
        {
            CHECK(write(options.config_file, text));
            CHECK(!load_config(options, "", config, error));
        }

        // GI lot G4: --gi-place, its modifiers and gi_placement_dir= / --gi-placement-dir.
        options = {};
        const std::array<std::string_view, 6> place{"--gi-place", "all",    "--gi-reset",
                                                    "--gi-dry-run", "--json", "--gi-placement-dir"};
        CHECK(!parse_options(place, options, error)); // --gi-placement-dir needs a value.
        const std::array<std::string_view, 7> place_dir{"--gi-place", "all",    "--gi-reset",         "--gi-dry-run",
                                                        "--json",     "--gi-placement-dir", "placements"};
        options = {};
        CHECK(parse_options(place_dir, options, error));
        CHECK(options.gi_place == "all" && options.gi_reset && options.gi_dry_run && options.json);
        CHECK(path_text(options.gi_placement_dir) == "placements" && options.log_level == "error");
        for (const auto& args : std::initializer_list<std::vector<std::string_view>>{
                 {"--gi-place"},
                 {"--gi-reset"},
                 {"--gi-dry-run"},
                 {"--gi-reset", "--list-worlds"},
                 {"--gi-place", "all", "--list-worlds"},
                 {"--gi-place", "all", "--gi-verify", "a.ob2gi"},
                 {"--gi-place", "all", "--world", "02_CityHub"},
                 {"--gi-place", "single", "--view-world"},
                 {"--gi-place", "all", "--bench"}})
        {
            options = {};
            CHECK(!parse_options(args, options, error));
        }
        options = {};
        const std::array<std::string_view, 3> separate{"--gi-place", "all", "--list-worlds"};
        CHECK(!parse_options(separate, options, error));
        CHECK(error == "--list-worlds, --gi-verify, --gi-place, --bake-gi and --gi-preview are separate headless "
                       "tools");

        // GI lot G5: --bake-gi and its options.
        options = {};
        const std::array<std::string_view, 12> bake{"--bake-gi",   "01_SubwayA", "--gi-quality", "custom",
                                                    "--gi-bounces", "3",          "--gi-rays",    "4096",
                                                    "--gi-jobs",   "7",          "--gi-out",     "bakes"};
        CHECK(parse_options(bake, options, error));
        CHECK(options.bake_gi == "01_SubwayA" && options.gi_quality == "custom" && options.gi_bounces == 3);
        CHECK(options.gi_rays == 4096 && options.gi_jobs == 7 && path_text(options.gi_out) == "bakes");
        CHECK(!options.gi_if_stale);
        options = {};
        const std::array<std::string_view, 3> stale{"--bake-gi", "01_SubwayA", "--gi-if-stale"};
        CHECK(parse_options(stale, options, error) && options.gi_if_stale && options.gi_quality.empty());
        for (const auto& args : std::initializer_list<std::vector<std::string_view>>{
                 {"--bake-gi"},
                 {"--gi-if-stale"},
                 {"--gi-quality", "final"},
                 {"--gi-out", "bakes"},
                 {"--bake-gi", "a", "--gi-quality", "best"},
                 {"--bake-gi", "a", "--gi-rays", "64"},
                 {"--bake-gi", "a", "--gi-quality", "preview", "--gi-bounces", "1"},
                 {"--bake-gi", "a", "--gi-quality", "custom", "--gi-bounces", "4"},
                 {"--bake-gi", "a", "--gi-quality", "custom", "--gi-rays", "15"},
                 {"--bake-gi", "a", "--gi-jobs", "0"},
                 {"--bake-gi", "a", "--gi-jobs", "257"},
                 {"--bake-gi", "a", "--gi-jobs", "2x"},
                 {"--bake-gi", "a", "--gi-place", "all"},
                 {"--bake-gi", "a", "--world", "02_CityHub"}})
        {
            options = {};
            CHECK(!parse_options(args, options, error));
        }

        // GI lot G7: --gi-sky in exact hundredths (custom bakes only), --gi-preview and its options.
        for (const auto& [text, hundredths] : std::initializer_list<std::pair<std::string_view, int>>{
                 {"0", 0}, {"4", 400}, {"1.5", 150}, {"0.25", 25}, {"3.99", 399}, {"2.00", 200}})
        {
            options = {};
            const std::array<std::string_view, 6> sky{"--bake-gi", "a", "--gi-quality", "custom", "--gi-sky", text};
            CHECK(parse_options(sky, options, error) && options.gi_sky == hundredths);
        }
        for (const auto text : {"4.01", "5", "-1", "1.", ".5", "1.234", "1e0", "", "nan", "0x1", "1,5", "1000"})
        {
            options = {};
            const std::array<std::string_view, 6> sky{"--bake-gi", "a", "--gi-quality", "custom", "--gi-sky", text};
            CHECK(!parse_options(sky, options, error));
        }
        options = {};
        const std::array<std::string_view, 4> plain_sky{"--bake-gi", "a", "--gi-sky", "1"};
        CHECK(!parse_options(plain_sky, options, error));
        CHECK(error == "--gi-bounces, --gi-rays and --gi-sky require --gi-quality custom");
        options = {};
        const std::array<std::string_view, 12> preview{"--gi-preview", "07_Cathedral", "--gi-preview-dir", "shots",
                                                       "--gi-k",       "2",            "--gi-f",           "0.5",
                                                       "--gi-out",     "bakes",        "--gi-jobs",        "3"};
        CHECK(parse_options(preview, options, error));
        CHECK(options.gi_preview == "07_Cathedral" && path_text(options.gi_preview_dir) == "shots");
        CHECK(options.gi_k == 2 && options.gi_f == .5f && options.gi_composition);
        CHECK(path_text(options.gi_out) == "bakes" && options.gi_jobs == 3 && options.bake_gi.empty());
        options = {};
        const std::array<std::string_view, 2> preview_defaults{"--gi-preview", "07_Cathedral"};
        CHECK(parse_options(preview_defaults, options, error));
        CHECK(options.gi_k == 1 && options.gi_f == 1 && !options.gi_composition && options.gi_preview_dir.empty());
        for (const auto& args : std::initializer_list<std::vector<std::string_view>>{
                 {"--gi-preview"},
                 {"--gi-preview-dir", "shots"},
                 {"--gi-k", "1"},
                 {"--gi-f", "1"},
                 {"--bake-gi", "a", "--gi-k", "1"},
                 {"--gi-preview", "a", "--gi-k", "4.5"},
                 {"--gi-preview", "a", "--gi-f", "-1"},
                 {"--gi-preview", "a", "--gi-k", "nan"},
                 {"--gi-preview", "a", "--gi-k", "1x"},
                 {"--gi-preview", "a", "--gi-quality", "final"},
                 {"--gi-preview", "a", "--gi-sky", "1"},
                 {"--gi-preview", "a", "--bake-gi", "a"},
                 {"--gi-preview", "a", "--list-worlds"},
                 {"--gi-preview", "a", "--world", "02_CityHub"}})
        {
            options = {};
            CHECK(!parse_options(args, options, error));
        }

        options = {};
        options.config_file = root / "config.ini";
        options.explicit_config = true;
        CHECK(write(options.config_file, "data_dir = data\ngi_placement_dir = placements\ngi_dir = bakes\n"));
        CHECK(load_config(options, "", config, error));
        CHECK(config.gi_placement_dir == root / "placements" && config.gi_dir == root / "bakes");
        options.gi_placement_dir = root / "from-command-line";
        CHECK(load_config(options, "", config, error) && config.gi_placement_dir == options.gi_placement_dir);
        options.gi_placement_dir.clear();
        CHECK(write(options.config_file, "data_dir = data\n"));
        CHECK(load_config(options, "", config, error) && config.gi_placement_dir.empty());
        for (const auto text : {"data_dir = data\ngi_placement_dir =\n",
                                "data_dir = data\ngi_placement_dir = a\ngi_placement_dir = b\n"})
        {
            CHECK(write(options.config_file, text));
            CHECK(!load_config(options, "", config, error));
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
    else if (name == "demo")
        test = demo;
    else if (name == "nightmares")
        test = nightmares;
    else if (name == "gi")
        test = gi;
    if (!test)
        return 2;
    std::error_code ec;
    const fs::path root = fs::absolute(path_from_utf8(argv[2]) / name, ec);
    if (ec)
        return 2;
    if (!OpenBlood2::Testing::reset_scratch_dir(root))
        return 2;
    const bool ok = test(root);
    OpenBlood2::Testing::remove_scratch_dir(root);
    if (ok)
        std::cout << "PASS " << name << '\n';
    return ok ? 0 : 1;
}
