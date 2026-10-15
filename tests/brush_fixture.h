#pragma once
#include "engine/collision.h"
#include <cmath>
namespace test_brush
{
    using namespace OpenBlood2;
    inline void face(WorldModel& model, std::vector<DVector> points, DVector normal)
    {
        const auto surface = static_cast<unsigned>(model.surfaces.size());
        model.planes.push_back({normal, vector_dot(normal, points[0])});
        model.surfaces.push_back({});
        model.surfaces.back().plane = surface;
        Polygon polygon;
        polygon.surface = surface;
        for (auto p : points)
        {
            polygon.points.push_back(static_cast<std::uint16_t>(model.points.size()));
            model.points.push_back(p);
        }
        model.polygons.push_back(std::move(polygon));
    }
    inline WorldModel prism(std::string name, const std::vector<DVector>& outline, float bottom, float top)
    {
        WorldModel m;
        m.name = std::move(name);
        for (std::size_t i = 0; i < outline.size(); ++i)
        {
            const auto a = outline[i], b = outline[(i + 1) % outline.size()];
            const auto length = std::hypot(b.x - a.x, b.z - a.z);
            face(m, {{a.x, bottom, a.z}, {b.x, bottom, b.z}, {b.x, top, b.z}, {a.x, top, a.z}},
                 {(b.z - a.z) / length, 0, (a.x - b.x) / length});
        }
        std::vector<DVector> lo, hi;
        for (auto p : outline)
        {
            lo.push_back({p.x, bottom, p.z});
            hi.push_back({p.x, top, p.z});
        }
        face(m, lo, {0, -1, 0});
        face(m, hi, {0, 1, 0});
        return m;
    }
    inline WorldModel box(std::string name, DVector min, DVector max)
    {
        return prism(std::move(name), {{min.x, 0, min.z}, {max.x, 0, min.z}, {max.x, 0, max.z}, {min.x, 0, max.z}},
                     min.y, max.y);
    }
} // namespace test_brush
