#pragma once

#include "app/xr_input.h"
#include "engine/xr/xr_runtime.h"
#include "game/server_adapter.h"
#include <array>
#include <cstdint>
#include <optional>

// The XR player: tracked head and controllers mapped onto the original client inputs.
namespace OpenBlood2
{
    inline constexpr float xr_max_lean = 0.5f;                // Metres of horizontal head offset.
    inline constexpr float xr_snap_angle = 0.78539816f;       // Default snap turn: 45 degrees.
    inline constexpr float xr_smooth_turn_speed = 2.0943951f; // Smooth turning: 120 degrees/s at full deflection.
    inline constexpr float xr_aim_range = 8192;               // World units of the convergence ray.
    inline constexpr float xr_weapon_deflection = 0.85f;      // Right stick up/down that changes weapon.
    inline constexpr float xr_grip_press = 0.85f, xr_grip_release = 0.6f; // Right grip: use item.

    // App-side state: body yaw (turns), control edges and the last aim written.
    struct XrPlayer
    {
        bool initialized = false;
        float body_yaw = 0, written_pitch = 0, written_yaw = 0;
        // Options > VR: the snap-turn step in radians (zero turns smoothly), and whether the left
        // stick moves relative to the right hand instead of the head.
        float turn_step = xr_snap_angle;
        bool hand_direction = false;
        xr::PressEdge prev_item, next_item, use_item;
        xr::StickEdge turn, weapon;
        xr::TriggerButton fire, alt_fire, grip;
        std::array<std::uint64_t, 2> shots{};
    };

    // One frame's tracking frame: the game camera's position, and the body yaw while the view is
    // the player's eye (snap turns) or the camera's own yaw otherwise.
    struct XrTracking
    {
        xr::Vec3 base{};
        float yaw = 0;
        xr::StereoCamera stereo;
    };
    XrTracking xr_tracking(const GameSession&, const XrPlayer&, const XrFrameInput&, const xr::Vec3& reference,
                           float units_per_meter);
    xr::Pose xr_world_pose(const XrTracking&, const xr::Vec3& reference, float units_per_meter,
                           const xr::Pose& tracked);
    // Where an aim pose points: the first solid hit along it (the player excluded), or the range end.
    xr::Vec3 xr_aim_target(GameSession&, const xr::Pose& aim);
    // Virtual shoulders for the arm IK: a neck pivot below and behind the tracked head and
    // shoulders across a body yaw that follows the head beyond a 45-degree dead zone. The body yaw
    // is kept relative to the tracking yaw, so snap turns carry it. Elbows point down, out and back.
    inline constexpr float xr_body_dead_zone = 0.78539816f;
    struct XrShoulders
    {
        std::array<xr::Vec3, 2> shoulders{}, poles{}; // {left, right}, world.
    };
    XrShoulders xr_shoulders(const XrTracking&, float units_per_meter, std::optional<float>& body_yaw);
    // Controller state into the original client inputs (right-handed layout): head- or hand-relative
    // locomotion through CBloodClientShell::locomotion_yaw, snap or smooth turns over `seconds`,
    // weapon cycling, items, and converged aim (shots leave the eye toward the point the right
    // controller designates). A scripted camera (CBloodClientShell::view.object) keeps the player's
    // rotation: no turning, weapon cycling or aim.
    void xr_apply_controls(GameSession&, XrPlayer&, const XrFrameInput&, const XrTracking&, const xr::Vec3& reference,
                           float units_per_meter, double seconds);
} // namespace OpenBlood2
