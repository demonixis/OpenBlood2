#include "engine/mark_clip.h"
#include "engine/collision.h"
#include <algorithm>
#include <cmath>
namespace OpenBlood2
{
    bool clip_mark(std::span<const WorldVertex> quad, const WorldDraw& draw, std::span<const DVector> polygon,
                   std::vector<WorldVertex>& triangles, std::string& error)
    {
        triangles.clear();
        error.clear();
        auto fail = [&]()
        {
            error = "Invalid or non-convex mark clipping polygon";
            return false;
        };
        WorldViewUniform checked;
        if (quad.size() != 6 || polygon.size() < 3 || polygon.size() > 512 ||
            !world_view_uniform({}, {}, 1, draw, checked))
            return fail();
        std::vector<DVector> ring;
        const auto inverse = inverse_rotation(draw.rotation);
        for (auto point : polygon)
        {
            if (!physics_vector(point))
                return fail();
            ring.push_back(rotate_vector(vector_subtract(point, draw.position), inverse));
        }
        double area = 0;
        for (std::size_t i = 0; i < ring.size(); ++i)
        {
            const auto a = ring[i], b = ring[(i + 1) % ring.size()];
            area += double(a.x) * b.y - double(a.y) * b.x;
        }
        if (std::abs(area) < 1e-8)
            return fail();
        const double sign = area > 0 ? 1 : -1;
        auto distance = [&](DVector a, DVector b, DVector p)
        { return sign * ((double(b.x) - a.x) * (double(p.y) - a.y) - (double(b.y) - a.y) * (double(p.x) - a.x)); };
        std::vector<WorldVertex> clipped{quad[0], quad[1], quad[2], quad[5]}, next;
        for (std::size_t i = 0; i < ring.size(); ++i)
        {
            const auto a = ring[i], b = ring[(i + 1) % ring.size()];
            for (auto point : ring)
                if (distance(a, b, point) < -1e-3)
                    return fail();
            next.clear();
            for (std::size_t j = 0; j < clipped.size(); ++j)
            {
                const auto& start = clipped[j];
                const auto& end = clipped[(j + 1) % clipped.size()];
                const auto d0 = distance(a, b, start.position), d1 = distance(a, b, end.position);
                if ((d0 >= 0) != (d1 >= 0))
                {
                    const float t = static_cast<float>(d0 / (d0 - d1));
                    auto vertex = start;
                    vertex.position =
                        vector_add(start.position, vector_scale(vector_subtract(end.position, start.position), t));
                    for (unsigned k = 0; k < 2; ++k)
                        vertex.uv[k] += (end.uv[k] - start.uv[k]) * t;
                    next.push_back(vertex);
                }
                if (d1 >= 0)
                    next.push_back(end);
            }
            clipped.swap(next);
            if (clipped.size() > 516)
                return fail();
        }
        for (std::size_t i = 1; i + 1 < clipped.size(); ++i)
        {
            const auto a = clipped[0].position, b = clipped[i].position, c = clipped[i + 1].position;
            if (std::abs((double(b.x) - a.x) * (double(c.y) - a.y) - (double(b.y) - a.y) * (double(c.x) - a.x)) < 1e-10)
                continue;
            for (auto index : {std::size_t(0), i, i + 1})
                triangles.push_back(clipped[index]);
        }
        return true;
    }
} // namespace OpenBlood2
