#include "engine/light_grid.h"
#include <algorithm>
#include <cmath>

namespace OpenBlood2
{
    bool build_light_grid(std::span<const StaticLight> source, LightGrid& grid, std::string& error)
    {
        grid = {};
        if (source.size() > LightGrid::light_limit)
        {
            error = "Light grid: more than 4096 static lights";
            return false;
        }
        double lo[3]{1e30, 1e30, 1e30}, hi[3]{-1e30, -1e30, -1e30};
        for (const auto& light : source)
        {
            const float p[3]{light.position.x, light.position.y, light.position.z};
            if (!std::isfinite(light.radius) || light.radius <= 0 || light.radius > 1e5f || !std::isfinite(p[0]) ||
                !std::isfinite(p[1]) || !std::isfinite(p[2]) || std::abs(p[0]) > 1e6f || std::abs(p[1]) > 1e6f ||
                std::abs(p[2]) > 1e6f || !std::isfinite(light.color.x) || !std::isfinite(light.color.y) ||
                !std::isfinite(light.color.z) || !std::isfinite(light.cos_half))
            {
                error = "Light grid: invalid static light";
                return false;
            }
            for (int k = 0; k < 3; ++k)
            {
                lo[k] = std::min(lo[k], double(p[k]) - light.radius);
                hi[k] = std::max(hi[k], double(p[k]) + light.radius);
            }
            grid.lights.push_back({p[0], p[1], p[2], light.radius});
            grid.lights.push_back({light.color.x, light.color.y, light.color.z, light.cos_half});
            grid.lights.push_back({light.direction.x, light.direction.y, light.direction.z, 0});
        }
        if (source.empty())
        {
            grid.dimensions = {1, 1, 1};
            grid.cells = {0};
            return true;
        }
        // Smallest cell size (at least minimum_cell) whose grid fits cell_limit.
        double extent[3];
        for (int k = 0; k < 3; ++k)
            extent[k] = std::max(1.0, hi[k] - lo[k]);
        double size = LightGrid::minimum_cell;
        auto cells = [&](double s)
        {
            double n = 1;
            for (int k = 0; k < 3; ++k)
                n *= std::ceil(extent[k] / s);
            return n;
        };
        while (cells(size) > double(LightGrid::cell_limit))
            size *= 1.25;
        grid.cell_size = float(size);
        for (int k = 0; k < 3; ++k)
        {
            grid.origin[k] = float(lo[k]);
            grid.dimensions[k] = std::uint32_t(std::max(1.0, std::ceil(extent[k] / size)));
        }
        const auto count = grid.cell_count();
        struct Candidate
        {
            float strength;
            std::uint32_t light;
        };
        std::vector<std::vector<Candidate>> lists(count);
        for (std::uint32_t l = 0; l < source.size(); ++l)
        {
            const auto& light = source[l];
            const float p[3]{light.position.x, light.position.y, light.position.z};
            const float luminance = .299f * light.color.x + .587f * light.color.y + .114f * light.color.z;
            if (luminance <= 0)
                continue;
            std::uint32_t first[3], last[3];
            for (int k = 0; k < 3; ++k)
            {
                const double a = (p[k] - light.radius - grid.origin[k]) / size;
                const double b = (p[k] + light.radius - grid.origin[k]) / size;
                first[k] = std::uint32_t(std::clamp(std::floor(a), 0.0, double(grid.dimensions[k] - 1)));
                last[k] = std::uint32_t(std::clamp(std::floor(b), 0.0, double(grid.dimensions[k] - 1)));
            }
            for (std::uint32_t z = first[2]; z <= last[2]; ++z)
                for (std::uint32_t y = first[1]; y <= last[1]; ++y)
                    for (std::uint32_t x = first[0]; x <= last[0]; ++x)
                    {
                        const std::uint32_t c[3]{x, y, z};
                        double d2 = 0;
                        for (int k = 0; k < 3; ++k)
                        {
                            const double cell_lo = grid.origin[k] + c[k] * size, cell_hi = cell_lo + size;
                            const double d = p[k] < cell_lo ? cell_lo - p[k] : p[k] > cell_hi ? p[k] - cell_hi : 0;
                            d2 += d * d;
                        }
                        const double distance = std::sqrt(d2);
                        if (distance >= light.radius)
                            continue;
                        lists[(std::size_t(z) * grid.dimensions[1] + y) * grid.dimensions[0] + x].push_back(
                            {float(luminance * (1 - distance / light.radius)), l});
                    }
        }
        grid.cells.assign(count, 0);
        std::vector<std::uint32_t> indices;
        for (std::size_t i = 0; i < count; ++i)
        {
            auto& list = lists[i];
            std::stable_sort(list.begin(), list.end(),
                             [](const Candidate& a, const Candidate& b) { return a.strength > b.strength; });
            if (list.size() > LightGrid::cell_light_limit)
            {
                grid.dropped += list.size() - LightGrid::cell_light_limit;
                list.resize(LightGrid::cell_light_limit);
            }
            if (indices.size() >= (1u << 27))
            {
                error = "Light grid: index budget exceeded";
                grid = {};
                return false;
            }
            grid.cells[i] = std::uint32_t(indices.size()) << 4 | std::uint32_t(list.size());
            for (const auto& candidate : list)
                indices.push_back(candidate.light);
        }
        grid.cells.insert(grid.cells.end(), indices.begin(), indices.end());
        return true;
    }
} // namespace OpenBlood2
