#include "app/xr_player.h"
#include "player_fixture.h"
#include <cmath>
#include <iostream>
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
            xr_apply_controls(g, p, f, xr_tracking(g, p, f, reference, scale), reference, scale);
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
        // A run latch keeps running at partial deflection.
        f = frame_at();
        f.controls.move = {0, 0.5f};
        f.controls.run = true;
        CHECK(play(g, p, f, 1));
        f.controls.run = false;
        CHECK(play(g, p, f, 1) && g.client_shell.movement_input.run && p.run_latch);
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
        // The vertical axis cycles weapons instead of turning.
        f.controls.right_stick = {0.2f, 1};
        const float held = p.body_yaw;
        CHECK(play(g, p, f, 2) && near(p.body_yaw, held, 1e-6f) && !p.weapon.armed);
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
        xr_apply_controls(g, p, f, t, reference, scale);
        auto& s = g.client_shell;
        CHECK(s.weapon_input.primary && !s.weapon_input.alternate && s.weapon_input.all_hands && s.use_down);
        CHECK(s.movement_input.jump && s.movement_input.crouch);
        f.controls.fire = 0.4f; // Hysteresis keeps the trigger held.
        f.controls.next_item = true;
        s.next_item_requested = false;
        xr_apply_controls(g, p, f, t, reference, scale);
        CHECK(s.weapon_input.primary && s.next_item_requested);
        s.next_item_requested = false;
        xr_apply_controls(g, p, f, t, reference, scale); // Held: no second request.
        CHECK(!s.next_item_requested);
        f.controls.fire = 0.2f;
        xr_apply_controls(g, p, f, t, reference, scale);
        CHECK(!s.weapon_input.primary);
        return true;
    }
} // namespace

int main(int argc, char** argv)
{
    if (argc < 3)
        return 2;
    const std::string_view test = argv[1];
    const std::filesystem::path root = argv[2];
    const bool ok = test == "locomotion" ? locomotion(root)
                    : test == "aim"      ? aim(root)
                    : test == "turning"  ? turning(root)
                    : test == "buttons"  ? buttons(root)
                                         : false;
    return ok ? 0 : 1;
}
