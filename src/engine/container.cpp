#include "engine/container.h"
#include <algorithm>
#include <cmath>
#include <map>
#include <numbers>

namespace OpenBlood2
{
    namespace
    {
        DVector cross(DVector a, DVector b)
        {
            return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
        }
        DVector rotate(DVector v, DRotation q)
        {
            return vector_add(
                v, vector_scale(cross(q.m_Vec, vector_add(cross(q.m_Vec, v), vector_scale(v, q.m_Spin))), 2));
        }
        double dot(DVector a, DVector b)
        {
            return double(a.x) * b.x + double(a.y) * b.y + double(a.z) * b.z;
        }
    } // namespace
    bool ContainerShape::transform(DRotation q, DVector destination)
    {
        if (triangles.empty() || !physics_vector(destination))
            return false;
        for (auto& plane : planes_)
        {
            const auto distance = plane.distance - vector_dot(plane.normal, origin);
            plane.normal = rotate_vector(plane.normal, q);
            plane.distance = distance + vector_dot(plane.normal, destination);
        }
        bool first = true;
        for (auto& triangle : triangles)
            for (auto& point : triangle)
            {
                point = vector_add(destination, rotate_vector(vector_subtract(point, origin), q));
                if (!physics_vector(point))
                    return false;
                if (first)
                {
                    min = max = point;
                    first = false;
                }
                min = {std::min(min.x, point.x), std::min(min.y, point.y), std::min(min.z, point.z)};
                max = {std::max(max.x, point.x), std::max(max.y, point.y), std::max(max.z, point.z)};
            }
        if (!boundary_.transform(q, origin, destination))
            return false;
        origin = destination;
        return true;
    }
    bool ContainerShape::build(const WorldModel& source, DRotation rotation, std::string& error)
    {
        *this = {};
        error.clear();
        auto fail = [&](const char* why)
        {
            error = why;
            *this = {};
            return false;
        };
        if (source.points.size() < 4 || source.points.size() > 4096 || source.polygons.size() < 4 ||
            source.polygons.size() > 4096)
            return fail("Container shell exceeds limits or has too few faces/points");
        const auto norm = dot(rotation.m_Vec, rotation.m_Vec) + double(rotation.m_Spin) * rotation.m_Spin;
        if (!std::isfinite(norm) || std::abs(norm - 1) > 0.001 || !physics_vector(source.translation))
            return fail("Invalid container transform");
        WorldModel mesh = source;
        origin = source.translation;
        for (auto& point : mesh.points)
        {
            point = vector_add(origin, rotate(vector_subtract(point, origin), rotation));
            if (!physics_vector(point))
                return fail("Invalid container vertex");
        }
        min = max = mesh.points.front();
        for (auto p : mesh.points)
        {
            min = {std::min(min.x, p.x), std::min(min.y, p.y), std::min(min.z, p.z)};
            max = {std::max(max.x, p.x), std::max(max.y, p.y), std::max(max.z, p.z)};
        }
        // DAT vertex coordinates are already in world space at WorldTranslation.
        // Rotate about that pivot; DAT Min/Max include another translation in tested v56 files.
        // BSP splits can leave near-identical vertices. Weld topology within 0.01
        // original units; retain the actual vertices/planes for overlap queries.
        std::vector<unsigned> canonical(mesh.points.size());
        for (unsigned i = 0; i < mesh.points.size(); ++i)
        {
            canonical[i] = i;
            for (unsigned j = 0; j < i; ++j)
                if (dot(vector_subtract(mesh.points[i], mesh.points[j]),
                        vector_subtract(mesh.points[i], mesh.points[j])) < 1e-4)
                {
                    canonical[i] = canonical[j];
                    break;
                }
        }
        struct EdgeUse
        {
            unsigned face;
            int direction;
        };
        std::map<std::pair<unsigned, unsigned>, std::vector<EdgeUse>> edges;
        std::size_t work = 0;
        convex = true;
        for (unsigned face = 0; face < mesh.polygons.size(); ++face)
        {
            auto& polygon = mesh.polygons[face];
            // Keep both serialized arrays in World; use the base ring for volume topology.
            // In retail v56, extra vertices repeat the ring with possible edge subdivisions.
            if (polygon.base_points != UINT16_MAX)
            {
                if (polygon.base_points < 3 || polygon.base_points > polygon.points.size())
                    return fail("Invalid container base ring");
                polygon.points.resize(polygon.base_points);
            }
            if (polygon.surface >= mesh.surfaces.size() || polygon.points.size() < 3 || polygon.points.size() > 512)
                return fail("Invalid container polygon");
            const auto plane_index = mesh.surfaces[polygon.surface].plane;
            if (plane_index >= source.planes.size())
                return fail("Invalid container plane");
            const auto& src = source.planes[plane_index];
            const auto length = std::sqrt(dot(src.normal, src.normal));
            if (!std::isfinite(length) || length < 0.001 || !std::isfinite(src.distance))
                return fail("Invalid container plane normal");
            const auto n = rotate(vector_scale(src.normal, float(1 / length)), rotation);
            const float d = float(src.distance / length) -
                            vector_dot(vector_scale(src.normal, float(1 / length)), origin) + vector_dot(n, origin);
            if (std::none_of(
                    planes_.begin(), planes_.end(), [&](const auto& p)
                    { return p.normal.x == n.x && p.normal.y == n.y && p.normal.z == n.z && p.distance == d; }))
                planes_.push_back({n, d});
            for (auto p : mesh.points)
                if (vector_dot(n, p) - d > 0.05f)
                    convex = false;
            for (auto index : polygon.points)
                if (index >= mesh.points.size() || std::abs(vector_dot(n, mesh.points[index]) - d) > 0.1f)
                    return fail("Container face is not on its plane");
            const auto a = mesh.points[polygon.points[0]];
            DVector face_normal{};
            for (std::size_t j = 1; j + 1 < polygon.points.size(); ++j)
                face_normal = vector_add(face_normal, cross(vector_subtract(mesh.points[polygon.points[j]], a),
                                                            vector_subtract(mesh.points[polygon.points[j + 1]], a)));
            if (dot(face_normal, face_normal) < 1e-10)
                return fail("Degenerate container face");
            if (vector_dot(face_normal, n) < 0)
                std::reverse(polygon.points.begin(), polygon.points.end());
            for (std::size_t j = 1; j + 1 < polygon.points.size(); ++j)
            {
                const std::array<DVector, 3> tri{mesh.points[polygon.points[0]], mesh.points[polygon.points[j]],
                                                 mesh.points[polygon.points[j + 1]]};
                if (dot(cross(vector_subtract(tri[1], tri[0]), vector_subtract(tri[2], tri[0])), n) < -0.01)
                    return fail("Non-convex individual container face");
                triangles.push_back(tri);
            }
            for (std::size_t j = 0; j < polygon.points.size(); ++j)
            {
                const auto first = canonical[polygon.points[j]],
                           last = canonical[polygon.points[(j + 1) % polygon.points.size()]];
                if (first == last)
                    continue;
                const auto start = mesh.points[first], delta = vector_subtract(mesh.points[last], start);
                const auto len2 = dot(delta, delta);
                std::vector<std::pair<double, unsigned>> split{{0, first}, {1, last}};
                if ((work += mesh.points.size()) > 16000000)
                    return fail("Container closure validation exceeds work limit");
                // Split T junctions before checking closed edges. Retail BSP faces are subdivided.
                for (unsigned k = 0; k < mesh.points.size(); ++k)
                {
                    if (canonical[k] != k || k == first || k == last)
                        continue;
                    const auto rel = vector_subtract(mesh.points[k], start);
                    const auto t = dot(rel, delta) / len2;
                    if (t <= 1e-7 || t >= 1 - 1e-7)
                        continue;
                    const auto distance = vector_subtract(rel, vector_scale(delta, float(t)));
                    if (dot(distance, distance) < 1e-4)
                        split.emplace_back(t, k);
                }
                std::sort(split.begin(), split.end());
                for (std::size_t k = 1; k < split.size(); ++k)
                {
                    const auto u = split[k - 1].second, v = split[k].second;
                    auto& edge = edges[{std::min(u, v), std::max(u, v)}];
                    edge.push_back({face, u < v ? 1 : -1});
                }
            }
        }
        // Multiple closed brushes can touch along the same DAT edge. First join
        // faces only through unambiguous two-face edges, then require each resulting
        // component to close independently. Balance alone would accept ambiguous
        // non-manifold sheets or duplicated geometry.
        std::vector<unsigned> parent(mesh.polygons.size()), size(mesh.polygons.size(), 1);
        for (unsigned i = 0; i < parent.size(); ++i)
            parent[i] = i;
        auto root = [&](unsigned face)
        {
            while (parent[face] != face)
            {
                parent[face] = parent[parent[face]];
                face = parent[face];
            }
            return face;
        };
        for (const auto& [key, uses] : edges)
            if (uses.size() == 2 && uses[0].direction + uses[1].direction == 0)
            {
                auto a = root(uses[0].face), b = root(uses[1].face);
                if (a == b)
                    continue;
                if (size[a] < size[b])
                    std::swap(a, b);
                parent[b] = a;
                size[a] += size[b];
            }
        for (auto& [key, uses] : edges)
        {
            std::sort(uses.begin(), uses.end(),
                      [&](const auto& a, const auto& b) { return root(a.face) < root(b.face); });
            for (std::size_t first = 0; first < uses.size();)
            {
                auto end = first;
                int balance = 0;
                while (end < uses.size() && root(uses[end].face) == root(uses[first].face))
                    balance += uses[end++].direction;
                if (end - first != 2 || balance != 0 || uses[first].face == uses[first + 1].face)
                {
                    const auto a = mesh.points[key.first], b = mesh.points[key.second];
                    const auto detail = std::string("Open or non-manifold container shell: ") +
                                        std::to_string(end - first) + " edges / balance " + std::to_string(balance) +
                                        " in component " + std::to_string(root(uses[first].face)) + " at " +
                                        property_text(a) + " -> " + property_text(b);
                    return fail(detail.c_str());
                }
                first = end;
            }
            shared_edges += uses.size() > 2;
        }
        for (unsigned i = 0; i < parent.size(); ++i)
            closed_components += root(i) == i;
        for (auto& surface : mesh.surfaces)
            surface.flags |= 1;
        // Faces were rewound to the rotated plane normals above; keep planes consistent.
        for (auto& plane : mesh.planes)
            plane.normal = rotate(plane.normal, rotation);
        if (!boundary_.build(mesh, error))
        {
            *this = {};
            return false;
        }
        return true;
    }
    bool ContainerShape::contains(DVector point) const
    {
        if (triangles.empty() || !physics_vector(point) || point.x < min.x - .001f || point.x > max.x + .001f ||
            point.y < min.y - .001f || point.y > max.y + .001f || point.z < min.z - .001f || point.z > max.z + .001f)
            return false;
        if (convex)
        {
            for (auto plane : planes_)
                if (vector_dot(plane.normal, point) - plane.distance > .001f)
                    return false;
            return true;
        }
        CollisionHit hit;
        if (boundary_.sweep(point, point, {.001f, .001f, .001f}, hit) == CollisionQuery::Hit)
            return true;
        double angle = 0;
        for (const auto& triangle : triangles)
        {
            const auto a = vector_subtract(triangle[0], point), b = vector_subtract(triangle[1], point),
                       c = vector_subtract(triangle[2], point);
            const auto la = std::sqrt(dot(a, a)), lb = std::sqrt(dot(b, b)), lc = std::sqrt(dot(c, c));
            angle +=
                2 * std::atan2(dot(a, cross(b, c)), la * lb * lc + dot(a, b) * lc + dot(b, c) * la + dot(c, a) * lb);
        }
        return std::abs(angle) > 2 * std::numbers::pi;
    }
    CollisionQuery ContainerShape::sweep(DVector from, DVector to, DVector half, CollisionHit& hit,
                                         double allowed_penetration, StartOverlap start) const
    {
        // A center inside a closed component remains start-solid under every policy.
        auto result = boundary_.sweep(from, to, half, hit, allowed_penetration, start);
        if (result != CollisionQuery::Invalid && contains(from))
        {
            hit = {};
            hit.fraction = 0;
            hit.point = from;
            hit.start_solid = true;
            return CollisionQuery::Hit;
        }
        return result;
    }
    bool ContainerShape::overlaps(DVector center, DVector half) const
    {
        if (!physics_vector(center) || !physics_vector(half) || half.x < 0 || half.y < 0 || half.z < 0 ||
            center.x + half.x < min.x || center.x - half.x > max.x || center.y + half.y < min.y ||
            center.y - half.y > max.y || center.z + half.z < min.z || center.z - half.z > max.z)
            return false;
        CollisionHit hit;
        return contains(center) || boundary_.sweep(center, center, half, hit) == CollisionQuery::Hit;
    }
} // namespace OpenBlood2
