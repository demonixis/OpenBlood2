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
    inline constexpr float xr_max_lean = 0.5f;          // Metres of horizontal head offset.
    inline constexpr float xr_snap_angle = 0.78539816f; // 45 degrees per snap turn.
    inline constexpr float xr_aim_range = 8192;         // World units of the convergence ray.

    // App-side state: body yaw (snap turns), control edges and the last aim written.
    struct XrPlayer
    {
        bool initialized = false;
        float body_yaw = 0, written_pitch = 0, written_yaw = 0;
        bool run_latch = false;
        xr::PressEdge run, next_item, use_item;
        xr::StickEdge turn, weapon;
        xr::TriggerButton fire, alt_fire;
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
    // Controller state into the original client inputs (right-handed layout): head-relative
    // locomotion through CBloodClientShell::locomotion_yaw, snap turns, weapon cycling, items, and
    // converged aim (shots leave the eye toward the point the right controller designates).
    void xr_apply_controls(GameSession&, XrPlayer&, const XrFrameInput&, const XrTracking&, const xr::Vec3& reference,
                           float units_per_meter);
} // namespace OpenBlood2
