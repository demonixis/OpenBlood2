#include "engine/gi_reflections.h"
#include "engine/gi_direct.h"
#include <algorithm>
#include <cfloat>
#include <chrono>
#include <cmath>
#include <mutex>

namespace OpenBlood2
{
    namespace L = gi_reflection_limits;
    namespace
    {
        constexpr double pi = 3.14159265358979323846;
        double elapsed_ms(std::chrono::steady_clock::time_point start)
        {
            return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
        }
        void put(std::vector<std::uint8_t>& out, std::uint32_t v)
        {
            for (int i = 0; i < 4; ++i)
                out.push_back(std::uint8_t(v >> (8 * i)));
        }
        std::uint32_t get(std::span<const std::uint8_t> in, std::size_t offset)
        {
            return std::uint32_t(in[offset]) | std::uint32_t(in[offset + 1]) << 8 | std::uint32_t(in[offset + 2]) << 16 |
                   std::uint32_t(in[offset + 3]) << 24;
        }
        bool power_of_two(std::uint32_t v)
        {
            return v && !(v & (v - 1));
        }
        std::uint32_t radical_inverse(std::uint32_t bits)
        {
            bits = (bits << 16u) | (bits >> 16u);
            bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xaaaaaaaau) >> 1u);
            bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xccccccccu) >> 2u);
            bits = ((bits & 0x0f0f0f0fu) << 4u) | ((bits & 0xf0f0f0f0u) >> 4u);
            bits = ((bits & 0x00ff00ffu) << 8u) | ((bits & 0xff00ff00u) >> 8u);
            return bits;
        }
        DVector normalized(double x, double y, double z)
        {
            const double length = std::sqrt(x * x + y * y + z * z);
            return length > 0 ? DVector{float(x / length), float(y / length), float(z / length)} : DVector{0, 1, 0};
        }
        // Copies the folded neighbours of a level's interior into its border (octahedral wrap: the map's
        // edges mirror onto themselves, its corners onto the opposite corner).
        void fill_border(std::uint32_t* level, std::uint32_t size)
        {
            const std::uint32_t n = size - 2;
            const auto at = [&](std::uint32_t a, std::uint32_t b) -> std::uint32_t& { return level[(b + 1) * size + a + 1]; };
            for (std::uint32_t b = 0; b < n; ++b)
            {
                level[(b + 1) * size] = at(0, n - 1 - b);
                level[(b + 1) * size + size - 1] = at(n - 1, n - 1 - b);
            }
            for (std::uint32_t a = 0; a < n; ++a)
            {
                level[a + 1] = at(n - 1 - a, 0);
                level[(size - 1) * size + a + 1] = at(n - 1 - a, n - 1);
            }
            level[0] = at(n - 1, n - 1);
            level[size - 1] = at(0, n - 1);
            level[(size - 1) * size] = at(n - 1, 0);
            level[(size - 1) * size + size - 1] = at(0, 0);
        }
    } // namespace

    void gi_octahedral_coordinates(double x, double y, double z, double& u, double& v)
    {
        const double sum = std::fabs(x) + std::fabs(y) + std::fabs(z);
        if (!(sum > 0))
        {
            u = v = 0;
            return;
        }
        x /= sum, y /= sum, z /= sum;
        if (y >= 0)
        {
            u = x, v = z;
            return;
        }
        u = (1 - std::fabs(z)) * (x >= 0 ? 1 : -1);
        v = (1 - std::fabs(x)) * (z >= 0 ? 1 : -1);
    }
    DVector gi_octahedral_direction(double u, double v)
    {
        double x = u, z = v;
        const double y = 1 - std::fabs(u) - std::fabs(v);
        if (y < 0)
        {
            const double ox = x;
            x = (1 - std::fabs(z)) * (ox >= 0 ? 1 : -1);
            z = (1 - std::fabs(ox)) * (z >= 0 ? 1 : -1);
        }
        return normalized(x, y, z);
    }
    std::uint32_t gi_reflection_levels(std::uint32_t resolution)
    {
        std::uint32_t levels = 0;
        for (auto s = resolution; s >= L::smallest_level; s >>= 1)
            ++levels;
        return levels;
    }
    std::size_t gi_reflection_level_offset(std::uint32_t resolution, std::uint32_t level)
    {
        std::size_t offset = 0;
        for (std::uint32_t l = 0; l < level; ++l)
            offset += std::size_t(resolution >> l) * (resolution >> l);
        return offset;
    }
    std::size_t gi_reflection_chain_texels(std::uint32_t resolution)
    {
        return gi_reflection_level_offset(resolution, gi_reflection_levels(resolution));
    }
    std::size_t GiReflections::first(GiReflectionSet set, std::size_t probe, std::uint32_t level) const
    {
        return (std::size_t(set) * size() + probe) * gi_reflection_chain_texels(resolution) +
               gi_reflection_level_offset(resolution, level);
    }

    bool write_gi_reflections(const GiReflections& in, std::vector<std::uint8_t>& out, std::string& error)
    {
        out.clear();
        const auto probes = in.valid.size();
        const bool sized = probes ? power_of_two(in.resolution) && in.resolution >= gi_placement_limits::min_resolution &&
                                        in.resolution <= gi_placement_limits::max_resolution &&
                                        in.levels == gi_reflection_levels(in.resolution)
                                  : !in.resolution && !in.levels;
        if (probes > gi_placement_limits::max_reflections || !sized ||
            in.texels.size() != L::sets * probes * (probes ? gi_reflection_chain_texels(in.resolution) : 0))
            return error = "GI reflections: invalid section contents", false;
        out.reserve(24 + 4 * (probes + in.texels.size()));
        put(out, L::format);
        put(out, std::uint32_t(probes));
        put(out, in.resolution);
        put(out, in.levels);
        put(out, L::sets);
        put(out, 0);
        for (const auto v : in.valid)
            put(out, v ? 1u : 0u);
        for (const auto t : in.texels)
            put(out, t);
        return true;
    }
    bool read_gi_reflections(std::span<const std::uint8_t> in, GiReflections& out, std::string& error)
    {
        out = {};
        if (in.size() < 24)
            return error = "GI reflections: truncated header", false;
        const auto format = get(in, 0), probes = get(in, 4), resolution = get(in, 8), levels = get(in, 12),
                   sets = get(in, 16), reserved = get(in, 20);
        if (format != L::format)
            return error = "GI reflections: unsupported format " + std::to_string(format), false;
        if (sets != L::sets || reserved)
            return error = "GI reflections: invalid set count or reserved field", false;
        if (probes > gi_placement_limits::max_reflections)
            return error = "GI reflections: too many probes", false;
        const bool sized = probes ? power_of_two(resolution) && resolution >= gi_placement_limits::min_resolution &&
                                        resolution <= gi_placement_limits::max_resolution &&
                                        levels == gi_reflection_levels(resolution)
                                  : !resolution && !levels;
        if (!sized)
            return error = "GI reflections: invalid resolution or level count", false;
        const std::size_t texels = std::size_t(sets) * probes * (probes ? gi_reflection_chain_texels(resolution) : 0);
        if (in.size() != 24 + 4 * (std::size_t(probes) + texels))
            return error = "GI reflections: size does not match the header", false;
        out.resolution = resolution;
        out.levels = levels;
        out.valid.resize(probes);
        for (std::uint32_t p = 0; p < probes; ++p)
        {
            const auto flags = get(in, 24 + 4 * std::size_t(p));
            if (flags > 1)
                return out = {}, error = "GI reflections: unknown probe flags", false;
            out.valid[p] = std::uint8_t(flags);
        }
        out.texels.resize(texels);
        const std::size_t base = 24 + 4 * std::size_t(probes);
        for (std::size_t i = 0; i < texels; ++i)
            out.texels[i] = get(in, base + 4 * i);
        return true;
    }
    bool check_gi_reflections(std::span<const GiReflectionProbe> probes, const GiReflections& r, std::string& error)
    {
        std::uint32_t resolution = 0;
        for (const auto& p : probes)
            resolution = std::max(resolution, p.resolution);
        if (r.size() != probes.size() || r.resolution != resolution)
            return error = "GI reflections: " + std::to_string(r.size()) + " probes at " + std::to_string(r.resolution) +
                           " for a placement with " + std::to_string(probes.size()) + " at " + std::to_string(resolution),
                   false;
        return true;
    }

    DVector gi_reflection_cube_direction(unsigned face, double u, double v)
    {
        switch (face)
        {
            case 0: return normalized(1, -v, -u);
            case 1: return normalized(-1, -v, u);
            case 2: return normalized(u, 1, v);
            case 3: return normalized(u, -1, -v);
            case 4: return normalized(u, -v, 1);
            case 5: return normalized(-u, -v, -1);
            default: return {0, 1, 0};
        }
    }
    void gi_reflection_cube_sample(const GiReflectionCube& cube, DVector direction, double out[3])
    {
        const double x = direction.x, y = direction.y, z = direction.z;
        const double ax = std::fabs(x), ay = std::fabs(y), az = std::fabs(z);
        unsigned face = 0;
        double su = 0, sv = 0;
        if (ax >= ay && ax >= az)
        {
            if (!(ax > 0))
            {
                out[0] = out[1] = out[2] = 0;
                return;
            }
            face = x >= 0 ? 0 : 1;
            su = (x >= 0 ? -z : z) / ax, sv = -y / ax;
        }
        else if (ay >= az)
            face = y >= 0 ? 2 : 3, su = x / ay, sv = (y >= 0 ? z : -z) / ay;
        else
            face = z >= 0 ? 4 : 5, su = (z >= 0 ? x : -x) / az, sv = -y / az;
        const double size = cube.size;
        const double px = std::clamp((su * .5 + .5) * size - .5, 0.0, size - 1);
        const double py = std::clamp((sv * .5 + .5) * size - .5, 0.0, size - 1);
        const auto x0 = std::uint32_t(std::floor(px)), y0 = std::uint32_t(std::floor(py));
        const auto x1 = std::min(x0 + 1, cube.size - 1), y1 = std::min(y0 + 1, cube.size - 1);
        const double tx = px - x0, ty = py - y0;
        const float* f = cube.texels.data() + std::size_t(face) * cube.size * cube.size * 3;
        const auto texel = [&](std::uint32_t tx_, std::uint32_t ty_, int c)
        { return double(f[(std::size_t(ty_) * cube.size + tx_) * 3 + c]); };
        for (int c = 0; c < 3; ++c)
        {
            const double top = texel(x0, y0, c) + (texel(x1, y0, c) - texel(x0, y0, c)) * tx;
            const double bottom = texel(x0, y1, c) + (texel(x1, y1, c) - texel(x0, y1, c)) * tx;
            out[c] = top + (bottom - top) * ty;
        }
    }
    DVector gi_ggx_half_vector(double sx, double sy, DVector normal, double roughness)
    {
        const double alpha = std::max(roughness * roughness, L::min_alpha);
        const double a2 = alpha * alpha;
        double sine_phi = 0, cosine_phi = 1;
        gi_sin_cos(2 * pi * sx, sine_phi, cosine_phi);
        const double cosine = std::sqrt((1 - sy) / (1 + (a2 - 1) * sy));
        const double sine = std::sqrt(std::max(1 - cosine * cosine, 0.0));
        const double hx = sine * cosine_phi, hy = sine * sine_phi, hz = cosine;
        const double nx = normal.x, ny = normal.y, nz = normal.z;
        // tangent = normalize(cross(reference, n)), bitangent = cross(n, tangent).
        double tx = 0, ty = 0, tz = 0;
        if (std::fabs(nz) < .999)
            tx = -ny, ty = nx, tz = 0; // (0, 0, 1) x n
        else
            tx = 0, ty = -nz, tz = ny; // (1, 0, 0) x n
        const double tl = std::sqrt(tx * tx + ty * ty + tz * tz);
        tx /= tl, ty /= tl, tz /= tl;
        const double bx = ny * tz - nz * ty, by = nz * tx - nx * tz, bz = nx * ty - ny * tx;
        return normalized(tx * hx + bx * hy + nx * hz, ty * hx + by * hy + ny * hz, tz * hx + bz * hy + nz * hz);
    }
    void gi_ggx_prefilter(const GiReflectionCube& cube, DVector normal, double roughness, std::uint32_t samples,
                          double out[3])
    {
        double sum[3]{}, total = 0;
        const double nx = normal.x, ny = normal.y, nz = normal.z;
        for (std::uint32_t i = 0; i < samples; ++i)
        {
            const auto h = gi_ggx_half_vector((i + .5) / samples, radical_inverse(i) * 2.3283064365386963e-10,
                                              normal, roughness);
            const double d = 2 * (nx * h.x + ny * h.y + nz * h.z);
            const auto l = normalized(d * h.x - nx, d * h.y - ny, d * h.z - nz);
            const double weight = std::clamp(nx * l.x + ny * l.y + nz * l.z, 0.0, 1.0);
            if (weight > 0)
            {
                double value[3];
                gi_reflection_cube_sample(cube, l, value);
                for (int c = 0; c < 3; ++c)
                    sum[c] += value[c] * weight;
                total += weight;
            }
        }
        if (total > 0)
            for (int c = 0; c < 3; ++c)
                out[c] = sum[c] / total;
        else
            gi_reflection_cube_sample(cube, normal, out);
    }
    void gi_reflection_chain(const GiReflectionCube& cube, std::uint32_t resolution, std::span<std::uint32_t> out)
    {
        const auto levels = gi_reflection_levels(resolution);
        const std::uint32_t samples = cube.size > L::wide_face ? L::wide_samples : L::samples;
        for (std::uint32_t level = 0; level < levels; ++level)
        {
            const std::uint32_t size = resolution >> level, n = size - 2;
            std::uint32_t* texels = out.data() + gi_reflection_level_offset(resolution, level);
            const double roughness = double(level) / (levels - 1);
            for (std::uint32_t b = 0; b < n; ++b)
                for (std::uint32_t a = 0; a < n; ++a)
                {
                    double value[3]{};
                    if (level == 0)
                    {
                        for (std::uint32_t j = 0; j < 2; ++j)
                            for (std::uint32_t i = 0; i < 2; ++i)
                            {
                                double sample[3];
                                gi_reflection_cube_sample(
                                    cube,
                                    gi_octahedral_direction(2 * (a + (i + .5) / 2) / n - 1, 2 * (b + (j + .5) / 2) / n - 1),
                                    sample);
                                for (int c = 0; c < 3; ++c)
                                    value[c] += sample[c] / 4;
                            }
                    }
                    else
                        gi_ggx_prefilter(cube, gi_octahedral_direction(2 * (a + .5) / n - 1, 2 * (b + .5) / n - 1),
                                         roughness, samples, value);
                    texels[(b + 1) * size + a + 1] = gi_rgb9e5({float(value[0]), float(value[1]), float(value[2])});
                }
            fill_border(texels, size);
        }
    }

    bool gi_reflection_light(const GiReceivers& r, const GiLightmaps& maps, const GiVertexLight& vertex,
                             std::span<const std::uint32_t> direct, std::vector<float>& original,
                             std::vector<float>& composed, std::string& error)
    {
        original.clear();
        composed.clear();
        if (maps.texels.size() != r.texels || vertex.corners.size() != r.corners || r.original.size() != r.elements() ||
            (!direct.empty() && direct.size() != r.texels))
            return error = "GI reflections: sections do not match the receivers", false;
        const float ambient[3]{r.ambient.x, r.ambient.y, r.ambient.z};
        original.resize(r.elements() * 3);
        composed.resize(r.elements() * 3);
        for (std::size_t e = 0; e < r.elements(); ++e)
        {
            const bool texel = e < r.texels;
            const auto energy = gi_rgb9e5_decode(texel ? maps.texels[e] : vertex.corners[e - r.texels]);
            const float en[3]{energy.x, energy.y, energy.z};
            DVector d{};
            if (texel && !direct.empty())
                d = gi_rgb9e5_decode(direct[e]);
            const float dv[3]{d.x, d.y, d.z};
            for (int c = 0; c < 3; ++c)
            {
                const auto o = r.original[e][c];
                original[e * 3 + c] = o / 255.f;
                const auto out = texel && !direct.empty()
                                     ? gi_compose_direct(dv[c], o, ambient[c], en[c], gi_default_k, gi_default_f)
                                     : gi_compose(o, ambient[c], en[c], gi_default_k, gi_default_f, texel);
                composed[e * 3 + c] = out / 255.f;
            }
        }
        return true;
    }

    bool bake_gi_reflections(const GiBakeScene& scene, const GiSky& sky, const GiReceivers& r,
                             std::span<const float> original, std::span<const float> composed,
                             std::span<const GiReflectionProbe> probes, unsigned jobs, const std::atomic<bool>* cancel,
                             const GiProgress& progress, GiReflections& out, GiReflectionReport& report,
                             std::string& error)
    {
        const auto start = std::chrono::steady_clock::now();
        out = {};
        report = {};
        report.probes = probes.size();
        if (original.size() != r.elements() * 3 || composed.size() != original.size() ||
            r.links.size() != scene.faces().size() || probes.size() > gi_placement_limits::max_reflections)
            return error = "GI reflections: invalid receivers or probes", false;
        std::uint32_t resolution = 0;
        for (const auto& p : probes)
        {
            if (!power_of_two(p.resolution) || p.resolution < gi_placement_limits::min_resolution ||
                p.resolution > gi_placement_limits::max_resolution)
                return error = "GI reflections: invalid probe resolution", false;
            resolution = std::max(resolution, p.resolution);
        }
        GiReflections result;
        result.resolution = resolution;
        result.levels = gi_reflection_levels(resolution);
        result.valid.assign(probes.size(), 0);
        const std::size_t chain = probes.empty() ? 0 : gi_reflection_chain_texels(resolution);
        result.texels.assign(L::sets * probes.size() * chain, 0);
        report.resolution = result.resolution;
        report.levels = result.levels;
        const unsigned threads = std::max(1u, jobs ? jobs : gi_default_jobs());
        struct Counters
        {
            GiRayCounters rays;
            double capture_ms = 0, prefilter_ms = 0;
        };
        std::vector<Counters> counters(threads);
        const std::uint64_t total = probes.size();
        std::atomic<std::uint64_t> done{0};
        std::uint64_t reported = 0;
        std::mutex report_lock;
        const auto run = run_gi_jobs(
            total, threads, cancel,
            [&](std::uint64_t p, unsigned thread)
            {
                auto& count = counters[thread];
                const auto& probe = probes[p];
                const auto captured = std::chrono::steady_clock::now();
                GiReflectionCube cubes[L::sets];
                const std::uint32_t size = probe.resolution;
                for (auto& cube : cubes)
                {
                    cube.size = size;
                    cube.texels.assign(std::size_t(6) * size * size * 3, 0.f);
                }
                GiRayCounters ignored;
                GiHit hit;
                std::uint64_t back_rays = 0;
                const double strata = L::subsamples;
                for (unsigned face = 0; face < 6; ++face)
                    for (std::uint32_t y = 0; y < size; ++y)
                        for (std::uint32_t x = 0; x < size; ++x)
                        {
                            double sum[L::sets][3]{};
                            for (std::uint32_t j = 0; j < L::subsamples; ++j)
                                for (std::uint32_t i = 0; i < L::subsamples; ++i)
                                {
                                    const auto direction =
                                        gi_reflection_cube_direction(face, 2 * (x + (i + .5) / strata) / size - 1,
                                                                     2 * (y + (j + .5) / strata) / size - 1);
                                    scene.trace(probe.capture, direction, FLT_MAX, hit, count.rays.trace);
                                    double value[3];
                                    bool back = false;
                                    gi_hit_radiance(scene, sky, r, original.data(), hit, direction, count.rays, value,
                                                    back);
                                    back_rays += back;
                                    for (int c = 0; c < 3; ++c)
                                        sum[0][c] += value[c];
                                    gi_hit_radiance(scene, sky, r, composed.data(), hit, direction, ignored, value, back);
                                    for (int c = 0; c < 3; ++c)
                                        sum[1][c] += value[c];
                                }
                            const auto texel = ((std::size_t(face) * size + y) * size + x) * 3;
                            for (std::uint32_t s = 0; s < L::sets; ++s)
                                for (int c = 0; c < 3; ++c)
                                    cubes[s].texels[texel + c] = float(sum[s][c] / (strata * strata));
                        }
                const std::uint64_t rays = std::uint64_t(6) * size * size * L::subsamples * L::subsamples;
                result.valid[p] = back_rays <= L::back_limit * rays;
                const auto filtered = std::chrono::steady_clock::now();
                count.capture_ms += std::chrono::duration<double, std::milli>(filtered - captured).count();
                for (std::uint32_t s = 0; s < L::sets; ++s)
                    gi_reflection_chain(cubes[s], resolution,
                                        std::span<std::uint32_t>(result.texels).subspan((s * total + p) * chain, chain));
                count.prefilter_ms += elapsed_ms(filtered);
                const auto finished = done.fetch_add(1) + 1;
                if (progress)
                {
                    std::lock_guard lock(report_lock);
                    if (finished > reported)
                    {
                        reported = finished;
                        progress("reflections", finished, total);
                    }
                }
            });
        report.threads = run.threads;
        if (run.cancelled)
        {
            report.cancelled = true;
            report.ms = elapsed_ms(start);
            return error = "cancelled", false;
        }
        for (const auto& count : counters)
        {
            report.trace.rays += count.rays.trace.rays;
            report.trace.nodes += count.rays.trace.nodes;
            report.trace.triangles += count.rays.trace.triangles;
            report.trace.texels += count.rays.trace.texels;
            report.back += count.rays.back;
            report.sky += count.rays.sky;
            report.untextured += count.rays.untextured;
            report.layer_limit += count.rays.layer_limit;
            report.capture_ms += count.capture_ms;
            report.prefilter_ms += count.prefilter_ms;
        }
        report.rays = report.trace.rays;
        for (const auto v : result.valid)
            report.valid += v;
        out = std::move(result);
        report.ms = elapsed_ms(start);
        return true;
    }
} // namespace OpenBlood2
