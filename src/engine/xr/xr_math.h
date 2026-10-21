#pragma once

#include <array>
#include <cmath>

// OpenXR tracking values converted to the engine's frame. OpenXR is right-handed with
// -Z forward, in metres; the engine is left-handed with +Z forward, in world units.
// The conversion is the reflection z -> -z plus a uniform scale, so a pose keeps its
// meaning (forward stays forward, right stays right). Kept free of the OpenXR headers
// so the arithmetic is testable on every platform.
namespace OpenBlood2::xr
{
    using Vec3 = std::array<float, 3>;
    using Quat = std::array<float, 4>; // x, y, z, w

    struct Pose
    {
        Vec3 position{};
        Quat orientation{0, 0, 0, 1};
    };

    // Tangent-space half angles in radians, as XrFovf: left and down are negative.
    struct Fov
    {
        float left = 0, right = 0, up = 0, down = 0;
    };

    inline Vec3 engine_position(const Vec3& xr, float units_per_meter)
    {
        return {xr[0] * units_per_meter, xr[1] * units_per_meter, -xr[2] * units_per_meter};
    }

    // M R M for M = diag(1, 1, -1): the axis is reflected and the angle reversed.
    inline Quat engine_orientation(const Quat& xr)
    {
        return {-xr[0], -xr[1], xr[2], xr[3]};
    }

    inline Pose engine_pose(const Pose& xr, float units_per_meter)
    {
        return {engine_position(xr.position, units_per_meter), engine_orientation(xr.orientation)};
    }

    inline Vec3 cross(const Vec3& a, const Vec3& b)
    {
        return {a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]};
    }

    // Same expression as the vertex shaders: v + 2 q x (q x v + w v).
    inline Vec3 rotate(const Quat& q, const Vec3& v)
    {
        const Vec3 axis{q[0], q[1], q[2]};
        const Vec3 inner = cross(axis, v);
        const Vec3 t{inner[0] + q[3] * v[0], inner[1] + q[3] * v[1], inner[2] + q[3] * v[2]};
        const Vec3 outer = cross(axis, t);
        return {v[0] + 2 * outer[0], v[1] + 2 * outer[1], v[2] + 2 * outer[2]};
    }

    struct Basis
    {
        Vec3 right{1, 0, 0}, up{0, 1, 0}, forward{0, 0, 1};
    };

    inline Basis basis(const Quat& engine)
    {
        return {rotate(engine, {1, 0, 0}), rotate(engine, {0, 1, 0}), rotate(engine, {0, 0, 1})};
    }

    // Hamilton product: rotate by b, then by a (same convention as the engine's multiply_rotation).
    inline Quat mul(const Quat& a, const Quat& b)
    {
        return {a[3] * b[0] + b[3] * a[0] + a[1] * b[2] - a[2] * b[1],
                a[3] * b[1] + b[3] * a[1] + a[2] * b[0] - a[0] * b[2],
                a[3] * b[2] + b[3] * a[2] + a[0] * b[1] - a[1] * b[0],
                a[3] * b[3] - a[0] * b[0] - a[1] * b[1] - a[2] * b[2]};
    }

    // Engine yaw rotation: forward (0, 0, 1) turns to (sin yaw, 0, cos yaw), as the original
    // player pitch/yaw forward.
    inline Quat yaw_rotation(float yaw)
    {
        return {0, std::sin(yaw / 2), 0, std::cos(yaw / 2)};
    }

    // The tracked eyes placed around a game camera, in engine world units. The game camera
    // contributes its position and yaw only (no pitch or roll); the head offset is measured from
    // the calibrated reference (OpenXR metres), its horizontal part clamped to max_lean metres.
    struct StereoCamera
    {
        std::array<Pose, 2> eyes{};
        Pose head{}; // Midpoint of the eyes, with the first eye's orientation.
    };
    inline StereoCamera stereo_camera(const Vec3& base, float base_yaw, const std::array<Pose, 2>& xr_eyes,
                                      const Vec3& reference, float units_per_meter, float max_lean)
    {
        const Vec3 centre{(xr_eyes[0].position[0] + xr_eyes[1].position[0]) / 2,
                          (xr_eyes[0].position[1] + xr_eyes[1].position[1]) / 2,
                          (xr_eyes[0].position[2] + xr_eyes[1].position[2]) / 2};
        Vec3 offset{centre[0] - reference[0], centre[1] - reference[1], centre[2] - reference[2]};
        const float lean = std::sqrt(offset[0] * offset[0] + offset[2] * offset[2]);
        if (lean > max_lean && lean > 0)
        {
            offset[0] *= max_lean / lean;
            offset[2] *= max_lean / lean;
        }
        const Quat yaw = yaw_rotation(base_yaw);
        StereoCamera out;
        for (std::size_t i = 0; i < 2; ++i)
        {
            const Vec3 relative{offset[0] + xr_eyes[i].position[0] - centre[0],
                                offset[1] + xr_eyes[i].position[1] - centre[1],
                                offset[2] + xr_eyes[i].position[2] - centre[2]};
            const Vec3 local = rotate(yaw, engine_position(relative, units_per_meter));
            out.eyes[i].position = {base[0] + local[0], base[1] + local[1], base[2] + local[2]};
            out.eyes[i].orientation = mul(yaw, engine_orientation(xr_eyes[i].orientation));
        }
        out.head.position = {(out.eyes[0].position[0] + out.eyes[1].position[0]) / 2,
                             (out.eyes[0].position[1] + out.eyes[1].position[1]) / 2,
                             (out.eyes[0].position[2] + out.eyes[1].position[2]) / 2};
        out.head.orientation = out.eyes[0].orientation;
        return out;
    }

    // Clip x = view_x * scale_x + view_z * offset_x (likewise y), so NDC spans -1..1 from
    // the left/down tangent to the right/up tangent. Symmetric 90 degrees gives scale 1,
    // offset 0: the desktop projection.
    struct Projection
    {
        float scale_x = 1, scale_y = 1, offset_x = 0, offset_y = 0;
    };

    inline bool valid_fov(const Fov& fov)
    {
        constexpr float limit = 1.5533430f; // 89 degrees
        return std::isfinite(fov.left) && std::isfinite(fov.right) && std::isfinite(fov.up) &&
               std::isfinite(fov.down) && fov.left > -limit && fov.right < limit && fov.down > -limit &&
               fov.up < limit && fov.left < fov.right && fov.down < fov.up;
    }

    inline Projection projection(const Fov& fov)
    {
        const float l = std::tan(fov.left), r = std::tan(fov.right);
        const float d = std::tan(fov.down), u = std::tan(fov.up);
        return {2 / (r - l), 2 / (u - d), -(r + l) / (r - l), -(u + d) / (u - d)};
    }

    // Matches cbuffer View in line.vert.hlsl; 144 bytes, pushed per draw.
    struct LineUniform
    {
        std::array<float, 4> eye{}, right{}, up{}, forward{}, projection{}, projection_offset{}, translation{},
            quaternion{0, 0, 0, 1}, scale{1, 1, 1, 0};
    };
    static_assert(sizeof(LineUniform) == 144);

    inline LineUniform line_uniform(const Pose& eye, const Fov& fov, float near_plane, float far_plane)
    {
        LineUniform out;
        const Basis b = basis(eye.orientation);
        const Projection p = projection(fov);
        out.eye = {eye.position[0], eye.position[1], eye.position[2], 0};
        out.right = {b.right[0], b.right[1], b.right[2], 0};
        out.up = {b.up[0], b.up[1], b.up[2], 0};
        out.forward = {b.forward[0], b.forward[1], b.forward[2], 0};
        out.projection = {p.scale_x, p.scale_y, far_plane / (far_plane - near_plane),
                          -near_plane * far_plane / (far_plane - near_plane)};
        out.projection_offset = {p.offset_x, p.offset_y, 0, 0};
        return out;
    }

    // Clip-space position the shader produces for a world point (tests and culling).
    inline std::array<float, 4> clip(const LineUniform& u, const Vec3& world)
    {
        const Vec3 rel{world[0] - u.eye[0], world[1] - u.eye[1], world[2] - u.eye[2]};
        const auto dot = [&rel](const std::array<float, 4>& a)
        { return rel[0] * a[0] + rel[1] * a[1] + rel[2] * a[2]; };
        const float z = dot(u.forward);
        return {dot(u.right) * u.projection[0] + z * u.projection_offset[0],
                dot(u.up) * u.projection[1] + z * u.projection_offset[1], z * u.projection[2] + u.projection[3], z};
    }
} // namespace OpenBlood2::xr
