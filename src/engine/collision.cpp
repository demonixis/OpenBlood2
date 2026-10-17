#include "engine/collision.h"
#include <algorithm>
#include <cmath>
#include <limits>

namespace OpenBlood2
{
    DVector rotate_vector(DVector v, DRotation q)
    {
        const auto cross = [](DVector a, DVector b)
        { return DVector{a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x}; };
        return vector_add(v, vector_scale(cross(q.m_Vec, vector_add(cross(q.m_Vec, v), vector_scale(v, q.m_Spin))), 2));
    }
    DRotation inverse_rotation(DRotation q)
    {
        return {vector_scale(q.m_Vec, -1), q.m_Spin};
    }
    DRotation multiply_rotation(DRotation a, DRotation b)
    {
        auto x = a.m_Vec, y = b.m_Vec;
        return {vector_add(vector_add(vector_scale(y, a.m_Spin), vector_scale(x, b.m_Spin)),
                           {x.y * y.z - x.z * y.y, x.z * y.x - x.x * y.z, x.x * y.y - x.y * y.x}),
                a.m_Spin * b.m_Spin - vector_dot(x, y)};
    }
    bool CollisionWorld::transform(DRotation rotation, DVector origin, DVector destination)
    {
        for (auto& triangle : triangles_)
        {
            for (auto& point : triangle.points)
            {
                point = vector_add(destination, rotate_vector(vector_subtract(point, origin), rotation));
                if (!physics_vector(point))
                    return false;
            }
            triangle.min = triangle.max = triangle.points[0];
            for (auto point : triangle.points)
            {
                triangle.min = {std::min(triangle.min.x, point.x), std::min(triangle.min.y, point.y),
                                std::min(triangle.min.z, point.z)};
                triangle.max = {std::max(triangle.max.x, point.x), std::max(triangle.max.y, point.y),
                                std::max(triangle.max.z, point.z)};
            }
        }
        nodes_.clear();
        if (!triangles_.empty())
            partition(0, static_cast<std::uint32_t>(triangles_.size()));
        return true;
    }

    DVector vector_add(DVector a, DVector b)
    {
        return {a.x + b.x, a.y + b.y, a.z + b.z};
    }
    DVector vector_subtract(DVector a, DVector b)
    {
        return {a.x - b.x, a.y - b.y, a.z - b.z};
    }
    DVector vector_scale(DVector a, float s)
    {
        return {a.x * s, a.y * s, a.z * s};
    }
    float vector_dot(DVector a, DVector b)
    {
        return a.x * b.x + a.y * b.y + a.z * b.z;
    }
    bool physics_vector(DVector v)
    {
        return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z) && std::abs(v.x) <= 1e7f &&
               std::abs(v.y) <= 1e7f && std::abs(v.z) <= 1e7f;
    }
    namespace
    {
        double precise_dot(DVector a, DVector b)
        {
            return double(a.x) * b.x + double(a.y) * b.y + double(a.z) * b.z;
        }
        DVector cross(DVector a, DVector b)
        {
            return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
        }
        DVector minv(DVector a, DVector b)
        {
            return {std::min(a.x, b.x), std::min(a.y, b.y), std::min(a.z, b.z)};
        }
        DVector maxv(DVector a, DVector b)
        {
            return {std::max(a.x, b.x), std::max(a.y, b.y), std::max(a.z, b.z)};
        }
        bool extents(DVector v)
        {
            return physics_vector(v) && v.x >= 0 && v.y >= 0 && v.z >= 0;
        }
        constexpr std::array<DVector, 3> axes{{{1, 0, 0}, {0, 1, 0}, {0, 0, 1}}};
        // Swept separating-axis intervals. An already touching face permits movement
        // parallel to or away from it; genuine initial penetration is reported.
        struct Sweep
        {
            DVector from, delta, half, normal{}, penetration_normal{};
            double enter = -std::numeric_limits<double>::infinity(), leave = 1,
                   penetration = std::numeric_limits<double>::infinity();
            const PlacementAnchor* anchor = nullptr;
            bool axis(DVector axis, double low, double high)
            {
                const double length = std::sqrt(precise_dot(axis, axis));
                if (length < 1e-8)
                    return true;
                // Use the same double projection for bounds and the moving
                // center. Rounding only the normalized float axis can separate
                // zero-width triangle intervals and miss rays on rotated faces.
                const double radius = (std::abs(double(axis.x)) * half.x + std::abs(double(axis.y)) * half.y +
                                       std::abs(double(axis.z)) * half.z) /
                                      length;
                const double origin = precise_dot(from, axis) / length, speed = precise_dot(delta, axis) / length;
                const double anchor_origin = anchor ? precise_dot(anchor->center, axis) / length : 0;
                const double anchor_radius =
                    anchor ? (std::abs(double(axis.x)) * anchor->half.x + std::abs(double(axis.y)) * anchor->half.y +
                              std::abs(double(axis.z)) * anchor->half.z) /
                                 length
                           : 0;
                axis = {float(axis.x / length), float(axis.y / length), float(axis.z / length)};
                low /= length;
                high /= length;
                // During initial placement, only use an axis that separates the
                // clear source body from this triangle, and keep that same side.
                // The nearest escape alone could push the new body through a floor.
                double sign = origin < (low + high) / 2 ? -1 : 1;
                if (anchor)
                    sign = anchor_origin - anchor_radius >= high - 1e-6  ? 1
                           : anchor_origin + anchor_radius <= low + 1e-6 ? -1
                                                                         : 0;
                low -= radius;
                high += radius;
                const double depth = sign < 0   ? origin - low
                                     : sign > 0 ? high - origin
                                                : std::numeric_limits<double>::infinity();
                if (depth < penetration)
                {
                    penetration = depth;
                    penetration_normal = vector_scale(axis, static_cast<float>(sign));
                }
                if (std::abs(speed) < 1e-10)
                {
                    if (radius == 0 && high - low > 1e-6)
                        return origin >= low - 1e-6 && origin <= high + 1e-6;
                    return origin > low + 1e-6 && origin < high - 1e-6;
                }
                double first = (low - origin) / speed, last = (high - origin) / speed;
                DVector n = vector_scale(axis, speed > 0 ? -1.0f : 1.0f);
                if (first > last)
                    std::swap(first, last);
                if (first > enter)
                {
                    enter = first;
                    normal = n;
                }
                leave = std::min(leave, last);
                return enter <= leave + 1e-9 && leave > 1e-8 && enter <= 1;
            }
            bool finish(CollisionHit& hit) const
            {
                if (enter > leave + 1e-9 || leave <= 1e-8 || enter > 1)
                    return false;
                hit.start_solid = enter < -1e-6;
                hit.penetration = hit.start_solid ? penetration : 0;
                hit.fraction = std::max(0.0, enter);
                hit.normal = hit.start_solid ? penetration_normal : normal;
                hit.point = vector_add(from, vector_scale(delta, static_cast<float>(hit.fraction)));
                return true;
            }
        };
        bool triangle_hit(const std::array<DVector, 3>& p, DVector from, DVector to, DVector half, CollisionHit& hit,
                          const PlacementAnchor* anchor = nullptr)
        {
            Sweep s{from, vector_subtract(to, from), half};
            s.anchor = anchor;
            const std::array<DVector, 3> edges{
                {vector_subtract(p[1], p[0]), vector_subtract(p[2], p[1]), vector_subtract(p[0], p[2])}};
            auto test = [&](DVector axis)
            {
                const double a = precise_dot(axis, p[0]), b = precise_dot(axis, p[1]), c = precise_dot(axis, p[2]);
                return s.axis(axis, std::min({a, b, c}), std::max({a, b, c}));
            };
            for (auto axis : axes)
                if (!test(axis))
                    return false;
            if (!test(cross(edges[0], edges[1])))
                return false;
            for (auto edge : edges)
                for (auto axis : axes)
                    if (!test(cross(edge, axis)))
                        return false;
            return s.finish(hit);
        }
        bool broadphase(DVector from, DVector to, DVector half, DVector min, DVector max)
        {
            auto lo = vector_subtract(minv(from, to), half), hi = vector_add(maxv(from, to), half);
            return lo.x <= max.x && hi.x >= min.x && lo.y <= max.y && hi.y >= min.y && lo.z <= max.z && hi.z >= min.z;
        }
    } // namespace
    CollisionQuery sweep_box(DVector from, DVector to, DVector half, DVector center, DVector other_half,
                             CollisionHit& hit)
    {
        hit = {};
        if (!physics_vector(from) || !physics_vector(to) || !extents(half) || !physics_vector(center) ||
            !extents(other_half))
            return CollisionQuery::Invalid;
        Sweep s{from, vector_subtract(to, from), half};
        auto low = vector_subtract(center, other_half), high = vector_add(center, other_half);
        for (auto axis : axes)
            if (!s.axis(axis, vector_dot(low, axis), vector_dot(high, axis)))
                return CollisionQuery::Miss;
        return s.finish(hit) ? CollisionQuery::Hit : CollisionQuery::Miss;
    }
    bool CollisionWorld::build(const WorldModel& model, std::string& error)
    {
        clear();
        error.clear();
        auto fail = [&]
        {
            clear();
            error = "Invalid or excessive primary collision geometry";
            return false;
        };
        if (model.polygons.size() > 2000000)
            return fail();
        for (std::uint32_t i = 0; i < model.polygons.size(); ++i)
        {
            const auto& polygon = model.polygons[i];
            if (polygon.surface >= model.surfaces.size() || polygon.points.size() < 3)
                return fail();
            // SURF_SOLID = bit 0 in the donor and LithTech world sources.
            if (!(model.surfaces[polygon.surface].flags & 1))
                continue;
            for (auto index : polygon.points)
                if (index >= model.points.size() || !physics_vector(model.points[index]))
                    return fail();
            for (std::size_t j = 1; j + 1 < polygon.points.size(); ++j)
            {
                if (triangles_.size() >= 2000000)
                    return fail();
                Triangle t{{model.points[polygon.points[0]], model.points[polygon.points[j]],
                            model.points[polygon.points[j + 1]]},
                           {},
                           {},
                           i,
                           model.surfaces[polygon.surface].texture_flags};
                auto normal =
                    cross(vector_subtract(t.points[1], t.points[0]), vector_subtract(t.points[2], t.points[0]));
                if (vector_dot(normal, normal) < 1e-12f)
                    continue;
                t.min = minv(t.points[0], minv(t.points[1], t.points[2]));
                t.max = maxv(t.points[0], maxv(t.points[1], t.points[2]));
                triangles_.push_back(t);
            }
        }
        if (!triangles_.empty())
            partition(0, static_cast<std::uint32_t>(triangles_.size()));
        return true;
    }
    std::uint32_t CollisionWorld::partition(std::uint32_t begin, std::uint32_t end)
    {
        Node n;
        n.begin = begin;
        n.end = end;
        n.min = triangles_[begin].min;
        n.max = triangles_[begin].max;
        for (auto i = begin + 1; i < end; ++i)
        {
            n.min = minv(n.min, triangles_[i].min);
            n.max = maxv(n.max, triangles_[i].max);
        }
        auto index = static_cast<std::uint32_t>(nodes_.size());
        nodes_.push_back(n);
        if (end - begin > 8)
        {
            auto size = vector_subtract(n.max, n.min);
            auto axis = size.x >= size.y && size.x >= size.z ? axes[0] : size.y >= size.z ? axes[1] : axes[2];
            auto mid = begin + (end - begin) / 2;
            std::nth_element(
                triangles_.begin() + begin, triangles_.begin() + mid, triangles_.begin() + end,
                [axis](const auto& a, const auto& b)
                { return vector_dot(vector_add(a.min, a.max), axis) < vector_dot(vector_add(b.min, b.max), axis); });
            auto left = partition(begin, mid), right = partition(mid, end);
            nodes_[index].left = left;
            nodes_[index].right = right;
        }
        return index;
    }
    void CollisionWorld::query(std::uint32_t index, DVector from, DVector to, DVector half, CollisionHit& hit,
                               bool& found, const PlacementAnchor* anchor) const
    {
        const auto& n = nodes_[index];
        if (!broadphase(from, to, half, n.min, n.max))
            return;
        if (n.left)
        {
            query(n.left, from, to, half, hit, found, anchor);
            query(n.right, from, to, half, hit, found, anchor);
            return;
        }
        for (auto i = n.begin; i < n.end; ++i)
        {
            CollisionHit candidate;
            if (triangle_hit(triangles_[i].points, from, to, half, candidate, anchor) &&
                (!found || candidate.fraction < hit.fraction ||
                 (candidate.fraction == hit.fraction && triangles_[i].polygon < hit.polygon)))
            {
                hit = candidate;
                hit.polygon = triangles_[i].polygon;
                hit.texture_flags = triangles_[i].texture_flags;
                found = true;
            }
        }
    }
    CollisionQuery CollisionWorld::sweep(DVector from, DVector to, DVector half, CollisionHit& hit) const
    {
        hit = {};
        if (!physics_vector(from) || !physics_vector(to) || !extents(half))
            return CollisionQuery::Invalid;
        if (nodes_.empty())
            return CollisionQuery::Miss;
        bool found = false;
        query(0, from, to, half, hit, found);
        return found ? CollisionQuery::Hit : CollisionQuery::Miss;
    }
    const char* body_placement_name(BodyPlacementResult result)
    {
        switch (result)
        {
            case BodyPlacementResult::Unqueried:
                return "not queried";
            case BodyPlacementResult::Placed:
                return "placed unchanged";
            case BodyPlacementResult::Adjusted:
                return "placed with clearance correction";
            case BodyPlacementResult::Blocked:
                return "blocked";
            case BodyPlacementResult::Invalid:
                return "invalid";
            case BodyPlacementResult::Unavailable:
                return "collision unavailable";
        }
        return "invalid";
    }
    BodyPlacement CollisionWorld::place_body(DVector anchor, DVector anchor_half, DVector requested, DVector half) const
    {
        BodyPlacement report;
        report.anchor = anchor;
        report.requested = report.candidate = requested;
        report.result = BodyPlacementResult::Invalid;
        const auto positive = [](DVector d) { return extents(d) && d.x > 0 && d.y > 0 && d.z > 0; };
        if (!physics_vector(anchor) || !physics_vector(requested) || !positive(anchor_half) || !positive(half))
            return report;
        CollisionHit hit;
        if (sweep(anchor, anchor, anchor_half, hit) != CollisionQuery::Miss)
        {
            report.result = BodyPlacementResult::Blocked; // Never repair an already penetrating source body.
            report.polygon = hit.polygon;
            return report;
        }
        // Bounded replacement spawn placement, not a retail depenetration solver.
        // Keep an owned candidate until the complete shape AND its center path from
        // the clear source fit. Never tunnel across a wall to the nearer empty side.
        const double limit = std::min(256.f, std::max({half.x, half.y, half.z}));
        const PlacementAnchor reference{anchor, anchor_half};
        for (unsigned attempt = 0; attempt <= 16; ++attempt)
        {
            if (!physics_vector(report.candidate))
                return report;
            hit = {};
            bool found = false;
            if (!nodes_.empty())
                query(0, report.candidate, report.candidate, half, hit, found, &reference);
            if (!found)
            {
                if (sweep(anchor, report.candidate, {}, hit) != CollisionQuery::Miss)
                {
                    report.result = BodyPlacementResult::Blocked;
                    report.polygon = hit.polygon;
                    return report;
                }
                report.result = report.corrections ? BodyPlacementResult::Adjusted : BodyPlacementResult::Placed;
                return report;
            }
            report.polygon = hit.polygon;
            if (!hit.start_solid || !std::isfinite(hit.penetration) || hit.penetration <= 0 ||
                !physics_vector(hit.normal))
                return report;
            const auto distance = hit.penetration + .001;
            if (attempt == 16 || report.distance + distance > limit)
            {
                report.result = BodyPlacementResult::Blocked;
                return report;
            }
            report.candidate = vector_add(report.candidate, vector_scale(hit.normal, static_cast<float>(distance)));
            report.distance += distance;
            ++report.corrections;
        }
        return report;
    }
} // namespace OpenBlood2
