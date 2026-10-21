#include "engine/arm_ik.h"
#include <algorithm>
#include <cmath>
#include <vector>
namespace OpenBlood2
{
    namespace
    {
        DVector add(DVector a, DVector b)
        {
            return {a.x + b.x, a.y + b.y, a.z + b.z};
        }
        DVector sub(DVector a, DVector b)
        {
            return {a.x - b.x, a.y - b.y, a.z - b.z};
        }
        DVector mul(DVector a, float s)
        {
            return {a.x * s, a.y * s, a.z * s};
        }
        float dot(DVector a, DVector b)
        {
            return a.x * b.x + a.y * b.y + a.z * b.z;
        }
        DVector cross(DVector a, DVector b)
        {
            return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
        }
        float length(DVector a)
        {
            return std::sqrt(dot(a, a));
        }
        bool finite(DVector a)
        {
            return std::isfinite(a.x) && std::isfinite(a.y) && std::isfinite(a.z);
        }
        DVector perpendicular(DVector unit)
        {
            const DVector axis = std::abs(unit.x) < .9f ? DVector{1, 0, 0} : DVector{0, 1, 0};
            const auto p = cross(unit, axis);
            return mul(p, 1 / length(p));
        }
        // `v` without its component along `unit`, normalized; `fallback` when that vanishes.
        DVector orthonormal(DVector v, DVector unit, DVector fallback)
        {
            const auto p = sub(v, mul(unit, dot(v, unit)));
            const float n = length(p);
            return n > 1e-6f ? mul(p, 1 / n) : fallback;
        }
        DRotation axis_angle(DVector unit, float angle)
        {
            const float s = std::sin(angle / 2);
            return {mul(unit, s), std::cos(angle / 2)};
        }
        // Shortest rotation taking unit `from` to unit `to`.
        DRotation arc(DVector from, DVector to)
        {
            const float d = dot(from, to);
            if (d < -.999999f)
                return axis_angle(perpendicular(from), 3.14159265f);
            const auto c = cross(from, to);
            DRotation q{c, 1 + d};
            const float n = std::sqrt(dot(q.m_Vec, q.m_Vec) + q.m_Spin * q.m_Spin);
            return {mul(q.m_Vec, 1 / n), q.m_Spin / n};
        }
        // Rotation taking the frame (d0, n0) to (d1, n1): the direction exactly, then the hinge
        // normal by a twist about the new direction.
        DRotation align(DVector d0, DVector n0, DVector d1, DVector n1)
        {
            const auto q1 = arc(d0, d1);
            const auto n = orthonormal(rotate_vector(q1, n0), d1, perpendicular(d1));
            const float twist = std::atan2(dot(cross(n, n1), d1), dot(n, n1));
            return compose_rotation(axis_angle(d1, twist), q1);
        }
        bool descends(const Model& model, std::size_t node, std::size_t ancestor)
        {
            for (int p = model.nodes[node].parent; p >= 0; p = model.nodes[p].parent)
                if (static_cast<std::size_t>(p) == ancestor)
                    return true;
            return false;
        }
        bool solve(const Model& model, const ArmTarget& target, std::span<NodePose> pose, ArmSolve& result)
        {
            const std::size_t u = target.upper, l = target.lower, w = target.wrist, count = model.nodes.size();
            if (u >= count || l >= count || w >= count || pose.size() != count || !descends(model, l, u) ||
                !descends(model, w, l) || !finite(target.shoulder) || !finite(target.pole))
                return false;
            const auto pu = pose[u].position, pl = pose[l].position, pw = pose[w].position;
            const auto d1 = sub(pl, pu), d2 = sub(pw, pl);
            const float l1 = length(d1), l2 = length(d2);
            if (!(l1 > 1e-4f) || !(l2 > 1e-4f) || !std::isfinite(l1 + l2))
                return false;
            const DVector u1 = mul(d1, 1 / l1), u2 = mul(d2, 1 / l2);
            // Authored hinge: the bend plane normal, or the lower arm's own x axis when straight.
            const auto bend = cross(u1, u2);
            const auto hinge = length(bend) > 1e-3f
                                   ? mul(bend, 1 / length(bend))
                                   : orthonormal(rotate_vector(pose[l].rotation, {1, 0, 0}), u1, perpendicular(u1));
            auto shoulder = target.shoulder;
            auto reach = sub(pw, shoulder);
            float distance = length(reach);
            const DVector dir = distance > 1e-6f ? mul(reach, 1 / distance) : mul(add(u1, u2), 1 / length(add(u1, u2)));
            const float longest = (l1 + l2) * .9999f, shortest = std::abs(l1 - l2) * 1.0001f + (l1 + l2) * 1e-4f;
            result.slid = distance > longest || distance < shortest;
            distance = std::clamp(distance, shortest, longest);
            shoulder = sub(pw, mul(dir, distance));
            const float along = (l1 * l1 - l2 * l2 + distance * distance) / (2 * distance);
            const float out = std::sqrt(std::max(0.f, l1 * l1 - along * along));
            const auto side = orthonormal(target.pole, dir, orthonormal(cross(hinge, dir), dir, perpendicular(dir)));
            const auto elbow = add(shoulder, add(mul(dir, along), mul(side, out)));
            const auto normal = cross(side, dir);
            const auto r1 = align(u1, hinge, mul(sub(elbow, shoulder), 1 / l1), normal);
            const auto r2 = align(u2, hinge, mul(sub(pw, elbow), 1 / l2), normal);
            std::vector<NodePose> next(pose.begin(), pose.end());
            for (std::size_t i = 0; i < count; ++i)
            {
                if (i == w || descends(model, i, w))
                    continue;
                if (i == l || descends(model, i, l))
                    next[i] = {add(elbow, rotate_vector(r2, sub(pose[i].position, pl))),
                               compose_rotation(r2, pose[i].rotation)};
                else if (i == u || descends(model, i, u))
                    next[i] = {add(shoulder, rotate_vector(r1, sub(pose[i].position, pu))),
                               compose_rotation(r1, pose[i].rotation)};
                else
                    continue;
                if (!finite(next[i].position) || !finite(next[i].rotation.m_Vec) ||
                    !std::isfinite(next[i].rotation.m_Spin))
                    return false;
            }
            std::copy(next.begin(), next.end(), pose.begin());
            result.shoulder = shoulder;
            result.elbow = elbow;
            return true;
        }
    } // namespace
    bool solve_arms(const Model& model, std::span<const ArmTarget> targets, std::span<NodePose> pose,
                    std::span<ArmSolve> results)
    {
        if (!results.empty() && results.size() != targets.size())
            return false;
        std::vector<NodePose> work(pose.begin(), pose.end());
        for (std::size_t i = 0; i < targets.size(); ++i)
        {
            ArmSolve result;
            if (!solve(model, targets[i], work, result))
                return false;
            if (!results.empty())
                results[i] = result;
        }
        std::copy(work.begin(), work.end(), pose.begin());
        return true;
    }
} // namespace OpenBlood2
