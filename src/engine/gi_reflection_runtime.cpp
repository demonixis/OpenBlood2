#include "engine/gi_reflection_runtime.h"
#include "engine/gi_probes.h"
#include <algorithm>
#include <cmath>
#include <map>

namespace OpenBlood2
{
    namespace
    {
        namespace L = gi_reflection_runtime_limits;
        float mode_value(GiReflectionMode mode)
        {
            return mode == GiReflectionMode::High ? 2.f : mode == GiReflectionMode::Low ? 1.f : 0.f;
        }
        double volume(const GiBox& b)
        {
            return std::max(0.0, double(b.max.x) - b.min.x) * std::max(0.0, double(b.max.y) - b.min.y) *
                   std::max(0.0, double(b.max.z) - b.min.z);
        }
        double clamp01(double v)
        {
            return v < 0 ? 0 : v > 1 ? 1 : v;
        }
    } // namespace

    const char* gi_reflection_mode_name(GiReflectionMode mode)
    {
        switch (mode)
        {
            case GiReflectionMode::Off:
                return "OFF";
            case GiReflectionMode::Low:
                return "LOW";
            case GiReflectionMode::High:
                return "HIGH";
        }
        return "?";
    }
    bool valid_gi_reflection_view(const GiReflectionView& v)
    {
        return v.intensity >= 0 && v.intensity <= L::max_intensity;
    }
    std::size_t GiReflectionGpu::level_offset(std::uint32_t layer, std::uint32_t level) const
    {
        std::size_t chain = 0, offset = 0;
        for (std::uint32_t l = 0; l < levels; ++l)
        {
            const std::size_t s = std::max<std::uint32_t>(1, size >> l);
            if (l < level)
                offset += s * s;
            chain += s * s;
        }
        return std::size_t(layer) * chain + offset;
    }

    GiReflectionGpu gi_reflection_placeholder()
    {
        GiReflectionGpu g;
        g.texels.assign(4, 0);
        g.records.assign(L::header, {0, 0, 0, 0});
        g.records[0] = {0, 0, 0, 1};
        g.records[1] = {0, 1, 1, 0};
        g.cells.assign(4, 0);
        return g;
    }

    bool gi_reflection_gpu(const GiReflections& refl, std::span<const GiReflectionProbe> probes, GiReflectionSet set,
                           GiReflectionMode mode, GiReflectionGpu& out, std::string& error)
    {
        if (mode == GiReflectionMode::Off)
            return error = "Reflection probes are off", false;
        if (std::uint32_t(set) >= gi_reflection_limits::sets)
            return error = "Invalid reflection set", false;
        if (!check_gi_reflections(probes, refl, error))
            return false;
        if (refl.texels.size() != gi_reflection_limits::sets * refl.size() *
                                      (refl.size() ? gi_reflection_chain_texels(refl.resolution) : 0) ||
            refl.valid.size() != refl.size())
            return error = "GI reflections: the texels do not match the probes", false;
        auto g = gi_reflection_placeholder();
        g.set = set;
        g.mode = mode;
        for (std::uint32_t p = 0; p < refl.size(); ++p)
            if (refl.valid[p])
                g.probe_of_layer.push_back(p);
        if (!g.probe_of_layer.empty())
        {
            g.chain_levels = refl.levels;
            g.dropped = mode == GiReflectionMode::Low && refl.levels > 1 ? 1 : 0;
            g.levels = refl.levels - g.dropped;
            g.size = refl.resolution >> g.dropped;
            g.layers = std::uint32_t(g.probe_of_layer.size());
            g.texels.assign(g.level_offset(g.layers, 0) * 4, 0);
            for (std::uint32_t layer = 0; layer < g.layers; ++layer)
                for (std::uint32_t level = 0; level < g.levels; ++level)
                {
                    const auto* from = refl.texels.data() + refl.first(set, g.probe_of_layer[layer], level + g.dropped);
                    auto* to = g.texels.data() + g.level_offset(layer, level) * 4;
                    const std::size_t s = g.size >> level;
                    for (std::size_t t = 0; t < s * s; ++t)
                    {
                        const auto c = gi_rgb9e5_decode(from[t]);
                        to[t * 4 + 0] = gi_half(c.x);
                        to[t * 4 + 1] = gi_half(c.y);
                        to[t * 4 + 2] = gi_half(c.z);
                        to[t * 4 + 3] = gi_half(1.f);
                    }
                }
            // The cell grid over the union of the influence boxes.
            DVector lo = probes[g.probe_of_layer[0]].influence.min, hi = probes[g.probe_of_layer[0]].influence.max;
            for (auto p : g.probe_of_layer)
            {
                const auto& b = probes[p].influence;
                lo = {std::min(lo.x, b.min.x), std::min(lo.y, b.min.y), std::min(lo.z, b.min.z)};
                hi = {std::max(hi.x, b.max.x), std::max(hi.y, b.max.y), std::max(hi.z, b.max.z)};
            }
            lo = {float(lo.x - L::margin), float(lo.y - L::margin), float(lo.z - L::margin)};
            hi = {float(hi.x + L::margin), float(hi.y + L::margin), float(hi.z + L::margin)};
            double cell = L::min_cell;
            auto dims = [&](double c)
            {
                return std::array<double, 3>{std::max(1.0, std::ceil((hi.x - lo.x) / c)),
                                             std::max(1.0, std::ceil((hi.y - lo.y) / c)),
                                             std::max(1.0, std::ceil((hi.z - lo.z) / c))};
            };
            for (auto d = dims(cell); d[0] > L::axis_cells || d[1] > L::axis_cells || d[2] > L::axis_cells ||
                                      d[0] * d[1] * d[2] > L::cells;
                 d = dims(cell))
                cell *= 2;
            const auto d = dims(cell);
            g.dimensions = {std::uint32_t(d[0]), std::uint32_t(d[1]), std::uint32_t(d[2])};
            g.origin = lo;
            g.cell_size = cell;
            const std::size_t count = std::size_t(g.dimensions[0]) * g.dimensions[1] * g.dimensions[2];
            std::vector<std::vector<std::uint32_t>> lists(count);
            for (std::uint32_t layer = 0; layer < g.layers; ++layer)
            {
                const auto& b = probes[g.probe_of_layer[layer]].influence;
                auto range = [&](double a, double z, double o, std::uint32_t n, std::uint32_t& first,
                                 std::uint32_t& last)
                {
                    const auto f = std::floor((a - L::margin - o) / cell), l = std::floor((z + L::margin - o) / cell);
                    first = std::uint32_t(std::clamp(f, 0.0, double(n - 1)));
                    last = std::uint32_t(std::clamp(l, 0.0, double(n - 1)));
                };
                std::uint32_t x0, x1, y0, y1, z0, z1;
                range(b.min.x, b.max.x, lo.x, g.dimensions[0], x0, x1);
                range(b.min.y, b.max.y, lo.y, g.dimensions[1], y0, y1);
                range(b.min.z, b.max.z, lo.z, g.dimensions[2], z0, z1);
                for (auto z = z0; z <= z1; ++z)
                    for (auto y = y0; y <= y1; ++y)
                        for (auto x = x0; x <= x1; ++x)
                            lists[(std::size_t(z) * g.dimensions[1] + y) * g.dimensions[0] + x].push_back(layer);
            }
            g.cells.assign(4 + count, 0);
            g.cells[0] = g.dimensions[0], g.cells[1] = g.dimensions[1], g.cells[2] = g.dimensions[2];
            g.cells[3] = std::uint32_t(count);
            std::map<std::vector<std::uint32_t>, std::uint32_t> shared;
            std::vector<std::uint32_t> indices;
            for (std::size_t c = 0; c < count; ++c)
            {
                auto& list = lists[c];
                if (list.empty())
                    continue;
                ++g.occupied;
                if (list.size() > L::cell_probes)
                {
                    // The highest priorities, then the smallest boxes, then the lowest indices.
                    ++g.overflow;
                    std::stable_sort(list.begin(), list.end(),
                                     [&](std::uint32_t a, std::uint32_t b)
                                     {
                                         const auto &pa = probes[g.probe_of_layer[a]], &pb = probes[g.probe_of_layer[b]];
                                         if (pa.priority != pb.priority)
                                             return pa.priority > pb.priority;
                                         const auto va = volume(pa.influence), vb = volume(pb.influence);
                                         return va != vb ? va < vb : a < b;
                                     });
                    list.resize(L::cell_probes);
                    std::sort(list.begin(), list.end());
                }
                auto [at, added] = shared.emplace(list, std::uint32_t(indices.size()));
                if (added)
                    indices.insert(indices.end(), list.begin(), list.end());
                g.cells[4 + c] = at->second << 4 | std::uint32_t(list.size());
            }
            g.lists = shared.size();
            g.cells.insert(g.cells.end(), indices.begin(), indices.end());
            g.records.resize(L::header + L::record * g.layers);
            g.records[2] = {float(lo.x), float(lo.y), float(lo.z), float(1 / cell)};
            for (std::uint32_t layer = 0; layer < g.layers; ++layer)
            {
                const auto& p = probes[g.probe_of_layer[layer]];
                auto* r = &g.records[L::header + L::record * layer];
                r[0] = {float(p.influence.min.x), float(p.influence.min.y), float(p.influence.min.z), p.blend_distance};
                r[1] = {float(p.influence.max.x), float(p.influence.max.y), float(p.influence.max.z), float(p.priority)};
                r[2] = {float(p.projection.min.x), float(p.projection.min.y), float(p.projection.min.z),
                        p.box_projection ? 1.f : 0.f};
                r[3] = {float(p.projection.max.x), float(p.projection.max.y), float(p.projection.max.z), float(layer)};
                r[4] = {float(p.capture.x), float(p.capture.y), float(p.capture.z), 0};
            }
        }
        g.records[0] = {float(g.layers), g.layers ? mode_value(mode) : 0.f, float(g.chain_levels), 1};
        g.records[1] = {float(g.dropped), float(g.levels), float(g.size), 0};
        out = std::move(g);
        return true;
    }

    void gi_reflection_header(GiReflectionGpu& g, const GiReflectionView& view)
    {
        if (g.records.size() < L::header)
            return;
        g.records[0][3] = valid_gi_reflection_view(view) ? view.intensity : 1.f;
        g.records[1][3] = view.only ? 1.f : 0.f;
    }

    namespace
    {
        DVector sample(const GiReflectionGpu& g, std::uint32_t layer, double u, double v, std::uint32_t level)
        {
            const int s = int(std::max<std::uint32_t>(1, g.size >> level));
            const double x = u * s - .5, y = v * s - .5;
            const int x0 = int(std::floor(x)), y0 = int(std::floor(y));
            const double fx = x - x0, fy = y - y0;
            const auto* base = g.texels.data() + g.level_offset(layer, level) * 4;
            double out[3]{};
            for (int j = 0; j < 2; ++j)
                for (int i = 0; i < 2; ++i)
                {
                    const int tx = std::clamp(x0 + i, 0, s - 1), ty = std::clamp(y0 + j, 0, s - 1);
                    const double w = (i ? fx : 1 - fx) * (j ? fy : 1 - fy);
                    for (int c = 0; c < 3; ++c)
                        out[c] += w * gi_half_decode(base[(std::size_t(ty) * s + tx) * 4 + c]);
                }
            return {float(out[0]), float(out[1]), float(out[2])};
        }
        DVector probe_sample(const GiReflectionGpu& g, std::uint32_t layer, DVector d, double level)
        {
            double u, v;
            gi_octahedral_coordinates(d.x, d.y, d.z, u, v);
            u = u * .5 + .5, v = v * .5 + .5;
            const double l0 = std::floor(level), l1 = std::min(l0 + 1, double(g.levels) - 1);
            const double s0 = g.size * std::exp2(-l0), s1 = g.size * std::exp2(-l1);
            const auto a = sample(g, layer, u * (1 - 2 / s0) + 1 / s0, v * (1 - 2 / s0) + 1 / s0, std::uint32_t(l0));
            const auto b = sample(g, layer, u * (1 - 2 / s1) + 1 / s1, v * (1 - 2 / s1) + 1 / s1, std::uint32_t(l1));
            const double t = level - l0;
            return {float(a.x + (b.x - a.x) * t), float(a.y + (b.y - a.y) * t), float(a.z + (b.z - a.z) * t)};
        }
        double coverage(DVector p, const std::array<float, 4>& lo, const std::array<float, 4>& hi)
        {
            const double d = std::min({p.x - lo[0], p.y - lo[1], p.z - lo[2], hi[0] - p.x, hi[1] - p.y, hi[2] - p.z});
            if (d < 0)
                return 0;
            return lo[3] > 0 ? clamp01(d / lo[3]) : 1;
        }
        DVector probe_radiance(const GiReflectionGpu& g, std::uint32_t index, DVector p, DVector r, double level)
        {
            const auto* rec = &g.records[L::header + L::record * index];
            DVector d = r;
            if (std::uint32_t(rec[2][3]) & 1)
            {
                auto safe = [](double v) { return std::fabs(v) > 1e-6 ? v : 1e-6; };
                const double sx = safe(r.x), sy = safe(r.y), sz = safe(r.z);
                const double fx = std::max((rec[2][0] - p.x) / sx, (rec[3][0] - p.x) / sx);
                const double fy = std::max((rec[2][1] - p.y) / sy, (rec[3][1] - p.y) / sy);
                const double fz = std::max((rec[2][2] - p.z) / sz, (rec[3][2] - p.z) / sz);
                const double t = std::max(std::min({fx, fy, fz}), 0.0);
                d = {float(p.x + r.x * t - rec[4][0]), float(p.y + r.y * t - rec[4][1]),
                     float(p.z + r.z * t - rec[4][2])};
            }
            const auto c = probe_sample(g, std::uint32_t(rec[3][3]), d, level);
            const double k = g.records[0][3];
            return {float(c.x * k), float(c.y * k), float(c.z * k)};
        }
    } // namespace

    DVector gi_reflection_environment(const GiReflectionGpu& g, DVector p, DVector r, double roughness, DVector fallback)
    {
        if (g.records.size() < L::header || g.cells.size() < 4)
            return fallback;
        const auto mode = std::uint32_t(g.records[0][1]);
        if (std::uint32_t(g.records[0][0]) == 0 || mode == 0)
            return fallback;
        const double inv = g.records[2][3];
        const int cx = int(std::floor((p.x - g.records[2][0]) * inv)), cy = int(std::floor((p.y - g.records[2][1]) * inv)),
                  cz = int(std::floor((p.z - g.records[2][2]) * inv));
        const std::uint32_t dx = g.cells[0], dy = g.cells[1], dz = g.cells[2];
        if (cx < 0 || cy < 0 || cz < 0 || std::uint32_t(cx) >= dx || std::uint32_t(cy) >= dy || std::uint32_t(cz) >= dz)
            return fallback;
        const auto packed = g.cells[4 + (std::size_t(cz) * dy + std::uint32_t(cy)) * dx + std::uint32_t(cx)];
        const std::size_t first = 4 + std::size_t(g.cells[3]) + (packed >> 4);
        const std::uint32_t candidates = std::min<std::uint32_t>(packed & 15, L::cell_probes);
        int best0 = -1, best1 = -1;
        double c0 = 0, c1 = 0, pr0 = 0, pr1 = 0, d0 = 0, d1 = 0;
        for (std::uint32_t i = 0; i < candidates; ++i)
        {
            const auto index = g.cells[first + i];
            const auto* rec = &g.records[L::header + L::record * index];
            const double c = coverage(p, rec[0], rec[1]);
            if (c <= 0)
                continue;
            const double priority = rec[1][3];
            const double tx = rec[4][0] - p.x, ty = rec[4][1] - p.y, tz = rec[4][2] - p.z;
            const double d = tx * tx + ty * ty + tz * tz;
            if (best0 < 0 || priority > pr0 || (priority == pr0 && (c > c0 || (c == c0 && d < d0))))
            {
                best1 = best0, c1 = c0, pr1 = pr0, d1 = d0;
                best0 = int(index), c0 = c, pr0 = priority, d0 = d;
            }
            else if (best1 < 0 || priority > pr1 || (priority == pr1 && (c > c1 || (c == c1 && d < d1))))
                best1 = int(index), c1 = c, pr1 = priority, d1 = d;
        }
        if (best0 < 0)
            return fallback;
        double w0 = c0, w1 = 0;
        if (mode >= 2 && best1 >= 0)
        {
            if (pr1 == pr0)
            {
                const double total = c0 + c1, share = std::min(total, 1.0) / total;
                w0 = c0 * share, w1 = c1 * share;
            }
            else
                w1 = (1 - w0) * c1;
        }
        const double level = std::clamp(clamp01(roughness) * (g.records[0][2] - 1) - g.records[1][0], 0.0,
                                        double(g.records[1][1]) - 1);
        const auto a = probe_radiance(g, std::uint32_t(best0), p, r, level);
        double e[3]{fallback.x * (1 - w0 - w1) + a.x * w0, fallback.y * (1 - w0 - w1) + a.y * w0,
                    fallback.z * (1 - w0 - w1) + a.z * w0};
        if (w1 > 0)
        {
            const auto b = probe_radiance(g, std::uint32_t(best1), p, r, level);
            e[0] += b.x * w1, e[1] += b.y * w1, e[2] += b.z * w1;
        }
        return {float(e[0]), float(e[1]), float(e[2])};
    }
} // namespace OpenBlood2
