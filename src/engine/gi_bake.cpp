#include "engine/gi_bake.h"
#include "engine/gi_jobs.h"
#include "engine/user_directory.h"
#include <cfloat>
#include <chrono>
#include <cmath>
#include <cstring>
#include <mutex>

namespace OpenBlood2
{
    namespace S = gi_bake_settings_limits;
    namespace
    {
        double elapsed_ms(std::chrono::steady_clock::time_point start)
        {
            return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
        }
        struct ProbeRecord
        {
            std::uint32_t front = 0, back = 0, sky = 0, miss = 0, layer_limit = 0, layers = 0;
            std::uint64_t hash = 0xcbf29ce484222325ull; // FNV-1a over every ray's hit.
            GiTraceStats trace;
            void mix(const void* data, std::size_t size)
            {
                const auto* bytes = static_cast<const std::uint8_t*>(data);
                for (std::size_t i = 0; i < size; ++i)
                    hash = (hash ^ bytes[i]) * 0x100000001b3ull;
            }
        };
        void put32(std::vector<std::uint8_t>& out, std::uint32_t value)
        {
            for (int i = 0; i < 4; ++i)
                out.push_back(static_cast<std::uint8_t>(value >> (8 * i)));
        }
    } // namespace

    DVector gi_uniform_direction(std::uint64_t stream, std::uint64_t index)
    {
        // Rejection from the cube: + - * / and sqrt only (correctly rounded), so every build and
        // optimization level gives the same bits; library sin/cos may not.
        GiRandom random(stream, index);
        for (;;)
        {
            const float x = 2 * random.uniform() - 1, y = 2 * random.uniform() - 1, z = 2 * random.uniform() - 1;
            const float r2 = x * x + y * y + z * z;
            if (r2 > 1e-6f && r2 <= 1)
            {
                const float scale = 1 / std::sqrt(r2);
                return {x * scale, y * scale, z * scale};
            }
        }
    }
    const char* gi_quality_name(GiQuality quality)
    {
        switch (quality)
        {
        case GiQuality::Preview:
            return "preview";
        case GiQuality::Final:
            return "final";
        default:
            return "custom";
        }
    }
    bool make_gi_bake_settings(std::string_view quality, std::uint32_t bounces, std::uint32_t rays, std::int32_t sky,
                               GiBakeSettings& settings, std::string& error)
    {
        GiBakeSettings out;
        if (quality == "preview")
            out = {GiQuality::Preview, 1, 64, 100};
        else if (quality == "final" || quality.empty())
            out = {GiQuality::Final, 2, 256, 100};
        else if (quality == "custom")
        {
            out.quality = GiQuality::Custom;
            if (bounces)
                out.bounces = bounces;
            if (rays)
                out.rays = rays;
            if (sky >= 0)
                out.sky = static_cast<std::uint32_t>(sky);
            if (sky < -1 || out.sky > S::max_sky)
            {
                error = "--gi-sky must be 0 to 4";
                return false;
            }
            if (out.bounces < S::min_bounces || out.bounces > S::max_bounces)
            {
                error = "--gi-bounces must be 1 to 3";
                return false;
            }
            if (out.rays < S::min_rays || out.rays > S::max_rays)
            {
                error = "--gi-rays must be 16 to 4096";
                return false;
            }
        }
        else
        {
            error = "--gi-quality must be preview, final or custom";
            return false;
        }
        if (out.quality != GiQuality::Custom && (bounces || rays || sky >= 0))
        {
            error = "--gi-bounces, --gi-rays and --gi-sky need --gi-quality custom";
            return false;
        }
        settings = out;
        return true;
    }
    std::string gi_settings_text(const GiBakeSettings& settings)
    {
        const auto sky = std::to_string(settings.sky / 100) + "." + char('0' + settings.sky / 10 % 10) +
                         char('0' + settings.sky % 10);
        return "openblood2-gi-settings 2\nquality " + std::string(gi_quality_name(settings.quality)) + "\nbounces " +
               std::to_string(settings.bounces) + "\nrays " + std::to_string(settings.rays) + "\nsky " + sky + "\n";
    }

    GiSurvey survey_gi_probes(const GiBakeScene& scene, std::span<const GiLightProbe> probes, std::uint32_t rays,
                              unsigned jobs, const std::atomic<bool>* cancel, const GiProgress& progress)
    {
        const auto start = std::chrono::steady_clock::now();
        GiSurvey survey;
        survey.probes = probes.size();
        std::vector<ProbeRecord> records(probes.size());
        std::atomic<std::uint64_t> done{0};
        std::mutex report;
        std::uint64_t reported = 0;
        const auto total = probes.size();
        const auto run = run_gi_jobs(
            probes.size(), jobs ? jobs : gi_default_jobs(), cancel,
            [&](std::uint64_t index, unsigned)
            {
                auto& record = records[index];
                const auto origin = probes[index].position;
                GiHit hit;
                for (std::uint32_t ray = 0; ray < rays; ++ray)
                {
                    scene.trace(origin, gi_uniform_direction(index, ray), FLT_MAX, hit, record.trace);
                    switch (hit.kind)
                    {
                    case GiHit::Kind::Miss:
                        ++record.miss;
                        break;
                    case GiHit::Kind::Sky:
                        ++record.sky;
                        break;
                    case GiHit::Kind::LayerLimit:
                        ++record.layer_limit;
                        break;
                    default:
                        ++(hit.back ? record.back : record.front);
                    }
                    record.layers += hit.layer_count;
                    const std::uint8_t kind[2]{static_cast<std::uint8_t>(hit.kind), hit.back};
                    record.mix(kind, sizeof kind);
                    record.mix(&hit.face, sizeof hit.face);
                    record.mix(&hit.t, sizeof hit.t);
                    record.mix(&hit.layer_count, sizeof hit.layer_count);
                }
                const auto finished = done.fetch_add(1) + 1;
                if (progress && (finished * 20 / total != (finished - 1) * 20 / total || finished == total))
                {
                    std::lock_guard lock(report);
                    if (finished > reported)
                    {
                        reported = finished;
                        progress("survey", finished, total);
                    }
                }
            });
        survey.threads = run.threads;
        survey.cancelled = run.cancelled;
        Sha256 digest;
        std::vector<std::uint8_t> bytes;
        for (const auto& record : records)
        {
            survey.front += record.front;
            survey.back += record.back;
            survey.sky += record.sky;
            survey.miss += record.miss;
            survey.layer_limit += record.layer_limit;
            survey.layers += record.layers;
            survey.inside += 2ull * record.back > rays;
            survey.trace.rays += record.trace.rays;
            survey.trace.nodes += record.trace.nodes;
            survey.trace.triangles += record.trace.triangles;
            survey.trace.texels += record.trace.texels;
            bytes.clear();
            for (const auto value : {record.front, record.back, record.sky, record.miss, record.layer_limit,
                                     record.layers})
                put32(bytes, value);
            put32(bytes, static_cast<std::uint32_t>(record.hash));
            put32(bytes, static_cast<std::uint32_t>(record.hash >> 32));
            digest.update(bytes);
        }
        survey.rays = survey.trace.rays;
        survey.digest = digest.finish();
        survey.ms = elapsed_ms(start);
        return survey;
    }

    const char* gi_bake_status_name(GiBakeResult::Status status)
    {
        switch (status)
        {
        case GiBakeResult::Status::Baked:
            return "baked";
        case GiBakeResult::Status::Current:
            return "current";
        case GiBakeResult::Status::Cancelled:
            return "cancelled";
        default:
            return "failed";
        }
    }

    GiBakeResult bake_gi_world(const Resources& resources, const GiBakeRequest& request)
    {
        const auto start = std::chrono::steady_clock::now();
        GiBakeResult result;
        auto fail = [&](int code, std::string error)
        {
            result.status = GiBakeResult::Status::Failed;
            result.exit_code = code;
            result.error = std::move(error);
            result.ms = elapsed_ms(start);
            return result;
        };
        auto cancelled = [&]
        {
            result.status = GiBakeResult::Status::Cancelled;
            result.exit_code = 12;
            result.error = "cancelled";
            result.ms = elapsed_ms(start);
            return result;
        };
        auto stop = [&] { return request.cancel && request.cancel->load(); };
        const auto& path = request.world;
        if (!resources.find(path))
            return fail(2, "World " + path + " is not mounted");
        if (request.jobs > S::max_jobs)
            return fail(2, "--gi-jobs must be 1 to 256");
        std::string error;
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
        std::filesystem::path out = request.out_dir;
        if (out.empty())
        {
            if (!user_directory(out, error))
                return fail(2, "GI output directory: " + error);
            out /= "gi";
        }
        result.file = out / path_from_utf8(name);

        // Placement: the committed file verbatim, else the defaults with automatic reflection probes.
        std::filesystem::path placement_dir;
        if (!gi_placement_directory(request.placement_dir, placement_dir, error))
            return fail(2, "GI placement directory: " + error);
        auto placement = default_gi_placement(path, digest);
        std::string detail;
        result.placement = load_gi_placement(placement_dir, path, digest, placement, detail);
        if (result.placement == GiPlacementStatus::Foreign || result.placement == GiPlacementStatus::Failed)
            return fail(4, std::string("Placement ") + gi_placement_status_name(result.placement) + ": " + detail);
        if (result.placement == GiPlacementStatus::Missing)
            placement = default_gi_placement(path, digest);
        const auto settings_text = gi_settings_text(request.settings);
        const auto settings_bytes = std::span(reinterpret_cast<const std::uint8_t*>(settings_text.data()),
                                              settings_text.size());
        Sha256Digest placement_key{};
        if (result.placement == GiPlacementStatus::Ready && !gi_placement_hash(placement, placement_key, error))
            return fail(4, "Placement: " + error);
        // --gi-if-stale: a Ready file with these settings and this placement is Current. A committed
        // placement fixes the key before any work; defaults are keyed once re-placed below.
        auto current = [&]
        {
            std::vector<std::uint8_t> existing;
            GiFile file;
            auto expectation = expected;
            expectation.settings = sha256(settings_bytes);
            expectation.placement = placement_key;
            unsigned stale = 0;
            std::string ignored;
            if (!request.if_stale || !read_gi_bytes(path_text(result.file), existing, ignored) ||
                !read_gi_file(existing, file, ignored) || gi_status(file.key, expectation, stale) != GiStatus::Ready)
                return false;
            result.status = GiBakeResult::Status::Current;
            result.exit_code = 0;
            result.bytes = existing.size();
            result.ms = elapsed_ms(start);
            return true;
        };
        if (result.placement == GiPlacementStatus::Ready && current())
            return result;

        CollisionWorld collision;
        GiPlacementScene placement_scene;
        if (world.models.empty() || !collision.build(world.models[0], error) ||
            !placement_scene.build(world, collision, error))
            return fail(5, "World " + path + ": " + error);
        GiLightLattice lattice;
        GiPlacementStats stats;
        if (!resolve_light_probes(placement_scene, placement, lattice, stats, nullptr, error))
            return fail(4, "Light probes: " + error);
        if (result.placement == GiPlacementStatus::Missing &&
            (!place_reflection_probes(placement_scene, lattice, placement, stats, error) ||
             !gi_placement_hash(placement, placement_key, error)))
            return fail(4, "Reflection probes: " + error);
        result.probes = lattice.probes.size();
        result.reflections = placement.reflections.size();
        if (result.placement == GiPlacementStatus::Missing && current())
            return result;
        if (stop())
            return cancelled();

        // Scene and survey.
        WorldMesh mesh;
        std::vector<WorldTriangleSource> sources;
        if (!mesh.build(world, error, &resources, true, &sources))
            return fail(5, "World mesh: " + error);
        std::vector<GiRangeUse> use;
        if (!gi_range_uses(world, placement.excluded_models, use, result.scene, error))
            return fail(4, "Placement: " + error);
        GiBakeScene scene;
        if (!scene.build(std::move(mesh), use, result.scene, error))
            return fail(5, "GI scene: " + error);
        if (stop())
            return cancelled();
        result.survey = survey_gi_probes(scene, lattice.probes, request.settings.rays, request.jobs, request.cancel,
                                         request.progress);
        if (result.survey.cancelled || stop())
            return cancelled();

        // Sky and indirect light (lot G7). The lattice seed is the first eight digest bytes.
        result.sky_setup = gi_sky_setup(world);
        GiSky sky;
        if (!sky.build(world, scene, result.sky_setup, float(request.settings.sky) / 100, request.jobs, result.sky,
                       error))
            return fail(4, "GI sky: " + error);
        GiReceivers receivers;
        if (!gi_receivers(world, scene, sources, use, request.jobs, receivers, error))
            return fail(4, error);
        std::uint64_t seed = 0;
        for (int i = 0; i < 8; ++i)
            seed |= std::uint64_t(digest[i]) << (8 * i);
        GiIndirect indirect;
        if (!bake_gi_lightmaps(scene, sky, receivers, request.settings.bounces, request.settings.rays, seed,
                               request.jobs, request.cancel, request.progress, indirect, result.lightmaps, error))
            return result.lightmaps.cancelled ? cancelled() : fail(4, "GI lightmaps: " + error);
        if (stop())
            return cancelled();

        // The file: INFO, PLAC, LMAP and VTX (probes arrive with G9).
        GiFile file;
        file.key.world = expected.world;
        file.key.albedo = expected.albedo;
        file.key.algorithm = expected.algorithm;
        file.key.edition = expected.edition;
        file.key.world_path = path;
        file.key.settings = sha256(settings_bytes);
        file.key.placement = placement_key;
        file.settings = settings_text;
        GiSection plac{gi_format::placement, {}};
        if (!encode_gi_placement_section(lattice, placement.reflections, plac.payload, error))
            return fail(4, "Placement section: " + error);
        file.sections.push_back(std::move(plac));
        GiLightmaps maps;
        GiVertexLight vertex_light;
        gi_lightmap_sections(receivers, indirect, maps, vertex_light);
        GiSection lmap{gi_format::lightmaps, {}}, vtx{gi_format::vertices, {}};
        if (!write_gi_lightmaps(maps, lmap.payload, error) || !write_gi_vertex_light(vertex_light, vtx.payload, error))
            return fail(4, "Lightmap sections: " + error);
        file.sections.push_back(std::move(lmap));
        file.sections.push_back(std::move(vtx));
        std::vector<std::uint8_t> encoded;
        if (!write_gi_file(file, encoded, error))
            return fail(4, "GI file: " + error);
        result.bytes = encoded.size();
        if (stop())
            return cancelled();
        std::error_code code;
        std::filesystem::create_directories(result.file.parent_path(), code);
        if (code)
            return fail(4, "GI output directory: " + code.message());
        if (!save_gi_file(result.file, file, error))
            return fail(4, "GI file: " + error);
        result.status = GiBakeResult::Status::Baked;
        result.exit_code = 0;
        result.ms = elapsed_ms(start);
        return result;
    }
} // namespace OpenBlood2
