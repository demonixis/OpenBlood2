#include "engine/world_shade.h"
#include "engine/collision.h"
#include <algorithm>

namespace OpenBlood2
{
    namespace
    {
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
        float axis(DVector v, int i)
        {
            return i == 0 ? v.x : i == 1 ? v.y : v.z;
        }
        // Slab test of the segment from + t * delta, t in [0, limit], against a box.
        bool segment_box(DVector from, DVector delta, float limit, DVector min, DVector max)
        {
            float low = 0, high = limit;
            for (int i = 0; i < 3; ++i)
            {
                const float o = axis(from, i), d = axis(delta, i), a = axis(min, i), b = axis(max, i);
                if (std::abs(d) < 1e-12f)
                {
                    if (o < a || o > b)
                        return false;
                    continue;
                }
                float t0 = (a - o) / d, t1 = (b - o) / d;
                if (t0 > t1)
                    std::swap(t0, t1);
                low = std::max(low, t0);
                high = std::min(high, t1);
                if (low > high)
                    return false;
            }
            return true;
        }
    } // namespace

    void WorldShade::clear()
    {
        ranges_.clear();
        stack_.clear();
        stats = {};
    }
    std::uint32_t WorldShade::partition(Tree& tree, std::uint32_t begin, std::uint32_t end)
    {
        Node node;
        node.begin = begin;
        node.end = end;
        node.min = tree.triangles[begin].min;
        node.max = tree.triangles[begin].max;
        for (auto i = begin + 1; i < end; ++i)
        {
            node.min = minv(node.min, tree.triangles[i].min);
            node.max = maxv(node.max, tree.triangles[i].max);
        }
        const auto index = static_cast<std::uint32_t>(tree.nodes.size());
        tree.nodes.push_back(node);
        if (end - begin > 4)
        {
            const DVector size = vector_subtract(node.max, node.min);
            const int split = size.x >= size.y && size.x >= size.z ? 0 : size.y >= size.z ? 1 : 2;
            const auto middle = begin + (end - begin) / 2;
            std::nth_element(
                tree.triangles.begin() + begin, tree.triangles.begin() + middle, tree.triangles.begin() + end,
                [&](const Triangle& a, const Triangle& b)
                { return axis(a.min, split) + axis(a.max, split) < axis(b.min, split) + axis(b.max, split); });
            const auto left = partition(tree, begin, middle);
            const auto right = partition(tree, middle, end);
            tree.nodes[index].left = left;
            tree.nodes[index].right = right;
        }
        return index;
    }
    bool WorldShade::build(const WorldMesh& mesh, std::string& error)
    {
        clear();
        error.clear();
        ranges_.resize(mesh.ranges.size());
        std::size_t total = 0;
        for (std::size_t r = 0; r < mesh.ranges.size(); ++r)
        {
            auto& tree = ranges_[r];
            for (const auto& batch : mesh.ranges[r].batches)
            {
                if (!batch.material || batch.translucent || batch.sky_portal)
                    continue;
                if (batch.count % 3 || batch.first > mesh.vertices.size() ||
                    batch.count > mesh.vertices.size() - batch.first || batch.count / 3 > triangle_limit - total)
                {
                    clear();
                    error = "World shade geometry exceeds limits";
                    return false;
                }
                total += batch.count / 3;
                for (std::uint32_t i = batch.first; i < batch.first + batch.count; i += 3)
                {
                    Triangle t;
                    for (int k = 0; k < 3; ++k)
                    {
                        t.points[k] = mesh.vertices[i + k].position;
                        t.colors[k] = mesh.vertices[i + k].color;
                    }
                    t.min = minv(t.points[0], minv(t.points[1], t.points[2]));
                    t.max = maxv(t.points[0], maxv(t.points[1], t.points[2]));
                    tree.triangles.push_back(t);
                }
            }
            if (!tree.triangles.empty())
                partition(tree, 0, static_cast<std::uint32_t>(tree.triangles.size()));
            stats.nodes += tree.nodes.size();
        }
        stats.triangles = total;
        return true;
    }
    bool WorldShade::trace(DVector from, DVector to, std::span<const WorldDraw> draws, DVector& light, float& fraction)
    {
        ++stats.traces;
        bool found = false;
        float best = 1;
        for (const auto& draw : draws)
        {
            if (draw.range >= ranges_.size() || ranges_[draw.range].nodes.empty())
                continue;
            const auto& tree = ranges_[draw.range];
            // Rigid transform: the parametric fraction is identical in local space.
            const auto inverse = inverse_rotation(draw.rotation);
            const auto origin = rotate_vector(vector_subtract(from, draw.position), inverse);
            const auto delta = vector_subtract(rotate_vector(vector_subtract(to, draw.position), inverse), origin);
            stack_.assign(1, 0);
            while (!stack_.empty())
            {
                const auto& node = tree.nodes[stack_.back()];
                stack_.pop_back();
                ++stats.node_tests;
                if (!segment_box(origin, delta, best, node.min, node.max))
                    continue;
                if (node.left || node.right)
                {
                    stack_.push_back(node.left);
                    stack_.push_back(node.right);
                    continue;
                }
                for (auto i = node.begin; i < node.end; ++i)
                {
                    const auto& t = tree.triangles[i];
                    ++stats.triangle_tests;
                    // Two-sided Moller-Trumbore with barycentric weights for the face light.
                    const auto e1 = vector_subtract(t.points[1], t.points[0]);
                    const auto e2 = vector_subtract(t.points[2], t.points[0]);
                    const auto p = cross(delta, e2);
                    const float det = vector_dot(e1, p);
                    if (std::abs(det) < 1e-12f)
                        continue;
                    const auto s = vector_subtract(origin, t.points[0]);
                    const float u = vector_dot(s, p) / det;
                    if (u < 0 || u > 1)
                        continue;
                    const auto q = cross(s, e1);
                    const float v = vector_dot(delta, q) / det;
                    if (v < 0 || u + v > 1)
                        continue;
                    const float f = vector_dot(e2, q) / det;
                    if (f < 0 || f > best)
                        continue;
                    best = f;
                    found = true;
                    const float w = 1 - u - v;
                    light = {w * t.colors[0].x + u * t.colors[1].x + v * t.colors[2].x,
                             w * t.colors[0].y + u * t.colors[1].y + v * t.colors[2].y,
                             w * t.colors[0].z + u * t.colors[1].z + v * t.colors[2].z};
                }
            }
        }
        if (found)
        {
            ++stats.hits;
            fraction = best;
        }
        return found;
    }
} // namespace OpenBlood2
