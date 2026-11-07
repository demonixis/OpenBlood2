#include "app/gi_tool.h"
#include "app/scene_presenter.h"
#include "engine/atomic_file.h"
#include "engine/gi_bake.h"
#include "engine/gi_direct.h"
#include "engine/gi_jobs.h"
#include "engine/gi_file.h"
#include "engine/gi_placement.h"
#include "engine/gi_preview.h"
#include "engine/resources.h"
#include "engine/user_directory.h"
#include <SDL3/SDL_surface.h>
#include <spdlog/spdlog.h>
#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdio>
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
        // A world by name (worlds/<name>.dat) or resource path; false when it is not a mounted DAT.
        bool mounted_world(const Resources& resources, const std::string& world, std::string& path)
        {
            const bool is_path = world.find_first_of("/\\") != std::string::npos || world.ends_with(".dat") ||
                                 world.ends_with(".DAT");
            return resource_path(is_path ? world : "worlds/" + world + ".dat", path) && path.ends_with(".dat") &&
                   resources.find(path);
        }
        std::atomic<bool> bake_cancel{false};
        static_assert(std::atomic<bool>::is_always_lock_free); // Written from a signal handler.
        extern "C" void on_bake_signal(int)
        {
            bake_cancel.store(true);
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
            std::string path;
            if (!mounted_world(resources, options.gi_place, path))
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

    int run_gi_bake(const Options& options, const Config& config)
    {
        Resources resources;
        std::string error;
        if (!resources.mount_game(config, error))
        {
            spdlog::error("Mount: {}", error);
            std::cout << "GI_RESULT status=failed exit=2 error=" << json_text(error) << '\n';
            return 2;
        }
        GiBakeRequest request;
        if (!mounted_world(resources, options.bake_gi, request.world))
            request.world = options.bake_gi; // Reported as unmounted by the bake (exit 2).
        if (!make_gi_bake_settings(options.gi_quality, options.gi_bounces, options.gi_rays, options.gi_sky,
                                   request.settings, error))
        {
            spdlog::error("{}", error);
            std::cout << "GI_RESULT status=failed exit=2 error=" << json_text(error) << '\n';
            return 2;
        }
        request.jobs = options.gi_jobs ? options.gi_jobs : gi_default_jobs();
        request.out_dir = options.gi_out;
        request.placement_dir = config.gi_placement_dir;
        request.if_stale = options.gi_if_stale;
        request.cancel = &bake_cancel;
        request.progress = [](std::string_view stage, std::uint64_t done, std::uint64_t total)
        { std::cout << "GI_PROGRESS stage=" << stage << " done=" << done << " total=" << total << std::endl; };
        bake_cancel.store(false);
        const auto old_int = std::signal(SIGINT, on_bake_signal);
        const auto old_term = std::signal(SIGTERM, on_bake_signal);
        const auto result = bake_gi_world(resources, request);
        std::signal(SIGINT, old_int == SIG_ERR ? SIG_DFL : old_int);
        std::signal(SIGTERM, old_term == SIG_ERR ? SIG_DFL : old_term);

        const auto& survey = result.survey;
        const auto& scene = result.scene;
        std::cout << "GI_RESULT status=" << gi_bake_status_name(result.status) << " exit=" << result.exit_code
                  << " world=" << request.world << " quality=" << gi_quality_name(request.settings.quality)
                  << " bounces=" << request.settings.bounces << " rays_per_probe=" << request.settings.rays
                  << " sky_scale=" << request.settings.sky / 100 << '.' << request.settings.sky / 10 % 10
                  << request.settings.sky % 10 << " jobs=" << request.jobs;
        if (result.status == GiBakeResult::Status::Failed || result.status == GiBakeResult::Status::Cancelled)
            std::cout << " error=" << json_text(result.error);
        if (!result.file.empty())
            std::cout << " file=" << path_text(result.file);
        if (result.status == GiBakeResult::Status::Baked)
        {
            const double seconds = survey.ms / 1000;
            const double rate = seconds > 0 ? double(survey.rays) / seconds : 0;
            std::cout << " placement=" << gi_placement_status_name(result.placement) << " probes=" << result.probes
                      << " reflections=" << result.reflections << " faces=" << scene.main_faces
                      << " sky_faces=" << scene.sky_faces << " translucent=" << scene.translucent
                      << " unresolved=" << scene.unresolved << " excluded=" << scene.excluded_ranges
                      << " nodes=" << scene.nodes << " depth=" << scene.depth << " threads=" << survey.threads
                      << " rays=" << survey.rays << " rays_per_second=" << std::llround(rate)
                      << " rays_per_second_per_thread=" << std::llround(survey.threads ? rate / survey.threads : 0)
                      << " front=" << survey.front << " back=" << survey.back << " sky=" << survey.sky
                      << " miss=" << survey.miss << " layer_limit=" << survey.layer_limit
                      << " layers=" << survey.layers << " inside=" << survey.inside
                      << " digest=" << digest_text(survey.digest) << " scene_ms=" << std::lround(scene.build_ms)
                      << " survey_ms=" << std::lround(survey.ms);
            const auto& sky = result.sky;
            const auto& setup = result.sky_setup;
            char covered[32], average[96];
            std::snprintf(covered, sizeof covered, "%.3f", sky.covered);
            std::snprintf(average, sizeof average, "%.3f,%.3f,%.3f", sky.average.x, sky.average.y, sky.average.z);
            std::cout << " sky_slots=" << sky.slots << " sky_triangles=" << sky.triangles
                      << " sky_covered=" << covered << " sky_average=" << average << " sky_pointers=" << setup.pointers
                      << " sky_unresolved=" << setup.unresolved_pointers << " sky_fog=" << setup.fog.enabled
                      << " sky_ms=" << std::lround(sky.ms);
            const auto& maps = result.lightmaps;
            std::cout << " blocks=" << maps.blocks << " texels=" << maps.texels << " sets=" << maps.sets
                      << " corners=" << maps.corners << " low_corners=" << maps.low_corners << " solid=" << maps.solid
                      << " back_invalid=" << maps.back_invalid << " filled=" << maps.filled
                      << " neighbour_filled=" << maps.neighbour_filled << " unfilled=" << maps.unfilled
                      << " lightmap_rays=" << maps.rays << " lightmap_back=" << maps.back
                      << " lightmap_sky=" << maps.sky << " lightmap_untextured=" << maps.untextured
                      << " lightmap_layer_limit=" << maps.layer_limit << " back_fraction=";
            for (std::size_t i = 0; i < maps.back_fraction.size(); ++i)
                std::cout << (i ? "," : "") << maps.back_fraction[i];
            std::cout << " pass_ms=";
            for (std::size_t i = 0; i < maps.pass_ms.size(); ++i)
                std::cout << (i ? "," : "") << std::lround(maps.pass_ms[i]);
            std::cout << " lightmap_ms=" << std::lround(maps.ms);
            const auto& probes = result.light_probes;
            std::cout << " probe_valid=" << probes.valid << " probe_invalid=" << probes.probes - probes.valid
                      << " probe_rays=" << probes.rays << " probe_back=" << probes.back
                      << " probe_sky=" << probes.sky << " probe_back_fraction=";
            for (std::size_t i = 0; i < probes.back_fraction.size(); ++i)
                std::cout << (i ? "," : "") << probes.back_fraction[i];
            std::cout << " probe_ms=" << std::lround(probes.ms);
        }
        if (result.status == GiBakeResult::Status::Baked || result.status == GiBakeResult::Status::Current)
            std::cout << " bytes=" << result.bytes;
        std::cout << " ms=" << std::lround(result.ms) << std::endl;
        if (result.exit_code && result.exit_code != 12)
            spdlog::error("GI bake {}: {}", request.world, result.error);
        return result.exit_code;
    }

    int run_gi_preview(const Options& options, const Config& config)
    {
        auto fail = [](int code, const std::string& error)
        {
            spdlog::error("{}", error);
            std::cout << "GI_PREVIEW_RESULT status=failed exit=" << code << " error=" << json_text(error) << '\n';
            return code;
        };
        Resources resources;
        std::string error;
        if (!resources.mount_game(config, error))
            return fail(2, "Mount: " + error);
        std::string path;
        if (!mounted_world(resources, options.gi_preview, path))
            return fail(2, "World " + options.gi_preview + " is not mounted");
        if (options.gi_jobs > 256)
            return fail(2, "--gi-jobs must be 1 to 256");
        const unsigned jobs = options.gi_jobs ? options.gi_jobs : gi_default_jobs();
        std::vector<std::uint8_t> bytes;
        World world;
        if (!resources.read(path, bytes, error) || !read_world(bytes, world, error))
            return fail(5, "World " + path + ": " + error);
        const auto digest = sha256(bytes);
        GiExpectation expected;
        if (!gi_expectation(resources, path, expected, error))
            return fail(5, "World " + path + ": " + error);
        std::string name;
        if (!gi_file_name(path, digest, name))
            return fail(2, "World path too long for a GI file: " + path);

        // The baked file: valid and Ready for this content (any bake settings and placement).
        std::filesystem::path out = options.gi_out;
        if (out.empty())
        {
            if (!user_directory(out, error))
                return fail(2, "GI output directory: " + error);
            out /= "gi";
        }
        const auto file_path = out / path_from_utf8(name);
        std::vector<std::uint8_t> encoded;
        GiFile file;
        if (!read_gi_bytes(path_text(file_path), encoded, error) || !read_gi_file(encoded, file, error))
            return fail(4, "GI file " + path_text(file_path) + ": " + error);
        unsigned stale = 0;
        const auto status = gi_status(file.key, expected, stale);
        if (status != GiStatus::Ready)
            return fail(4, "GI file " + path_text(file_path) + " is " + gi_status_name(status) +
                               (stale ? " (" + gi_stale_text(stale) + ")" : std::string()));
        const auto* lmap = file.find(gi_format::lightmaps);
        const auto* vtx = file.find(gi_format::vertices);
        if (!lmap || !vtx)
            return fail(4, "GI file " + path_text(file_path) + " has no LMAP/VTX sections");

        // The bake scene, as the bake builds it (the placement names the excluded models).
        std::filesystem::path placement_dir;
        if (!gi_placement_directory(config.gi_placement_dir, placement_dir, error))
            return fail(2, "GI placement directory: " + error);
        auto placement = default_gi_placement(path, digest);
        std::string detail;
        const auto placement_status = load_gi_placement(placement_dir, path, digest, placement, detail);
        if (placement_status == GiPlacementStatus::Foreign || placement_status == GiPlacementStatus::Failed)
            return fail(4, std::string("Placement ") + gi_placement_status_name(placement_status) + ": " + detail);
        if (placement_status == GiPlacementStatus::Missing)
            placement = default_gi_placement(path, digest);
        WorldMesh mesh;
        std::vector<WorldTriangleSource> sources;
        if (!mesh.build(world, error, &resources, true, &sources))
            return fail(5, "World mesh: " + error);
        std::vector<GiRangeUse> use;
        GiSceneReport scene_report;
        if (!gi_range_uses(world, placement.excluded_models, use, scene_report, error))
            return fail(4, "Placement: " + error);
        GiBakeScene scene;
        if (!scene.build(std::move(mesh), use, scene_report, error))
            return fail(5, "GI scene: " + error);
        GiSkyReport sky_report;
        GiSky sky;
        if (!sky.build(world, scene, gi_sky_setup(world), 1, jobs, sky_report, error))
            return fail(4, "GI sky: " + error);
        GiReceivers receivers;
        if (!gi_receivers(world, scene, sources, use, jobs, receivers, error))
            return fail(4, error);
        GiLightmaps maps;
        GiVertexLight vertex;
        GiIndirect indirect;
        if (!read_gi_lightmaps(lmap->payload, maps, error) || !read_gi_vertex_light(vtx->payload, vertex, error) ||
            !check_gi_lightmaps(world, maps, error) || !check_gi_vertex_light(world, vertex, error) ||
            !gi_indirect_from_sections(receivers, maps, vertex, indirect, error))
            return fail(4, "GI file " + path_text(file_path) + ": " + error);
        const auto values = gi_preview_values(receivers, indirect, options.gi_k, options.gi_f);

        // Mean E over valid texels and corners, and how much the composition changes the stored light
        // (8-bit channels of every element, DETAIL LOW corners included), for comparing bakes.
        double sums[2][3]{}, delta = 0;
        std::size_t counts[2]{}, changed = 0;
        for (std::size_t e = 0; e < receivers.elements(); ++e)
        {
            const int kind = e < receivers.texels ? 0 : 1;
            for (int c = 0; c < 3; ++c)
            {
                const auto o = receivers.original[e][c];
                const float ambient = c == 0 ? receivers.ambient.x : c == 1 ? receivers.ambient.y : receivers.ambient.z;
                const int d = int(gi_compose(o, ambient, indirect.energy[e * 3 + c], options.gi_k, options.gi_f,
                                             kind == 0)) - o;
                changed += d != 0;
                delta += d;
            }
            if (!indirect.valid[e])
                continue;
            ++counts[kind];
            for (int c = 0; c < 3; ++c)
                sums[kind][c] += indirect.energy[e * 3 + c];
        }
        const double channels = 3.0 * double(std::max<std::size_t>(receivers.elements(), 1));

        std::filesystem::path preview_dir = options.gi_preview_dir;
        if (preview_dir.empty())
        {
            if (!user_directory(preview_dir, error))
                return fail(2, "GI preview directory: " + error);
            preview_dir /= "gi";
            preview_dir /= "preview";
        }
        auto views = gi_preview_views(world);
        if (views.empty())
        {
            // No start point: the centre of the world, looking along +Z and turned twice.
            const DVector centre{(world.min.x + world.max.x) / 2, (world.min.y + world.max.y) / 2,
                                 (world.min.z + world.max.z) / 2};
            views.push_back({centre, {0, 0, 1}, "centre"});
            views.push_back({centre, {.8660254f, 0, -.5f}, "centre+120"});
            views.push_back({centre, {-.8660254f, 0, -.5f}, "centre+240"});
        }
        constexpr unsigned width = 480, height = 360;
        auto stem = name.substr(0, name.size() - std::string_view(".ob2gi").size());
        std::vector<std::uint8_t> rgba;
        for (const auto& view : views)
        {
            const auto start = std::chrono::steady_clock::now();
            render_gi_preview(scene, sky, receivers, values, view, width, height, jobs, rgba);
            const auto png = preview_dir / path_from_utf8(stem + "." + view.label + ".png");
            std::error_code code;
            std::filesystem::create_directories(png.parent_path(), code);
            if (code)
                return fail(4, "GI preview directory: " + code.message());
            auto* surface = SDL_CreateSurfaceFrom(int(width * 3), int(height), SDL_PIXELFORMAT_RGBA32, rgba.data(),
                                                  int(width * 3 * 4));
            const bool saved = surface && SDL_SavePNG(surface, path_text(png).c_str());
            if (!saved)
                error = SDL_GetError();
            SDL_DestroySurface(surface);
            if (!saved)
                return fail(4, "GI preview " + path_text(png) + ": " + error);
            std::cout << "GI_PREVIEW view=" << view.label << " eye=" << view.eye.x << ',' << view.eye.y << ','
                      << view.eye.z << " ms=" << std::lround(elapsed_ms(start)) << " file=" << json_text(path_text(png))
                      << std::endl;
        }
        char numbers[384];
        std::snprintf(numbers, sizeof numbers,
                      " k=%.2f f=%.2f ambient=%.0f,%.0f,%.0f texel_e=%.4f,%.4f,%.4f corner_e=%.4f,%.4f,%.4f"
                      " valid_texels=%zu valid_corners=%zu changed=%.4f mean_delta=%.3f indirect_exposure=%.0f",
                      options.gi_k, options.gi_f, receivers.ambient.x, receivers.ambient.y, receivers.ambient.z,
                      counts[0] ? sums[0][0] / counts[0] : 0, counts[0] ? sums[0][1] / counts[0] : 0,
                      counts[0] ? sums[0][2] / counts[0] : 0, counts[1] ? sums[1][0] / counts[1] : 0,
                      counts[1] ? sums[1][1] / counts[1] : 0, counts[1] ? sums[1][2] / counts[1] : 0, counts[0],
                      counts[1], double(changed) / channels, delta / channels, double(gi_preview_indirect_exposure));
        std::string settings;
        for (std::size_t at = file.settings.find('\n'); at != std::string::npos && at + 1 < file.settings.size();)
        {
            const auto end = file.settings.find('\n', at + 1);
            auto line = file.settings.substr(at + 1, end - at - 1);
            if (const auto space = line.find(' '); space != std::string::npos)
                line[space] = '=';
            settings += ' ' + line;
            at = end;
        }
        std::cout << "GI_PREVIEW_RESULT status=ok exit=0 world=" << path << " views=" << views.size() << numbers
                  << settings << " texels=" << receivers.texels << " corners=" << receivers.corners << '\n';
        return 0;
    }

    namespace
    {
        std::string fit_json(const GiDirectFit& fit)
        {
            char buffer[256];
            std::snprintf(buffer, sizeof buffer,
                          "{\"texels\":%zu,\"fit\":%.4f,\"fit_step\":%.4f,\"mean_abs\":%.3f,\"brighter\":%.4f,"
                          "\"darker\":%.4f}",
                          fit.texels, fit.share(fit.fit), fit.share(fit.fit_step), fit.mean_abs(),
                          fit.share(fit.brighter), fit.share(fit.darker));
            return buffer;
        }
        void merge_fit(GiDirectFit& into, const GiDirectFit& fit)
        {
            into.texels += fit.texels;
            into.fit += fit.fit;
            into.fit_step += fit.fit_step;
            into.brighter += fit.brighter;
            into.darker += fit.darker;
            into.abs_sum += fit.abs_sum;
        }
        // Writes one RGBA preview strip as a PNG.
        bool save_png(const std::filesystem::path& png, unsigned width, unsigned height,
                      std::vector<std::uint8_t>& rgba, std::string& error)
        {
            std::error_code code;
            std::filesystem::create_directories(png.parent_path(), code);
            if (code)
            {
                error = code.message();
                return false;
            }
            auto* surface =
                SDL_CreateSurfaceFrom(int(width), int(height), SDL_PIXELFORMAT_RGBA32, rgba.data(), int(width * 4));
            const bool saved = surface && SDL_SavePNG(surface, path_text(png).c_str());
            if (!saved)
                error = SDL_GetError();
            SDL_DestroySurface(surface);
            return saved;
        }
    } // namespace

    int run_gi_direct_audit(const Options& options, const Config& config)
    {
        auto fail = [](int code, const std::string& error)
        {
            spdlog::error("{}", error);
            std::cout << "{\"status\":\"failed\",\"exit\":" << code << ",\"error\":" << json_text(error) << "}\n";
            return code;
        };
        Resources resources;
        std::string error;
        if (!resources.mount_game(config, error))
            return fail(2, "Mount: " + error);
        if (options.gi_jobs > 256)
            return fail(2, "--gi-jobs must be 1 to 256");
        const unsigned jobs = options.gi_jobs ? options.gi_jobs : gi_default_jobs();
        const auto& target = options.gi_direct_audit;
        std::vector<std::string> targets;
        for (const auto& [group, path] : mounted_worlds(resources))
            if (target == "all" || (target == "single" && group == 0))
                targets.push_back(path);
        if (target != "all" && target != "single")
        {
            std::string path;
            if (!mounted_world(resources, target, path))
                return fail(2, "World " + target + " is not mounted");
            targets.push_back(path);
        }
        std::filesystem::path preview_dir = options.gi_preview_dir;
        std::string json = "{\"status\":\"ok\",\"term\":\"ambient + BrightScale (Light: color f + outer (1 - f); "
                           "DirLight: (inner a + outer (1 - a)) f), f = 1 - d/R\",\"tolerance\":[11,7,11],\"worlds\":[";
        GiDirectFit total, total_edge, total_interior;
        std::size_t total_lights = 0;
        for (std::size_t index = 0; index < targets.size(); ++index)
        {
            const auto& path = targets[index];
            const auto start = std::chrono::steady_clock::now();
            std::vector<std::uint8_t> bytes;
            World world;
            if (!resources.read(path, bytes, error) || !read_world(bytes, world, error))
                return fail(5, "World " + path + ": " + error);
            std::vector<GiDirectLight> lights;
            GiDirectLightReport light_report;
            if (!gi_direct_lights(world, lights, light_report, error))
                return fail(4, "World " + path + ": " + error);
            WorldMesh mesh;
            std::vector<WorldTriangleSource> sources;
            if (!mesh.build(world, error, &resources, true, &sources))
                return fail(5, "World mesh " + path + ": " + error);
            std::vector<GiRangeUse> use;
            GiSceneReport scene_report;
            if (!gi_range_uses(world, {}, use, scene_report, error))
                return fail(4, "World " + path + ": " + error);
            GiBakeScene scene;
            if (!scene.build(std::move(mesh), use, scene_report, error))
                return fail(5, "GI scene " + path + ": " + error);
            GiReceivers receivers;
            if (!gi_receivers(world, scene, sources, use, jobs, receivers, error))
                return fail(4, "World " + path + ": " + error);
            GiDirectOccluders occluders;
            if (!occluders.build(world, error))
                return fail(4, "World " + path + ": " + error);
            GiDirectTexels direct;
            if (!gi_direct_texels(world, receivers, lights, occluders, jobs, direct, error))
                return fail(4, "World " + path + ": " + error);
            GiDirectAudit audit;
            gi_direct_audit(world, receivers, direct, audit);
            merge_fit(total, audit.all);
            merge_fit(total_edge, audit.edge);
            merge_fit(total_interior, audit.interior);
            total_lights += light_report.lights;

            std::string views;
            if (!preview_dir.empty())
            {
                GiSkyReport sky_report;
                GiSky sky;
                if (!sky.build(world, scene, gi_sky_setup(world), 1, jobs, sky_report, error))
                    return fail(4, "GI sky " + path + ": " + error);
                GiPreviewValues values;
                const auto n = receivers.elements();
                values.original.resize(n * 3);
                values.composed.resize(n * 3);
                for (std::size_t e = 0; e < n; ++e)
                    for (int c = 0; c < 3; ++c)
                    {
                        values.original[e * 3 + c] = receivers.original[e][c] / 255.f;
                        // Decision D18: vertex-lit faces keep their stored colours.
                        values.composed[e * 3 + c] =
                            e < receivers.texels ? direct.value(e, c) / 255.f : values.original[e * 3 + c];
                    }
                auto list = gi_preview_views(world);
                if (list.size() > 3)
                    list.resize(3);
                std::string name;
                if (!gi_file_name(path, sha256(bytes), name))
                    return fail(2, "World path too long for a GI file: " + path);
                const auto stem = name.substr(0, name.size() - std::string_view(".ob2gi").size());
                constexpr unsigned width = 480, height = 360;
                std::vector<std::uint8_t> rgba;
                for (std::size_t v = 0; v < list.size(); ++v)
                {
                    render_gi_preview(scene, sky, receivers, values, list[v], width, height, jobs, rgba,
                                      GiPreviewThird::Difference);
                    const auto png = preview_dir / path_from_utf8(stem + ".direct." + list[v].label + ".png");
                    if (!save_png(png, width * 3, height, rgba, error))
                        return fail(4, "GI preview " + path_text(png) + ": " + error);
                    views += std::string(v ? "," : "") + json_text(path_text(png));
                }
            }
            const auto& l = light_report;
            char head[640];
            std::snprintf(head, sizeof head,
                          "{\"path\":%s,\"lights\":{\"lights\":%zu,\"omni\":%zu,\"cones\":%zu,\"unclipped\":%zu,"
                          "\"in_solid\":%zu,\"negative\":%zu,\"cone_outer\":%zu,\"omni_outer\":%zu,"
                          "\"object_lights\":%zu,\"global_dir_lights\":%zu,\"invalid\":%zu},"
                          "\"ambient\":[%.0f,%.0f,%.0f],\"blocks\":%zu,\"solid_texels\":%zu,\"tests\":%llu,"
                          "\"direct_ms\":%.0f,\"ms\":%.0f,",
                          json_text(path).c_str(), l.lights, l.omni, l.cones, l.unclipped, l.in_solid, l.negative,
                          l.cone_outer, l.omni_outer, l.object_lights, l.global_dir_lights, l.invalid,
                          receivers.ambient.x, receivers.ambient.y, receivers.ambient.z, receivers.blocks.size(),
                          audit.solid_texels, static_cast<unsigned long long>(direct.tests), direct.ms,
                          elapsed_ms(start));
            std::string entry = head;
            entry += "\"all\":" + fit_json(audit.all) + ",\"interior\":" + fit_json(audit.interior) +
                     ",\"edge\":" + fit_json(audit.edge) + ",\"primary\":" + fit_json(audit.primary) +
                     ",\"world_models\":" + fit_json(audit.world_models) + ",\"by_lights\":{";
            static constexpr const char* groups[4]{"0", "1", "2-3", "4+"};
            for (int g = 0; g < 4; ++g)
                entry += std::string(g ? "," : "") + "\"" + groups[g] + "\":" + fit_json(audit.by_lights[g]);
            entry += "},\"by_flags\":{";
            for (std::size_t f = 0; f < audit.by_flags.size(); ++f)
            {
                char flags[16];
                std::snprintf(flags, sizeof flags, "0x%x", audit.by_flags[f].first);
                entry += std::string(f ? "," : "") + "\"" + flags + "\":" + fit_json(audit.by_flags[f].second);
            }
            entry += "},\"views\":[" + views + "]}";
            json += std::string(index ? "," : "") + entry;
            spdlog::info("Direct audit {}: fit {:.1f}% over {} texels", path, 100 * audit.all.share(audit.all.fit),
                         audit.all.texels);
        }
        json += "],\"total\":{\"worlds\":" + std::to_string(targets.size()) +
                ",\"lights\":" + std::to_string(total_lights) + ",\"all\":" + fit_json(total) +
                ",\"interior\":" + fit_json(total_interior) + ",\"edge\":" + fit_json(total_edge) + "}}";
        std::cout << json << '\n';
        return 0;
    }

    void log_gi_status(const ScenePresenter& presenter, std::uint64_t& logged)
    {
        const auto& status = presenter.gi_status();
        if (status.loads != logged)
        {
            logged = status.loads;
            if (status.state != GiRuntimeState::NoWorld)
                spdlog::info("{}", gi_status_line(status));
        }
        if (presenter.rebuilt && presenter.world_mesh.gi_blocks + presenter.world_mesh.gi_sets)
            spdlog::info("GI composed ({}, k {:.2f}, f {:.2f}): {} blocks, {} texels, {} corner sets; world mesh "
                         "{:.1f} ms",
                         gi_view_name(presenter.gi_composition.view), presenter.gi_composition.k,
                         presenter.gi_composition.f, presenter.world_mesh.gi_blocks, presenter.world_mesh.gi_texels,
                         presenter.world_mesh.gi_sets, presenter.mesh_build_ms);
    }
} // namespace OpenBlood2
