#pragma once

#include "engine/xr/xr_math.h"
#include "game/player_messages.h"
#include <algorithm>
#include <array>
#include <cmath>

// Controller input mapped onto the original client inputs. Pure values, testable headless.
namespace OpenBlood2::xr
{
    // An analog stick (x right, y forward) as the original eight movement flags: the nearest
    // 45-degree sector keeps the original per-direction speeds and animations, and the residual
    // angle turns the movement yaw so the direction is exact. Full deflection runs.
    struct Locomotion
    {
        MovementInput input;
        float yaw = 0;
        bool moving = false;
    };
    inline Locomotion locomotion(std::array<float, 2> stick, float reference_yaw, float deadzone = .2f,
                                 float run_threshold = .85f)
    {
        Locomotion out;
        out.yaw = reference_yaw;
        const float x = stick[0], y = stick[1];
        const float magnitude = std::sqrt(x * x + y * y);
        if (!std::isfinite(magnitude) || magnitude < deadzone)
            return out;
        constexpr float sector_angle = 0.78539816f;
        const float angle = std::atan2(x, y); // 0 forward, positive to the right
        const int sector = static_cast<int>(std::lround(angle / sector_angle));
        out.yaw = reference_yaw + (angle - static_cast<float>(sector) * sector_angle);
        const int k = (sector + 8) % 8; // 0 forward, 2 right, 4 backward, 6 left
        out.input.forward = k == 0 || k == 1 || k == 7;
        out.input.backward = k == 3 || k == 4 || k == 5;
        out.input.right = k == 1 || k == 2 || k == 3;
        out.input.left = k == 5 || k == 6 || k == 7;
        out.input.run = magnitude >= run_threshold;
        out.moving = true;
        return out;
    }

    // One command per deflection: fires past `press`, re-arms below `release`.
    struct StickEdge
    {
        bool armed = true;
        int update(float value, float press = .75f, float release = .3f)
        {
            if (!std::isfinite(value))
                value = 0;
            if (armed && std::fabs(value) >= press)
            {
                armed = false;
                return value > 0 ? 1 : -1;
            }
            if (std::fabs(value) < release)
                armed = true;
            return 0;
        }
    };

    // Analog trigger as a held button with hysteresis.
    struct TriggerButton
    {
        bool down = false;
        bool update(float value)
        {
            down = std::isfinite(value) && (down ? value > .35f : value > .55f);
            return down;
        }
    };

    // Press edge of a boolean control.
    struct PressEdge
    {
        bool was_down = false;
        bool update(bool down)
        {
            const bool pressed = down && !was_down;
            was_down = down;
            return pressed;
        }
    };

    // The original pitch/yaw (forward = (sin yaw cos pitch, -sin pitch, cos yaw cos pitch)) from
    // `from` toward `to`, pitch clamped like the desktop look. False when the points coincide.
    inline bool aim_angles(const Vec3& from, const Vec3& to, float& pitch, float& yaw)
    {
        const Vec3 d{to[0] - from[0], to[1] - from[1], to[2] - from[2]};
        const float length = std::sqrt(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);
        if (!std::isfinite(length) || length < 1e-3f)
            return false;
        pitch = std::clamp(-std::asin(std::clamp(d[1] / length, -1.f, 1.f)), -1.48f, 1.48f);
        yaw = std::atan2(d[0], d[2]);
        return true;
    }

    // A ray (OpenXR frame, unit direction) against a quad facing +Z at `panel`, `width` x `height`
    // metres: true when it reaches the front within the bounds, with the canvas UV (v downward).
    inline bool panel_hit(const Pose& panel, float width, float height, const Vec3& origin, const Vec3& direction,
                          std::array<float, 2>& uv, float& distance)
    {
        const auto dot = [](const Vec3& a, const Vec3& b) { return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]; };
        const Vec3 normal = rotate(panel.orientation, {0, 0, 1});
        const float facing = dot(direction, normal);
        if (!std::isfinite(facing) || facing > -1e-4f)
            return false; // Parallel, or reaching the back.
        const Vec3 to{panel.position[0] - origin[0], panel.position[1] - origin[1], panel.position[2] - origin[2]};
        const float t = dot(to, normal) / facing;
        if (!std::isfinite(t) || t <= 0)
            return false;
        const Vec3 hit{origin[0] + direction[0] * t - panel.position[0],
                       origin[1] + direction[1] * t - panel.position[1],
                       origin[2] + direction[2] * t - panel.position[2]};
        const Quat inverse{-panel.orientation[0], -panel.orientation[1], -panel.orientation[2], panel.orientation[3]};
        const Vec3 local = rotate(inverse, hit);
        uv = {local[0] / width + .5f, .5f - local[1] / height};
        distance = t;
        return uv[0] >= 0 && uv[0] <= 1 && uv[1] >= 0 && uv[1] <= 1;
    }

    // Yaw of an engine orientation's forward axis, in the original convention.
    inline float forward_yaw(const Quat& q)
    {
        const Vec3 f = rotate(q, {0, 0, 1});
        return std::atan2(f[0], f[2]);
    }
} // namespace OpenBlood2::xr
