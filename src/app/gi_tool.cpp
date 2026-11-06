#include "app/gi_tool.h"
#include "engine/gi_file.h"
#include "engine/resources.h"
#include <spdlog/spdlog.h>
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
        std::vector<std::pair<int, std::string>> worlds;
        for (const auto& [path, resource] : resources.index())
            if ((path.starts_with("worlds/") || path.starts_with("worlds_ao/")) && path.ends_with(".dat"))
                worlds.emplace_back(world_group(path), path);
        std::stable_sort(worlds.begin(), worlds.end(),
                         [](const auto& a, const auto& b) { return a.first < b.first; });
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
} // namespace OpenBlood2
