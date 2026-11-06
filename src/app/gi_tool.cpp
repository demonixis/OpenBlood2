#include "app/gi_tool.h"
#include "engine/atomic_file.h"
#include "engine/gi_file.h"
#include "engine/gi_placement.h"
#include "engine/resources.h"
#include <spdlog/spdlog.h>
#include <chrono>
#include <iostream>

namespace OpenBlood2
{
    namespace
    {
        // Single-player worlds (worlds/<name>.dat), multiplayer (worlds/multi/), other worlds/
        // subdirectories, then the Nightmares add-on (worlds_ao/); index (path) order within each.
        int world_group(const std::string& path)
        {
            if (path.starts_with("worlds/multi/"))
                return 1;
            if (path.starts_with("worlds/"))
                return path.find('/', 7) == std::string::npos ? 0 : 2;
            return 3;
        }
        const char* group_name(int group)
        {
            static constexpr const char* names[]{"single", "multi", "other", "addon"};
            return names[group];
        }
        std::string json_text(std::string_view text)
        {
            std::string out = "\"";
            for (const char c : text)
            {
                if (c == '"' || c == '\\')
                    out += '\\';
                if (static_cast<unsigned char>(c) < 32)
                {
                    char buffer[8];
                    std::snprintf(buffer, sizeof buffer, "\\u%04x", c);
                    out += buffer;
                    continue;
                }
                out += c;
            }
            return out + '"';
        }
        // Mounted world DATs in the --list-worlds order, with their group.
        std::vector<std::pair<int, std::string>> mounted_worlds(const Resources& resources)
        {
            std::vector<std::pair<int, std::string>> worlds;
            for (const auto& [path, resource] : resources.index())
                if ((path.starts_with("worlds/") || path.starts_with("worlds_ao/")) && path.ends_with(".dat"))
                    worlds.emplace_back(world_group(path), path);
            std::stable_sort(worlds.begin(), worlds.end(),
                             [](const auto& a, const auto& b) { return a.first < b.first; });
            return worlds;
        }
        double elapsed_ms(std::chrono::steady_clock::time_point start)
        {
            return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
        }
    } // namespace

    int run_list_worlds(const Options& options, const Config& config)
    {
        Resources resources;
        std::string error;
        if (!resources.mount_game(config, error))
        {
            spdlog::error("Mount: {}", error);
            return 2;
        }
        const auto worlds = mounted_worlds(resources);
        std::string json = "{\"edition\":" + json_text(gi_edition_name(gi_edition(resources))) + ",\"worlds\":[";
        for (std::size_t i = 0; i < worlds.size(); ++i)
        {
            const auto& [group, path] = worlds[i];
            Sha256Digest digest{};
            std::vector<std::uint8_t> bytes;
            if (!resources.read(path, bytes, error))
            {
                spdlog::error("World {}: {}", path, error);
                return 5;
            }
            digest = sha256(bytes);
            std::string name;
            if (!gi_file_name(path, digest, name))
                name.clear(); // Path longer than the GI limit: listed, but not bakeable.
            if (options.json)
                json += std::string(i ? "," : "") + "{\"path\":" + json_text(path) + ",\"kind\":" +
                        json_text(group_name(group)) + ",\"digest\":" + json_text(digest_text(digest)) +
                        ",\"gi_file\":" + (name.empty() ? std::string("null") : json_text(name)) + "}";
            else
                std::cout << group_name(group) << ' ' << path << ' ' << digest_text(digest) << ' '
                          << (name.empty() ? "-" : name) << '\n';
        }
        if (options.json)
            std::cout << json << "]}\n";
        spdlog::info("{} worlds", worlds.size());
        return 0;
    }

    int run_gi_verify(const Options& options, const Config& config)
    {
        const std::string file_path = path_text(options.gi_verify);
        std::vector<std::uint8_t> bytes;
        std::string error;
        if (!read_gi_bytes(file_path, bytes, error))
        {
            spdlog::error("{}", error.empty() ? "GI file not found: " + file_path : error);
            return 2;
        }
        GiFile file;
        if (!read_gi_file(bytes, file, error))
        {
            std::cout << "status failed\n" << error << '\n';
            return 4;
        }
        const auto& key = file.key;
        std::cout << "world " << key.world_path << "\nworld_digest " << digest_text(key.world) << "\nalbedo "
                  << digest_text(key.albedo) << "\nsettings " << digest_text(key.settings) << "\nplacement "
                  << digest_text(key.placement) << "\nalgorithm " << key.algorithm << "\nedition "
                  << gi_edition_name(key.edition) << "\nsettings_text " << file.settings.size() << " bytes\n";
        for (const auto& section : file.sections)
            std::cout << "section " << gi_tag_name(section.tag) << ' ' << section.payload.size() << " bytes\n";
        Resources resources;
        if (!resources.mount_game(config, error))
        {
            spdlog::error("Mount: {}", error);
            return 2;
        }
        GiExpectation expected;
        if (!resources.find(key.world_path))
        {
            std::cout << "status foreign\nworld not in the mounted data\n";
            return 5;
        }
        if (!gi_expectation(resources, key.world_path, expected, error))
        {
            spdlog::error("World {}: {}", key.world_path, error);
            return 5;
        }
        unsigned stale = 0;
        const auto status = gi_status(key, expected, stale);
        std::cout << "status " << gi_status_name(status) << '\n';
        if (status == GiStatus::Stale)
            std::cout << "stale " << gi_stale_text(stale) << '\n';
        std::string name;
        if (gi_file_name(key.world_path, expected.world, name))
            std::cout << "expected_name " << name << '\n';
        return status == GiStatus::Ready ? 0 : 4;
    }

    int run_gi_place(const Options& options, const Config& config)
    {
        Resources resources;
        std::string error;
        if (!resources.mount_game(config, error))
        {
            spdlog::error("Mount: {}", error);
            return 2;
        }
        std::filesystem::path directory;
        if (!gi_placement_directory(config.gi_placement_dir, directory, error))
        {
            spdlog::error("GI placement directory: {}", error);
            return 2;
        }
        // single: worlds/<name>.dat; all: every mounted world; otherwise one world by name or path.
        std::vector<std::string> targets;
        for (const auto& [group, path] : mounted_worlds(resources))
            if (options.gi_place == "all" || (options.gi_place == "single" && group == 0))
                targets.push_back(path);
        if (options.gi_place != "all" && options.gi_place != "single")
        {
            const bool is_path = options.gi_place.find_first_of("/\\") != std::string::npos ||
                                 options.gi_place.ends_with(".dat") || options.gi_place.ends_with(".DAT");
            std::string path;
            if (!resource_path(is_path ? options.gi_place : "worlds/" + options.gi_place + ".dat", path) ||
                !path.ends_with(".dat") || !resources.find(path))
            {
                spdlog::error("World {} is not mounted", options.gi_place);
                return 2;
            }
            targets.push_back(path);
        }
        int exit_code = 0;
        std::string json = "{\"directory\":" + json_text(path_text(directory)) + ",\"worlds\":[";
        for (std::size_t index = 0; index < targets.size(); ++index)
        {
            const auto& path = targets[index];
            std::vector<std::uint8_t> bytes;
            World world;
            CollisionWorld collision;
            GiPlacementScene scene;
            if (!resources.read(path, bytes, error) || !read_world(bytes, world, error) ||
                !collision.build(world.models.at(0), error) || !scene.build(world, collision, error))
            {
                spdlog::error("World {}: {}", path, error);
                exit_code = 5;
                continue;
            }
            const auto digest = sha256(bytes);
            auto placement = default_gi_placement(path, digest);
            std::string status = "failed", detail;
            GiLightLattice lattice;
            GiPlacementStats stats;
            double light_ms = 0, reflection_ms = 0;
            bool ok = true;
            if (!options.gi_reset)
            {
                const auto loaded = load_gi_placement(directory, path, digest, placement, detail);
                ok = loaded == GiPlacementStatus::Ready || loaded == GiPlacementStatus::Missing;
                if (!ok)
                    detail = std::string("existing placement ") + gi_placement_status_name(loaded) + ": " + detail +
                             " (--gi-reset replaces it)";
                if (loaded == GiPlacementStatus::Missing)
                    placement = default_gi_placement(path, digest);
            }
            if (ok)
            {
                auto start = std::chrono::steady_clock::now();
                ok = resolve_light_probes(scene, placement, lattice, stats, nullptr, detail);
                light_ms = elapsed_ms(start);
                start = std::chrono::steady_clock::now();
                ok = ok && place_reflection_probes(scene, lattice, placement, stats, detail);
                reflection_ms = elapsed_ms(start);
            }
            std::string name;
            gi_placement_name(path, name);
            const auto file = directory / path_from_utf8(name);
            std::string text;
            if (ok && !write_gi_placement(placement, text, detail))
                ok = false;
            if (ok)
            {
                std::vector<std::uint8_t> existing;
                std::string ignored;
                const bool same = read_file_bounded(file, gi_placement_limits::max_text, existing, ignored) &&
                                  std::string_view(reinterpret_cast<const char*>(existing.data()), existing.size()) ==
                                      text;
                if (same)
                    status = "unchanged";
                else if (options.gi_dry_run)
                    status = "dry-run";
                else if (save_gi_placement(directory, placement, detail))
                    status = "written";
                else
                    ok = false;
            }
            if (!ok)
            {
                status = "failed";
                exit_code = std::max(exit_code, 4);
            }
            const std::pair<const char*, std::uint64_t> counts[]{
                {"spacing", lattice.spacing},          {"probes", stats.probes},
                {"relocated", stats.relocated},        {"nodes", stats.nodes},
                {"excluded", stats.excluded},          {"solid", stats.solid},
                {"unknown", stats.unknown},            {"clearance", stats.clearance},
                {"world_model", stats.world_model},    {"reflections", placement.reflections.size()},
                {"manual", stats.manual},              {"open", stats.open},
                {"rooms", stats.room_candidates},      {"small_rooms", stats.small_rooms},
                {"overlapping", stats.overlapping},    {"capped", stats.capped}};
            if (options.json)
            {
                json += std::string(index ? "," : "") + "{\"world\":" + json_text(path) +
                        ",\"status\":" + json_text(status);
                for (const auto& [key, value] : counts)
                    json += ",\"" + std::string(key) + "\":" + std::to_string(value);
                json += ",\"light_ms\":" + std::to_string(std::lround(light_ms)) +
                        ",\"reflection_ms\":" + std::to_string(std::lround(reflection_ms)) +
                        ",\"file\":" + json_text(path_text(file)) + ",\"detail\":" + json_text(detail) + "}";
            }
            else
            {
                std::cout << "GI_PLACE " << path << " status=" << status;
                for (const auto& [key, value] : counts)
                    std::cout << ' ' << key << '=' << value;
                std::cout << " light_ms=" << std::lround(light_ms) << " reflection_ms=" << std::lround(reflection_ms)
                          << " file=" << path_text(file) << '\n';
                if (!detail.empty())
                    std::cout << "  " << detail << '\n';
            }
        }
        if (options.json)
            std::cout << json << "]}\n";
        spdlog::info("GI placement: {} world(s), directory {}", targets.size(), path_text(directory));
        return exit_code;
    }
} // namespace OpenBlood2
