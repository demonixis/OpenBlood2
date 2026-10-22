#include "app/xr_player.h"
#include "player_fixture.h"
#include <cmath>
#include <iostream>
#include <optional>
#include <string_view>
using namespace OpenBlood2;
#define CHECK(x)                                                                                                       \
    do                                                                                                                 \
    {                                                                                                                  \
        if (!(x))                                                                                                      \
        {                                                                                                              \
            std::cerr << __func__ << ':' << __LINE__ << ": " #x "\n";                                                  \
            return false;                                                                                              \
        }                                                                                                              \
    } while (false)
namespace
{
    constexpr float quarter = 1.5707963f, scale = 48;
    const xr::Vec3 reference{0, 1.6f, 0};

    bool near(float a, float b, float epsilon)
    {
        return std::fabs(a - b) <= epsilon;
    }
    float wrap(float angle)
    {
        return std::remainder(angle, 6.2831853f);
    }
    xr::Quat xr_yaw(float radians)
    {
        return {0, std::sin(radians / 2), 0, std::cos(radians / 2)};
    }
    // A focused frame: eyes at the calibrated reference, looking along `head_yaw` (OpenXR sense).
    XrFrameInput frame_at(float head_yaw = 0)
    {
        XrFrameInput f;
        f.should_render = f.views_valid = f.focused = true;
        for (std::size_t i = 0; i < 2; ++i)
        {
            const auto side = xr::rotate(xr_yaw(head_yaw), {i ? 0.032f : -0.032f, 0, 0});
            f.eye_pose[i] = {{side[0], 1.6f, side[2]}, xr_yaw(head_yaw)};
            f.eye_fov[i] = {-0.8f, 0.8f, 0.8f, -0.8f};
        }
        return f;
    }
    bool enter(GameSession& g, const std::filesystem::path& root)
    {
        std::string error;
        CHECK(test_player::setup(g, root) && g.enter_player(error) && g.pump_player_messages(error));
        WorldModel floor;
        floor.points = {{-4000, 0, -4000}, {4000, 0, -4000}, {4000, 0, 4000}, {-4000, 0, 4000}};
        floor.surfaces.push_back({});
        floor.surfaces[0].flags = 1;
        floor.polygons.push_back({0, {0, 1, 2, 3}});
        CHECK(g.collision.build(floor, error));
        for (int i = 0; i < 30; ++i)
            CHECK(g.step(1.0 / 60));
        return true;
    }
    // Applies the controls and steps, like the XR loop.
    bool play(GameSession& g, XrPlayer& p, const XrFrameInput& f, int frames)
    {
        for (int i = 0; i < frames; ++i)
        {
            xr_apply_controls(g, p, f, xr_tracking(g, p, f, reference, scale), reference, scale, 1.0 / 60);
            CHECK(g.step(1.0 / 60));
        }
        return true;
    }

    bool locomotion(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(enter(g, root));
        XrPlayer p;
        auto f = frame_at();
        f.controls.move = {0, 0.6f};
        const auto from = g.client_shell.position;
        CHECK(play(g, p, f, 40));
        const float yaw = p.body_yaw;
        // Walked along the head's (= body's) forward, not sideways.
        const DVector delta = vector_subtract(g.client_shell.position, from);
        const float along = delta.x * std::sin(yaw) + delta.z * std::cos(yaw);
        const float across = delta.x * std::cos(yaw) - delta.z * std::sin(yaw);
        CHECK(along > 20 && std::fabs(across) < along * 0.05f);
        CHECK(g.client_shell.locomotion_yaw && near(wrap(*g.client_shell.locomotion_yaw - yaw), 0, 1e-3f));
        CHECK(!g.client_shell.movement_input.run && g.client_shell.movement_input.forward);
        // Head turned 90 degrees left: forward on the stick now walks to the body's left.
        f = frame_at(quarter);
        f.controls.move = {0, 1};
        const auto turned = g.client_shell.position;
        CHECK(play(g, p, f, 40));
        const DVector left = vector_subtract(g.client_shell.position, turned);
        const float leftward = -(left.x * std::cos(yaw) - left.z * std::sin(yaw));
        CHECK(leftward > 20 && g.client_shell.movement_input.run);
        // Running needs full deflection (no run toggle).
        f = frame_at();
        f.controls.move = {0, 0.8f};
        CHECK(play(g, p, f, 1) && !g.client_shell.movement_input.run);
        // Options > VR MOVE [RIGHT HAND]: forward follows the right hand's yaw, not the head's.
        p.hand_direction = true;
        f.hands[1].aim_valid = true;
        f.hands[1].aim = {{0.2f, 1.3f, -0.3f}, xr_yaw(-quarter)}; // Pointing a quarter turn right.
        f.controls.move = {0, 1};
        CHECK(play(g, p, f, 1) && g.client_shell.locomotion_yaw &&
              near(wrap(*g.client_shell.locomotion_yaw - p.body_yaw - quarter), 0, 1e-3f));
        // Pointing straight down falls back to the head.
        f.hands[1].aim.orientation = {-std::sin(0.75f), 0, 0, std::cos(0.75f)};
        CHECK(play(g, p, f, 1) && near(wrap(*g.client_shell.locomotion_yaw - p.body_yaw), 0, 1e-3f));
        return true;
    }

    bool aim(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(enter(g, root));
        XrPlayer p;
        auto f = frame_at();
        // Right controller below and right of the eye, pointing straight ahead: the converged aim is
        // the body's forward (the far point barely shifts it).
        f.hands[1].aim_valid = true;
        f.hands[1].aim = {{0.2f, 1.3f, -0.3f}, {0, 0, 0, 1}};
        CHECK(play(g, p, f, 1));
        CHECK(near(wrap(g.client_shell.rotation.yaw - p.body_yaw), 0, 0.01f) &&
              near(g.client_shell.rotation.pitch, 0, 0.01f));
        // Pointing right (OpenXR -90 degrees about +Y) aims a quarter turn right of the body.
        f.hands[1].aim.orientation = xr_yaw(-quarter);
        CHECK(play(g, p, f, 1));
        CHECK(near(wrap(g.client_shell.rotation.yaw - p.body_yaw - quarter), 0, 0.02f));
        // Pointing down at the floor converges on the hit point below.
        const float down = 0.5f;
        f.hands[1].aim.orientation = {-std::sin(down / 2), 0, 0, std::cos(down / 2)};
        CHECK(play(g, p, f, 1));
        CHECK(g.client_shell.rotation.pitch > 0.3f);
        // No left-hand weapon: the left hand never redirects fire.
        f.hands[0].aim_valid = true;
        f.hands[0].aim = {{-0.2f, 1.3f, -0.3f}, xr_yaw(quarter)};
        CHECK(play(g, p, f, 1) && !g.client_shell.left_hand_forward);
        // The body yaw is not the aim: aiming right did not turn the body.
        const float body = p.body_yaw;
        f.hands[1].aim.orientation = xr_yaw(-quarter);
        CHECK(play(g, p, f, 5) && near(p.body_yaw, body, 1e-6f));
        return true;
    }

    bool turning(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(enter(g, root));
        XrPlayer p;
        auto f = frame_at();
        CHECK(play(g, p, f, 1));
        const float start = p.body_yaw;
        f.controls.right_stick = {1, 0.2f};
        CHECK(play(g, p, f, 3)); // One snap per deflection.
        CHECK(near(p.body_yaw - start, xr_snap_angle, 1e-5f));
        f.controls.right_stick = {0, 0};
        CHECK(play(g, p, f, 1));
        f.controls.right_stick = {-0.9f, 0};
        CHECK(play(g, p, f, 2) && near(p.body_yaw, start, 1e-5f));
        // A server-driven turn (start point, teleport) is adopted into the body yaw.
        f.controls.right_stick = {0, 0};
        CHECK(play(g, p, f, 1));
        const float before = p.body_yaw;
        g.client_shell.rotation.yaw += 1;
        CHECK(play(g, p, f, 1) && near(p.body_yaw - before, 1, 1e-4f));
        // The vertical axis cycles weapons instead of turning, past 0.85 only.
        f.controls.right_stick = {0.2f, 0.8f};
        const float held = p.body_yaw;
        CHECK(play(g, p, f, 2) && near(p.body_yaw, held, 1e-6f) && p.weapon.armed);
        f.controls.right_stick = {0.2f, 1};
        CHECK(play(g, p, f, 2) && near(p.body_yaw, held, 1e-6f) && !p.weapon.armed);
        // Options > VR: another snap step.
        f.controls.right_stick = {0, 0};
        CHECK(play(g, p, f, 1));
        p.turn_step = 1.5707963f;
        const float stepped = p.body_yaw;
        f.controls.right_stick = {-1, 0};
        CHECK(play(g, p, f, 3) && near(p.body_yaw - stepped, -1.5707963f, 1e-5f));
        // Smooth turning: proportional to deflection beyond the dead zone and to time.
        p.turn_step = 0;
        f.controls.right_stick = {0, 0};
        CHECK(play(g, p, f, 1));
        const float smooth = p.body_yaw;
        f.controls.right_stick = {1, 0};
        CHECK(play(g, p, f, 30) && near(p.body_yaw - smooth, xr_smooth_turn_speed * 0.5f, 1e-4f));
        f.controls.right_stick = {-0.6f, 0};
        const float back = p.body_yaw;
        CHECK(play(g, p, f, 60) && near(p.body_yaw - back, -xr_smooth_turn_speed * 0.5f, 1e-4f));
        f.controls.right_stick = {0.15f, 0}; // Inside the dead zone.
        const float still = p.body_yaw;
        CHECK(play(g, p, f, 10) && near(p.body_yaw, still, 1e-6f));
        // A scripted camera keeps the player's rotation: no turning, no aim.
        auto& shell = g.client_shell;
        const auto camera = g.objects.create(g.objects.find_class("BaseClass"), {});
        CHECK(camera);
        shell.view.object = camera;
        const float yaw = shell.rotation.yaw, pitch = shell.rotation.pitch, body = p.body_yaw;
        f.controls.right_stick = {1, 0};
        f.hands[1].aim_valid = true;
        f.hands[1].aim = {{0.2f, 1.3f, -0.3f}, xr_yaw(-quarter)};
        xr_apply_controls(g, p, f, xr_tracking(g, p, f, reference, scale), reference, scale, 1.0 / 60);
        CHECK(near(p.body_yaw, body, 1e-6f) && shell.rotation.yaw == yaw && shell.rotation.pitch == pitch);
        shell.view.object = nullptr;
        return true;
    }

    bool buttons(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(enter(g, root));
        XrPlayer p;
        auto f = frame_at();
        f.controls.fire = 0.6f;
        f.controls.use = f.controls.jump = f.controls.crouch = true;
        auto t = xr_tracking(g, p, f, reference, scale);
        const auto apply = [&] { xr_apply_controls(g, p, f, t, reference, scale, 1.0 / 60); };
        apply();
        auto& s = g.client_shell;
        CHECK(s.weapon_input.primary && !s.weapon_input.alternate && s.weapon_input.all_hands && s.use_down);
        CHECK(s.movement_input.jump && s.movement_input.crouch);
        // The left stick click jumps too.
        f.controls.jump = false;
        f.controls.stick_jump = true;
        apply();
        CHECK(s.movement_input.jump);
        f.controls.stick_jump = false;
        f.controls.fire = 0.4f; // Hysteresis keeps the trigger held.
        f.controls.next_item = true;
        s.next_item_requested = s.prev_item_requested = false;
        apply();
        CHECK(s.weapon_input.primary && s.next_item_requested && !s.prev_item_requested && !s.movement_input.jump);
        s.next_item_requested = false;
        apply(); // Held: no second request.
        CHECK(!s.next_item_requested);
        f.controls.next_item = false;
        f.controls.prev_item = true;
        apply();
        CHECK(s.prev_item_requested && !s.next_item_requested);
        f.controls.fire = 0.2f;
        apply();
        CHECK(!s.weapon_input.primary);
        // The right grip uses the current item past 0.85, once per squeeze.
        s.use_item_requested = false;
        f.controls.use_item = 0.8f;
        apply();
        CHECK(!s.use_item_requested);
        f.controls.use_item = 0.9f;
        apply();
        CHECK(s.use_item_requested);
        s.use_item_requested = false;
        f.controls.use_item = 0.7f;
        apply();
        CHECK(!s.use_item_requested);
        f.controls.use_item = 0.5f;
        apply();
        f.controls.use_item = 0.95f;
        apply();
        CHECK(s.use_item_requested);
        return true;
    }
    // Virtual shoulders: below and behind the head, across a body yaw with a 45-degree dead zone
    // kept relative to the tracking yaw.
    bool shoulders()
    {
        XrTracking t;
        t.yaw = 0.3f;
        t.stereo.head = {{10, 70, 5}, xr::yaw_rotation(0.3f)};
        std::optional<float> body;
        auto s = xr_shoulders(t, scale, body);
        CHECK(body && near(*body, 0, 1e-5f));
        const auto right = xr::rotate(xr::yaw_rotation(0.3f), {1, 0, 0});
        const xr::Vec3 across{s.shoulders[1][0] - s.shoulders[0][0], s.shoulders[1][1] - s.shoulders[0][1],
                              s.shoulders[1][2] - s.shoulders[0][2]};
        CHECK(near(across[0] * right[0] + across[2] * right[2], .34f * scale, 1e-3f) && near(across[1], 0, 1e-4f));
        // 22 cm below the eyes, 9 cm behind them.
        const auto forward = xr::rotate(xr::yaw_rotation(0.3f), {0, 0, 1});
        const float mid_y = (s.shoulders[0][1] + s.shoulders[1][1]) / 2;
        const float mid_back = ((s.shoulders[0][0] + s.shoulders[1][0]) / 2 - 10) * forward[0] +
                               ((s.shoulders[0][2] + s.shoulders[1][2]) / 2 - 5) * forward[2];
        CHECK(near(mid_y, 70 - .22f * scale, 1e-3f) && near(mid_back, -.09f * scale, 1e-3f));
        // Elbows: down, outward, back.
        for (std::size_t side = 0; side < 2; ++side)
        {
            const auto& pole = s.poles[side];
            const float out = (pole[0] * right[0] + pole[2] * right[2]) * (side ? 1.f : -1.f);
            CHECK(pole[1] < -.9f && out > .4f && pole[0] * forward[0] + pole[2] * forward[2] < 0);
        }
        // Within the dead zone the body stays; beyond it it trails the head by 45 degrees.
        t.stereo.head.orientation = xr::yaw_rotation(0.3f + 0.6f);
        xr_shoulders(t, scale, body);
        CHECK(near(*body, 0, 1e-5f));
        t.stereo.head.orientation = xr::yaw_rotation(0.3f + 1.4f);
        xr_shoulders(t, scale, body);
        CHECK(near(*body, 1.4f - xr_body_dead_zone, 1e-4f));
        t.stereo.head.orientation = xr::yaw_rotation(0.3f - 1.2f);
        xr_shoulders(t, scale, body);
        CHECK(near(*body, -1.2f + xr_body_dead_zone, 1e-4f));
        // A snap turn moves the tracking yaw and the head together: the relative body yaw holds.
        const float before = *body;
        t.yaw += xr_snap_angle;
        t.stereo.head.orientation = xr::yaw_rotation(t.yaw - 1.2f);
        xr_shoulders(t, scale, body);
        CHECK(near(*body, before, 1e-4f));
        // Looking straight down keeps the body yaw.
        t.stereo.head.orientation = xr::mul(xr::yaw_rotation(t.yaw + 1.f), xr::Quat{0.7071068f, 0, 0, 0.7071068f});
        xr_shoulders(t, scale, body);
        CHECK(near(*body, before, 1e-4f));
        return true;
    }
} // namespace

int main(int argc, char** argv)
{
    if (argc < 3)
        return 2;
    const std::string_view test = argv[1];
    const std::filesystem::path root = argv[2];
    const bool ok = test == "locomotion"  ? locomotion(root)
                    : test == "aim"       ? aim(root)
                    : test == "turning"   ? turning(root)
                    : test == "buttons"   ? buttons(root)
                    : test == "shoulders" ? shoulders()
                                          : false;
    return ok ? 0 : 1;
}
