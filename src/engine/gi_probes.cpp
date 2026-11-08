#include "engine/gi_probes.h"
#include <algorithm>
#include <cfloat>
#include <chrono>
#include <cmath>
#include <cstring>
#include <mutex>

namespace OpenBlood2
{
    namespace L = gi_probe_limits;
    namespace
    {
        constexpr double pi = 3.14159265358979323846;
        // Real SH normalization constants.
        constexpr double y0 = 0.28209479177387814, y1 = 0.48860251190291992, y2 = 1.0925484305920792,
                         y20 = 0.31539156525252005, y22 = 0.54627421529603959;
        // CyberEngine's cosine-lobe band factors (pi, 2 pi / 3, pi / 4) over pi: E/pi coefficients.
        constexpr double band[9]{1, 2.0 / 3, 2.0 / 3, 2.0 / 3, .25, .25, .25, .25, .25};

        double elapsed_ms(std::chrono::steady_clock::time_point start)
        {
            return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
        }
        void put32(std::vector<std::uint8_t>& out, std::uint32_t value)
        {
            for (int i = 0; i < 4; ++i)
                out.push_back(std::uint8_t(value >> (8 * i)));
        }
        std::uint32_t get32(std::span<const std::uint8_t> bytes, std::size_t at)
        {
            std::uint32_t value = 0;
            for (int i = 0; i < 4; ++i)
                value |= std::uint32_t(bytes[at + i]) << (8 * i);
            return value;
        }
    } // namespace

    std::uint16_t gi_half(float value)
    {
        if (!(value == value))
            return 0;
        std::uint32_t bits = 0;
        std::memcpy(&bits, &value, sizeof bits);
        const std::uint16_t sign = std::uint16_t((bits >> 16) & 0x8000);
        const std::uint32_t magnitude = bits & 0x7FFFFFFF;
        if (magnitude >= 0x477FE000) // 65504 and above, infinity included.
            return sign | 0x7BFF;
        if (magnitude < 0x38800000) // Below 2^-14: a subnormal half, round(|value| 2^24) to even.
        {
            float a = 0;
            std::memcpy(&a, &magnitude, sizeof a);
            return sign | std::uint16_t(std::nearbyint(a * 16777216.0f));
        }
        const std::uint32_t exponent = (magnitude >> 23) - 112, mantissa = magnitude & 0x7FFFFF;
        std::uint32_t half = exponent << 10 | mantissa >> 13;
        const std::uint32_t rest = mantissa & 0x1FFF;
        if (rest > 0x1000 || (rest == 0x1000 && (half & 1)))
            ++half; // A carry into the exponent is the next binade, still exact.
        return sign | std::uint16_t(std::min<std::uint32_t>(half, 0x7BFF));
    }
    float gi_half_decode(std::uint16_t half)
    {
        const int exponent = half >> 10 & 31, mantissa = half & 1023;
        float value = 0;
        if (exponent == 31)
            return 0;
        value = exponent ? std::ldexp(float(1024 + mantissa), exponent - 25) : std::ldexp(float(mantissa), -24);
        return (half & 0x8000) ? -value : value;
    }

    void gi_sh_basis(DVector direction, double out[9])
    {
        const double x = direction.x, y = direction.y, z = direction.z;
        out[0] = y0;
        out[1] = y1 * y;
        out[2] = y1 * z;
        out[3] = y1 * x;
        out[4] = y2 * x * y;
        out[5] = y2 * y * z;
        out[6] = y20 * (3 * z * z - 1);
        out[7] = y2 * x * z;
        out[8] = y22 * (x * x - y * y);
    }
    DVector gi_sh_irradiance(std::span<const float, 27> sh, DVector normal)
    {
        double basis[9];
        gi_sh_basis(normal, basis);
        double out[3]{};
        for (int k = 0; k < 9; ++k)
            for (int c = 0; c < 3; ++c)
                out[c] += basis[k] * sh[k * 3 + c];
        return {float(out[0]), float(out[1]), float(out[2])};
    }
    DVector gi_fibonacci_direction(std::uint32_t i, std::uint32_t n)
    {
        const double z = 1 - (2.0 * i + 1) / n;
        const double r = std::sqrt(std::max(0.0, 1 - z * z));
        // Azimuth 2 pi t, t = frac(i / golden ratio): the nearest quarter turn q, then the residual
        // in [-pi/4, pi/4].
        double t = i * 0.6180339887498949;
        t -= std::floor(t);
        const double quarters = std::floor(4 * t + .5);
        double sine = 0, cosine = 1;
        gi_quarter_sin_cos(2 * pi * (t - quarters / 4), sine, cosine);
        double s = sine, c = cosine;
        switch (int(quarters) & 3)
        {
        case 1:
            s = cosine, c = -sine;
            break;
        case 2:
            s = -sine, c = -cosine;
            break;
        case 3:
            s = -cosine, c = sine;
            break;
        default:
            break;
        }
        return {float(r * c), float(r * s), float(z)};
    }
    std::array<double, 4> gi_probe_rotation(std::uint64_t seed, std::uint64_t probe)
    {
        GiRandom random(seed, probe, gi_probe_stream);
        for (;;)
        {
            const double w = 2.0 * random.uniform() - 1, x = 2.0 * random.uniform() - 1,
                         y = 2.0 * random.uniform() - 1, z = 2.0 * random.uniform() - 1;
            const double length = w * w + x * x + y * y + z * z;
            if (length > 1e-6 && length <= 1)
            {
                const double scale = 1 / std::sqrt(length);
                return {w * scale, x * scale, y * scale, z * scale};
            }
        }
    }
    DVector gi_probe_direction(const std::array<double, 4>& q, std::uint32_t i, std::uint32_t n)
    {
        const auto p = gi_fibonacci_direction(i, n);
        const double w = q[0], x = q[1], y = q[2], z = q[3];
        const double px = p.x, py = p.y, pz = p.z;
        // v' = v + 2 w (q x v) + 2 q x (q x v).
        const double tx = 2 * (y * pz - z * py), ty = 2 * (z * px - x * pz), tz = 2 * (x * py - y * px);
        const double ox = px + w * tx + (y * tz - z * ty), oy = py + w * ty + (z * tx - x * tz),
                     oz = pz + w * tz + (x * ty - y * tx);
        return {float(ox), float(oy), float(oz)};
    }

    bool write_gi_probes(const GiProbes& probes, std::vector<std::uint8_t>& out, std::string& error)
    {
        const auto count = probes.valid.size();
        if (count > L::max_probes || probes.sh.size() != count * L::values)
            return error = "GI probes: invalid counts", false;
        out.clear();
        out.reserve(12 + count * L::values * 2 + (count + 7) / 8);
        put32(out, L::format);
        put32(out, std::uint32_t(count));
        put32(out, 0);
        for (const auto value : probes.sh)
        {
            const auto half = gi_half(value);
            out.push_back(std::uint8_t(half));
            out.push_back(std::uint8_t(half >> 8));
        }
        const auto bits = out.size();
        out.resize(bits + (count + 7) / 8, 0);
        for (std::size_t i = 0; i < count; ++i)
            if (probes.valid[i])
                out[bits + i / 8] |= std::uint8_t(1u << (i % 8));
        return true;
    }
    bool read_gi_probes(std::span<const std::uint8_t> bytes, GiProbes& out, std::string& error)
    {
        if (bytes.size() < 12)
            return error = "GI probes: truncated header", false;
        if (get32(bytes, 0) != L::format)
            return error = "GI probes: unsupported format", false;
        const std::uint32_t count = get32(bytes, 4);
        if (count > L::max_probes)
            return error = "GI probes: too many probes", false;
        if (get32(bytes, 8) != 0)
            return error = "GI probes: nonzero reserved field", false;
        const std::size_t values = std::size_t(count) * L::values, bits = 12 + values * 2;
        if (bytes.size() != bits + (std::size_t(count) + 7) / 8)
            return error = "GI probes: size does not match the probe count", false;
        GiProbes probes;
        probes.sh.resize(values);
        for (std::size_t i = 0; i < values; ++i)
        {
            const auto half = std::uint16_t(bytes[12 + i * 2] | bytes[13 + i * 2] << 8);
            if ((half >> 10 & 31) == 31)
                return error = "GI probes: a coefficient is not finite", false;
            probes.sh[i] = gi_half_decode(half);
        }
        probes.valid.resize(count);
        for (std::size_t i = 0; i < count; ++i)
            probes.valid[i] = bytes[bits + i / 8] >> (i % 8) & 1;
        if (count % 8 && bytes.back() >> (count % 8))
            return error = "GI probes: nonzero validity padding", false;
        out = std::move(probes);
        return true;
    }
    bool check_gi_probes(const GiLightLattice& lattice, const GiProbes& probes, std::string& error)
    {
        if (probes.size() != lattice.probes.size() || probes.sh.size() != probes.size() * L::values)
            return error = "GI probes: " + std::to_string(probes.size()) + " probes for a lattice of " +
                           std::to_string(lattice.probes.size()),
                   false;
        return true;
    }

    bool bake_gi_probes(const GiBakeScene& scene, const GiSky& sky, const GiReceivers& r, std::span<const float> light,
                        std::span<const GiLightProbe> lattice, std::uint32_t rays, std::uint64_t seed, unsigned jobs,
                        const std::atomic<bool>* cancel, const GiProgress& progress, GiProbes& out,
                        GiProbeReport& report, std::string& error)
    {
        const auto start = std::chrono::steady_clock::now();
        out = {};
        report = {};
        report.probes = lattice.size();
        if (!rays || light.size() != r.elements() * 3 || r.links.size() != scene.faces().size() ||
            lattice.size() > L::max_probes)
            return error = "GI probes: invalid receivers or settings", false;
        const unsigned threads = std::max(1u, jobs ? jobs : gi_default_jobs());
        struct Counters
        {
            GiRayCounters rays;
            std::array<std::uint64_t, 10> fraction{};
        };
        std::vector<Counters> counters(threads);
        GiProbes probes;
        probes.sh.assign(lattice.size() * L::values, 0.f);
        probes.valid.assign(lattice.size(), 0);
        const std::uint64_t total = lattice.size();
        const auto chunks = (total + L::chunk - 1) / L::chunk;
        std::atomic<std::uint64_t> done{0};
        std::uint64_t reported = 0;
        std::mutex report_lock;
        const double weight = 4 * pi / rays;
        const auto run = run_gi_jobs(
            chunks, threads, cancel,
            [&](std::uint64_t chunk, unsigned thread)
            {
                auto& count = counters[thread];
                GiHit hit;
                const auto first = chunk * L::chunk, end = std::min<std::uint64_t>(first + L::chunk, total);
                for (auto p = first; p < end; ++p)
                {
                    const auto rotation = gi_probe_rotation(seed, p);
                    const auto origin = lattice[p].position;
                    double sum[9][3]{};
                    std::uint32_t back_rays = 0;
                    for (std::uint32_t i = 0; i < rays; ++i)
                    {
                        const auto direction = gi_probe_direction(rotation, i, rays);
                        scene.trace(origin, direction, FLT_MAX, hit, count.rays.trace);
                        double value[3];
                        bool back = false;
                        gi_hit_radiance(scene, sky, r, light.data(), hit, direction, count.rays, value, back);
                        back_rays += back;
                        double basis[9];
                        gi_sh_basis(direction, basis);
                        for (int k = 0; k < 9; ++k)
                            for (int c = 0; c < 3; ++c)
                                sum[k][c] += value[c] * basis[k];
                    }
                    for (int k = 0; k < 9; ++k)
                        for (int c = 0; c < 3; ++c)
                            probes.sh[p * L::values + k * 3 + c] = float(band[k] * weight * sum[k][c]);
                    probes.valid[p] = back_rays <= L::back_limit * rays;
                    ++count.fraction[std::min<std::uint32_t>(9, back_rays * 10 / rays)];
                }
                const auto finished = done.fetch_add(end - first) + (end - first);
                if (progress && total && (finished * 20 / total != (finished - (end - first)) * 20 / total))
                {
                    std::lock_guard lock(report_lock);
                    if (finished > reported)
                    {
                        reported = finished;
                        progress("probes", finished, total);
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
            for (std::size_t i = 0; i < count.fraction.size(); ++i)
                report.back_fraction[i] += count.fraction[i];
        }
        report.rays = report.trace.rays;
        for (const auto v : probes.valid)
            report.valid += v;
        out = std::move(probes);
        report.ms = elapsed_ms(start);
        return true;
    }

    bool GiProbeField::build(GiLightLattice lattice, GiProbes probes, std::string& error)
    {
        *this = {};
        if (!check_gi_probes(lattice, probes, error))
            return false;
        if (!lattice.spacing || !lattice.dims[0] || !lattice.dims[1] || !lattice.dims[2])
        {
            if (!lattice.probes.empty())
                return error = "GI probes: a lattice without nodes holds probes", false;
        }
        const std::uint64_t nodes = std::uint64_t(lattice.dims[0]) * lattice.dims[1] * lattice.dims[2];
        for (std::size_t i = 0; i < lattice.probes.size(); ++i)
            if (lattice.probes[i].node >= nodes || (i && lattice.probes[i].node <= lattice.probes[i - 1].node))
                return error = "GI probes: lattice nodes out of range or order", false;
        lattice_ = std::move(lattice);
        probes_ = std::move(probes);
        for (const auto v : probes_.valid)
            valid_ += v != 0;
        return true;
    }
    std::int64_t GiProbeField::probe_at(std::uint32_t node) const
    {
        const auto& list = lattice_.probes;
        const auto at = std::lower_bound(list.begin(), list.end(), node,
                                         [](const GiLightProbe& p, std::uint32_t n) { return p.node < n; });
        return at != list.end() && at->node == node ? at - list.begin() : -1;
    }
    namespace
    {
        // Lattice coordinates of a point: the cell's first node and the fractions inside it.
        void locate(const GiLightLattice& lattice, DVector point, std::uint32_t base[3], double fraction[3])
        {
            const double p[3]{point.x, point.y, point.z}, o[3]{lattice.origin.x, lattice.origin.y, lattice.origin.z};
            for (int a = 0; a < 3; ++a)
            {
                const auto dim = lattice.dims[a];
                if (!dim || !lattice.spacing)
                {
                    base[a] = 0, fraction[a] = 0;
                    continue;
                }
                double g = (p[a] - o[a]) / lattice.spacing;
                if (!(g > 0))
                    g = 0; // NaN included.
                g = std::min(g, double(dim - 1));
                base[a] = dim > 1 ? std::min<std::uint32_t>(std::uint32_t(g), dim - 2) : 0;
                fraction[a] = dim > 1 ? g - base[a] : 0;
            }
        }
    } // namespace
    void GiProbeField::cell(DVector point, std::array<std::uint32_t, 3>& out) const
    {
        std::uint32_t base[3];
        double fraction[3];
        locate(lattice_, point, base, fraction);
        out = {base[0], base[1], base[2]};
    }
    bool GiProbeField::sample(DVector point, GiProbeSample& out) const
    {
        out = {};
        out.probes.fill(-1);
        if (probes_.size() == 0 || !lattice_.spacing)
            return false;
        double fraction[3]{};
        std::uint32_t base[3]{};
        locate(lattice_, point, base, fraction);
        out.cell = {base[0], base[1], base[2]};
        double total = 0, weights[8]{};
        for (int corner = 0; corner < 8; ++corner)
        {
            const std::uint32_t x = base[0] + (corner & 1), y = base[1] + (corner >> 1 & 1),
                                z = base[2] + (corner >> 2 & 1);
            const double w = (corner & 1 ? fraction[0] : 1 - fraction[0]) *
                             (corner & 2 ? fraction[1] : 1 - fraction[1]) *
                             (corner & 4 ? fraction[2] : 1 - fraction[2]);
            if (x >= lattice_.dims[0] || y >= lattice_.dims[1] || z >= lattice_.dims[2])
                continue;
            const auto probe = probe_at(x + lattice_.dims[0] * (y + lattice_.dims[1] * z));
            out.probes[corner] = probe;
            if (probe < 0 || !probes_.valid[std::size_t(probe)] || w <= 0)
                continue;
            weights[corner] = w;
            total += w;
        }
        if (!(total > 0))
            return false;
        double sh[27]{};
        for (int corner = 0; corner < 8; ++corner)
        {
            if (weights[corner] <= 0)
                continue;
            const double w = weights[corner] / total;
            out.weights[corner] = float(w);
            const float* values = probes_.sh.data() + std::size_t(out.probes[corner]) * L::values;
            for (std::uint32_t k = 0; k < L::values; ++k)
                sh[k] += w * values[k];
        }
        for (std::uint32_t k = 0; k < L::values; ++k)
            out.sh[k] = float(sh[k]);
        out.valid = true;
        return true;
    }

    GiProbeLight gi_probe_model_light(std::span<const float, 27> sh, float scale, float floor, DVector ambient_light)
    {
        GiProbeLight out;
        const double a_light[3]{ambient_light.x, ambient_light.y, ambient_light.z};
        double e0[3], g[3][3]; // g[channel] = (x, y, z).
        for (int c = 0; c < 3; ++c)
        {
            e0[c] = scale * y0 * sh[c];
            g[c][0] = scale * y1 * sh[3 * 3 + c];
            g[c][1] = scale * y1 * sh[1 * 3 + c];
            g[c][2] = scale * y1 * sh[2 * 3 + c];
        }
        double d[3]{};
        for (int axis = 0; axis < 3; ++axis)
            d[axis] = .2126 * g[0][axis] + .7152 * g[1][axis] + .0722 * g[2][axis];
        const double length = std::sqrt(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);
        const bool directed = length > 1e-12 && std::isfinite(length);
        if (directed)
        {
            for (auto& v : d)
                v /= length;
            out.direction = {float(d[0]), float(d[1]), float(d[2])};
        }
        double ambient[3], color[3];
        for (int c = 0; c < 3; ++c)
        {
            const double mean = std::isfinite(e0[c]) ? std::max(0.0, e0[c]) : 0.0;
            double lobe = 0;
            if (directed)
            {
                const double along = g[c][0] * d[0] + g[c][1] * d[1] + g[c][2] * d[2];
                lobe = std::isfinite(along) ? std::min(2 * std::max(0.0, along), 4 * mean) : 0.0;
            }
            const double base = std::max(0.0, mean - lobe / 4);
            const double fill = floor * a_light[c];
            ambient[c] = std::max(base, fill);
            color[c] = std::max(base + lobe, fill) - ambient[c];
        }
        out.ambient = {float(ambient[0]), float(ambient[1]), float(ambient[2])};
        out.color = {float(color[0]), float(color[1]), float(color[2])};
        return out;
    }
} // namespace OpenBlood2
