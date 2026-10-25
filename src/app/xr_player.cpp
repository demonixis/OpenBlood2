#include "app/xr_player.h"
#include <algorithm>
#include <cmath>
#include <spdlog/spdlog.h>
#include <string>

namespace OpenBlood2
{
    namespace
    {
        xr::Vec3 vec(DVector v)
        {
            return {v.x, v.y, v.z};
        }
        DVector dvec(const xr::Vec3& v)
        {
            return {v[0], v[1], v[2]};
        }
    } // namespace

    XrTracking xr_tracking(const GameSession& game, const XrPlayer& player, const XrFrameInput& frame,
                           const xr::Vec3& reference, float units_per_meter)
    {
        const auto& client = game.client_shell;
        const auto& view = client.view;
        XrTracking t;
        t.base = vec(view.position);
        if (player.initialized && !view.object && !client.death_camera.active)
            t.yaw = player.body_yaw;
        else
        {
            const auto forward = rotate_vector(DVector{0, 0, 1}, view.rotation);
            t.yaw = std::atan2(forward.x, forward.z);
        }
        t.stereo = xr::stereo_camera(t.base, t.yaw, {frame.eye_pose[0], frame.eye_pose[1]}, reference, units_per_meter,
                                     xr_max_lean);
        return t;
    }

    xr::Pose xr_world_pose(const XrTracking& t, const xr::Vec3& reference, float units_per_meter,
                           const xr::Pose& tracked)
    {
        return xr::tracked_pose(t.base, t.yaw, reference, t.stereo.shift, units_per_meter, tracked);
    }

    XrShoulders xr_shoulders(const XrTracking& t, float units_per_meter, std::optional<float>& body_yaw)
    {
        const auto wrap = [](float a) { return std::remainder(a, 6.2831853f); };
        const auto& head = t.stereo.head;
        const auto forward = xr::rotate(head.orientation, {0, 0, 1});
        if (std::hypot(forward[0], forward[2]) > .2f) // Looking straight up or down keeps the body.
        {
            const float relative = wrap(std::atan2(forward[0], forward[2]) - t.yaw);
            if (!body_yaw)
                body_yaw = relative;
            const float lag = wrap(relative - *body_yaw);
            if (std::abs(lag) > xr_body_dead_zone)
                body_yaw = wrap(relative - std::copysign(xr_body_dead_zone, lag));
        }
        const auto yaw = xr::yaw_rotation(t.yaw + body_yaw.value_or(0));
        const float m = units_per_meter;
        const auto neck = xr::rotate(head.orientation, {0, -.10f * m, -.09f * m});
        XrShoulders out;
        for (std::size_t side = 0; side < 2; ++side)
        {
            const float sign = side ? 1.f : -1.f;
            const auto across = xr::rotate(yaw, {sign * .17f * m, -.12f * m, 0});
            for (std::size_t i = 0; i < 3; ++i)
                out.shoulders[side][i] = head.position[i] + neck[i] + across[i];
            out.poles[side] = xr::rotate(yaw, {sign * .45f, -1, -.35f});
        }
        return out;
    }

    xr::Vec3 xr_aim_target(GameSession& game, const xr::Pose& aim)
    {
        const auto direction = xr::rotate(aim.orientation, {0, 0, 1});
        const xr::Vec3 end{aim.position[0] + direction[0] * xr_aim_range, aim.position[1] + direction[1] * xr_aim_range,
                           aim.position[2] + direction[2] * xr_aim_range};
        SegmentHit hit;
        if (game.client_shell.player &&
            game.server.IntersectSegment(dvec(aim.position), dvec(end), game.client_shell.player, hit) ==
                SegmentResult::Hit)
            return vec(hit.point);
        return end;
    }

    void xr_apply_controls(GameSession& game, XrPlayer& player, const XrFrameInput& frame, const XrTracking& t,
                           const xr::Vec3& reference, float units_per_meter, double seconds)
    {
        auto& shell = game.client_shell;
        shell.xr_view = true;
        shell.third_person = shell.third_person_active = false;
        const auto& c = frame.controls;
        // Adopt server-driven turns (start points, teleports, restores) into the body yaw.
        if (!player.initialized)
        {
            player.body_yaw = shell.rotation.yaw;
            player.initialized = true;
        }
        else if (shell.rotation.yaw != player.written_yaw)
            player.body_yaw += shell.rotation.yaw - player.written_yaw;
        // Right stick: the dominant axis turns (snap or smooth) or, pushed past 0.85, cycles weapons;
        // nothing during a scripted camera (its sticks re-arm on release as usual).
        const bool cinematic = shell.view.object != nullptr;
        const auto right_stick = cinematic ? std::array<float, 2>{} : c.right_stick;
        const bool turning = std::fabs(right_stick[0]) >= std::fabs(right_stick[1]);
        const float turn = turning ? right_stick[0] : 0;
        if (player.turn_step > 0)
            player.body_yaw += static_cast<float>(player.turn.update(turn)) * player.turn_step;
        else if (std::isfinite(seconds) && seconds > 0)
            player.body_yaw += xr::stick_axis(turn) * xr_smooth_turn_speed * static_cast<float>(seconds);
        if (const int cycle = player.weapon.update(turning ? 0 : right_stick[1], xr_weapon_deflection))
        {
            std::string why;
            if (!game.cycle_weapon(cycle > 0, why))
                spdlog::info("Weapon selection: {}", why);
        }
        // Head- or right-hand-relative locomotion (the head while the hand points up or down); the aim
        // stays the controller's.
        float reference_yaw = xr::forward_yaw(t.stereo.head.orientation);
        if (player.hand_direction && frame.hands[1].aim_valid)
        {
            const auto hand = xr_world_pose(t, reference, units_per_meter, frame.hands[1].aim);
            const auto forward = xr::rotate(hand.orientation, {0, 0, 1});
            if (std::hypot(forward[0], forward[2]) > .2f)
                reference_yaw = std::atan2(forward[0], forward[2]);
        }
        const auto move = xr::locomotion(c.move, reference_yaw);
        shell.movement_input = move.input; // Full deflection runs.
        shell.movement_input.jump = c.jump || c.stick_jump;
        shell.movement_input.crouch = c.crouch;
        shell.locomotion_yaw = move.yaw;
        shell.weapon_input.primary = player.fire.update(c.fire);
        shell.weapon_input.alternate = player.alt_fire.update(c.alt_fire);
        shell.weapon_input.all_hands = true; // Original HandleFiring: both carried hands.
        shell.use_down = c.use;
        shell.detonate_down = c.detonate;
        shell.prev_item_requested |= player.prev_item.update(c.prev_item);
        shell.next_item_requested |= player.next_item.update(c.next_item);
        shell.use_item_requested |=
            player.use_item.update(player.grip.update(c.use_item, xr_grip_press, xr_grip_release));
        if (cinematic)
        {
            player.written_pitch = shell.rotation.pitch;
            player.written_yaw = shell.rotation.yaw;
            return;
        }
        // Aim from the eye (the original fire origin) toward the controller's designated point.
        xr::Vec3 target;
        if (frame.hands[1].aim_valid)
            target = xr_aim_target(game, xr_world_pose(t, reference, units_per_meter, frame.hands[1].aim));
        else
        {
            const auto look = xr::rotate(t.stereo.head.orientation, {0, 0, 1});
            const auto& eye = t.stereo.head.position;
            target = {eye[0] + look[0] * xr_aim_range, eye[1] + look[1] * xr_aim_range,
                      eye[2] + look[2] * xr_aim_range};
        }
        float pitch = shell.rotation.pitch, yaw = shell.rotation.yaw;
        if (xr::aim_angles(vec(shell.camera_position), target, pitch, yaw))
        {
            shell.rotation.pitch = pitch;
            shell.rotation.yaw = yaw;
        }
        player.written_pitch = shell.rotation.pitch;
        player.written_yaw = shell.rotation.yaw;
        // A left-hand weapon (dual weapons) fires toward the left controller's point.
        shell.left_hand_forward.reset();
        const bool left_weapon = std::any_of(shell.weapons.begin(), shell.weapons.end(),
                                             [](const CViewWeapon& w) { return w.left && w.model; });
        if (left_weapon && frame.hands[0].aim_valid)
        {
            const auto left = xr_aim_target(game, xr_world_pose(t, reference, units_per_meter, frame.hands[0].aim));
            float left_pitch = 0, left_yaw = 0;
            if (xr::aim_angles(vec(shell.camera_position), left, left_pitch, left_yaw))
            {
                const float cp = std::cos(left_pitch);
                shell.left_hand_forward =
                    DVector{std::sin(left_yaw) * cp, -std::sin(left_pitch), std::cos(left_yaw) * cp};
            }
        }
    }
} // namespace OpenBlood2
