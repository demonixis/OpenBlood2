#include "engine/gi_bake.h"
#include "engine/gi_jobs.h"
#include "engine/surface_materials.h"
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
        case GiQuality::Ultra:
            return "ultra";
        default:
            return "custom";
        }
    }
    const char* gi_direct_source_name(GiDirectSource source)
    {
        return source == GiDirectSource::Rebaked ? "rebaked" : "original";
    }
    bool gi_direct_source(std::string_view name, GiDirectSource& source)
    {
        if (name == "original")
            source = GiDirectSource::Original;
        else if (name == "rebaked")
            source = GiDirectSource::Rebaked;
        else
            return false;
        return true;
    }
    bool make_gi_bake_settings(const GiBakeArguments& a, GiBakeSettings& settings, std::string& error)
    {
        GiBakeSettings out;
        const bool tuned = a.bounces || a.rays || a.sky >= 0 || a.texel_samples || a.denoise >= 0 ||
                           a.shadow_radius || a.shadow_rays || a.reflection_strata || a.reflection_samples;
        if (a.quality == "preview")
            out = {GiQuality::Preview, 1, 64, 100};
        else if (a.quality == "final" || a.quality.empty())
            out = {GiQuality::Final, 2, 256, 100};
        else if (a.quality == "ultra")
        {
            out = {GiQuality::Ultra, 3, 512, 100};
            out.texel_grid = 2;
            out.denoise = 1;
            out.shadow_radius = 8;
            out.shadow_rays = S::default_shadow_rays;
            out.reflection_strata = 4;
            out.reflection_samples = 256;
        }
        else if (a.quality == "custom")
        {
            out.quality = GiQuality::Custom;
            if (a.bounces)
                out.bounces = a.bounces;
            if (a.rays)
                out.rays = a.rays;
            if (a.sky >= 0)
                out.sky = static_cast<std::uint32_t>(a.sky);
            if (a.sky < -1 || out.sky > S::max_sky)
            {
                error = "--gi-sky must be 0 to 4";
                return false;
            }
            if (out.bounces < S::min_bounces || out.bounces > S::max_bounces)
            {
                error = "--gi-bounces must be 1 to 4";
                return false;
            }
            if (out.rays < S::min_rays || out.rays > S::max_rays)
            {
                error = "--gi-rays must be 16 to 4096";
                return false;
            }
            if (a.texel_samples)
            {
                std::uint32_t grid = 1;
                while (grid < S::max_texel_grid && grid * grid < a.texel_samples)
                    ++grid;
                if (grid * grid != a.texel_samples)
                {
                    error = "--gi-texel-samples must be 1, 4, 9 or 16";
                    return false;
                }
                out.texel_grid = grid;
            }
            if (a.denoise < -1 || a.denoise > int(S::max_denoise))
            {
                error = "--gi-denoise must be 0 to 4";
                return false;
            }
            if (a.denoise >= 0)
                out.denoise = static_cast<std::uint32_t>(a.denoise);
            if (a.shadow_radius > S::max_shadow_radius || a.shadow_rays > S::max_shadow_rays)
            {
                error = "--gi-shadow-radius and --gi-shadow-rays must be 1 to 64";
                return false;
            }
            if (a.shadow_rays && !a.shadow_radius)
            {
                error = "--gi-shadow-rays needs --gi-shadow-radius";
                return false;
            }
            out.shadow_radius = a.shadow_radius;
            out.shadow_rays = a.shadow_radius ? (a.shadow_rays ? a.shadow_rays : S::default_shadow_rays) : 0;
            if (a.reflection_strata &&
                (a.reflection_strata < S::min_reflection_strata || a.reflection_strata > S::max_reflection_strata))
            {
                error = "--gi-reflection-strata must be 2 to 8";
                return false;
            }
            if (a.reflection_strata)
                out.reflection_strata = a.reflection_strata;
            if (a.reflection_samples &&
                (a.reflection_samples < S::min_reflection_samples || a.reflection_samples > S::max_reflection_samples))
            {
                error = "--gi-reflection-samples must be 32 to 1024";
                return false;
            }
            out.reflection_samples = a.reflection_samples;
        }
        else
        {
            error = "--gi-quality must be preview, final, ultra or custom";
            return false;
        }
        if (out.quality != GiQuality::Custom && tuned)
        {
            error = "--gi-bounces, --gi-rays, --gi-sky, --gi-texel-samples, --gi-denoise, --gi-shadow-radius, "
                    "--gi-shadow-rays, --gi-reflection-strata and --gi-reflection-samples need --gi-quality custom";
            return false;
        }
        if (a.emissive < -1 || a.emissive > int(S::max_emissive))
        {
            error = "--gi-emissive must be 0 to 4";
            return false;
        }
        if (a.emissive >= 0)
            out.emissive = static_cast<std::uint32_t>(a.emissive);
        settings = out;
        return true;
    }
    bool make_gi_bake_settings(std::string_view quality, std::uint32_t bounces, std::uint32_t rays, std::int32_t sky,
                               GiBakeSettings& settings, std::string& error)
    {
        GiBakeArguments a;
        a.quality = quality;
        a.bounces = bounces;
        a.rays = rays;
        a.sky = sky;
        return make_gi_bake_settings(a, settings, error);
    }
    std::string gi_manifest_digest_prefix()
    {
        const auto text = builtin_surface_manifest();
        return digest_text(sha256(std::span(reinterpret_cast<const std::uint8_t*>(text.data()), text.size())))
            .substr(0, 16);
    }
    std::string gi_settings_text(const GiBakeSettings& settings)
    {
        auto hundredths = [](std::uint32_t v)
        { return std::to_string(v / 100) + "." + char('0' + v / 10 % 10) + char('0' + v % 10); };
        std::string text = "openblood2-gi-settings 2\nquality " + std::string(gi_quality_name(settings.quality)) +
                           "\nbounces " + std::to_string(settings.bounces) + "\nrays " +
                           std::to_string(settings.rays) + "\nsky " + hundredths(settings.sky) + "\n";
        if (settings.texel_grid > 1)
            text += "texel-samples " + std::to_string(settings.texel_grid * settings.texel_grid) + "\n";
        if (settings.denoise)
            text += "denoise " + std::to_string(settings.denoise) + "\n";
        if (settings.shadow_radius)
            text += "shadows " + std::to_string(settings.shadow_radius) + " " + std::to_string(settings.shadow_rays) +
                    "\n";
        if (settings.reflection_strata != 2)
            text += "reflection-strata " + std::to_string(settings.reflection_strata) + "\n";
        if (settings.reflection_samples)
            text += "reflection-samples " + std::to_string(settings.reflection_samples) + "\n";
        if (settings.emissive)
            text += "emissive " + hundredths(settings.emissive) + " " + gi_manifest_digest_prefix() + "\n";
        if (settings.direct == GiDirectSource::Rebaked)
            text += "direct rebaked\n";
        return text;
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
        if (request.settings.emissive)
        {
            // Lot GE1: the built-in manifest's emissive classes (the settings text names its digest).
            SurfaceManifest manifest;
            if (!manifest.parse_builtin(error) ||
                !scene.prepare_emission(manifest, request.settings.emissive, result.emission, error))
                return fail(5, "GI emission: " + error);
        }
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
        if (!gi_receivers(world, scene, sources, use, request.jobs, receivers, error, request.settings.texel_grid))
            return fail(4, error);
        std::uint64_t seed = 0;
        for (int i = 0; i < 8; ++i)
            seed |= std::uint64_t(digest[i]) << (8 * i);
        // The re-baked direct light (lot GD2), before the passes that bounce it.
        GiDirectBake direct;
        const bool rebaked = request.settings.direct == GiDirectSource::Rebaked;
        if (rebaked)
        {
            GiDirectOptions options; // Lot GQ1: soft shadows.
            options.shadow_radius = request.settings.shadow_radius;
            options.shadow_rays = request.settings.shadow_rays;
            options.seed = seed;
            if (!gi_rebake_direct(world, receivers, request.jobs, request.cancel, request.progress, direct, error,
                                  options))
                return direct.cancelled ? cancelled() : fail(4, "GI direct light: " + error);
            result.direct_lights = direct.lights;
            result.direct_triangles = direct.occluder_triangles;
            result.direct_kept = direct.kept;
            result.direct_saturated = direct.saturated;
            result.direct_tests = direct.tests;
            result.direct_ms = direct.ms;
            if (stop())
                return cancelled();
        }
        GiIndirect indirect;
        if (!bake_gi_lightmaps(scene, sky, receivers, request.settings.bounces, request.settings.rays, seed,
                               request.jobs, request.cancel, request.progress, indirect, result.lightmaps, error,
                               direct.direct, request.settings.denoise))
            return result.lightmaps.cancelled ? cancelled() : fail(4, "GI lightmaps: " + error);
        if (stop())
            return cancelled();

        // Light probes (lot G9), from the state the last pass leaves, with the pass ray count.
        std::vector<float> light;
        gi_light_state(receivers, indirect, light, direct.direct);
        GiProbes probes;
        if (!bake_gi_probes(scene, sky, receivers, light, lattice.probes, request.settings.rays, seed, request.jobs,
                            request.cancel, request.progress, probes, result.light_probes, error))
            return result.light_probes.cancelled ? cancelled() : fail(4, "GI light probes: " + error);
        if (stop())
            return cancelled();

        // Reflection probes (lot G11): the world as GI off and GI on draw it, and the sky as shown.
        GiLightmaps maps;
        GiVertexLight vertex_light;
        gi_lightmap_sections(receivers, indirect, maps, vertex_light);
        std::vector<float> original_light, composed_light;
        if (!gi_reflection_light(receivers, maps, vertex_light, rebaked ? std::span<const std::uint32_t>(direct.section.texels)
                                                                        : std::span<const std::uint32_t>{},
                                 original_light, composed_light, error))
            return fail(4, error);
        GiSky shown = sky;
        shown.set_scale(1);
        GiReflections reflections;
        if (!bake_gi_reflections(scene, shown, receivers, original_light, composed_light, placement.reflections,
                                 request.jobs, request.cancel, request.progress, reflections, result.reflection_probes,
                                 error, request.settings.reflection_strata, request.settings.reflection_samples))
            return result.reflection_probes.cancelled ? cancelled() : fail(4, "GI reflection probes: " + error);
        if (stop())
            return cancelled();

        // The file: INFO, PLAC, LMAP, DIR (re-baked only), VTX, PROB and REFL.
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
        GiSection lmap{gi_format::lightmaps, {}}, vtx{gi_format::vertices, {}};
        if (!write_gi_lightmaps(maps, lmap.payload, error) || !write_gi_vertex_light(vertex_light, vtx.payload, error))
            return fail(4, "Lightmap sections: " + error);
        file.sections.push_back(std::move(lmap));
        if (rebaked)
        {
            GiSection dir{gi_format::direct, {}};
            if (!write_gi_direct(direct.section, dir.payload, error))
                return fail(4, "Direct section: " + error);
            file.sections.push_back(std::move(dir));
        }
        file.sections.push_back(std::move(vtx));
        GiSection prob{gi_format::probes, {}};
        if (!write_gi_probes(probes, prob.payload, error))
            return fail(4, "Probe section: " + error);
        file.sections.push_back(std::move(prob));
        GiSection refl{gi_format::reflections, {}};
        if (!write_gi_reflections(reflections, refl.payload, error))
            return fail(4, "Reflection section: " + error);
        file.sections.push_back(std::move(refl));
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
