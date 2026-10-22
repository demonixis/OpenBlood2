#include "app/xr_input.h"
#include <cmath>
#include <iostream>
#include <string_view>
using namespace OpenBlood2::xr;
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
    bool near(float a, float b, float epsilon = 1e-4f)
    {
        return std::fabs(a - b) <= epsilon;
    }
    constexpr float quarter = 1.5707963f;
    Quat xr_yaw_quat(float radians)
    {
        return {0, std::sin(radians / 2), 0, std::cos(radians / 2)};
    }

    bool sectors()
    {
        // Inside the dead zone: no flags, the reference yaw.
        auto l = locomotion({0.1f, 0.1f}, 0.3f);
        CHECK(!l.moving && !l.input.forward && near(l.yaw, 0.3f));
        // Straight ahead walks forward at the reference yaw; full deflection runs.
        l = locomotion({0, 0.5f}, 0.3f);
        CHECK(l.moving && l.input.forward && !l.input.right && !l.input.left && !l.input.run && near(l.yaw, 0.3f));
        CHECK(locomotion({0, 1}, 0).input.run);
        // Right is a strafe; backward-left keeps both original flags.
        l = locomotion({1, 0}, 0);
        CHECK(l.input.right && !l.input.forward && !l.input.backward && near(l.yaw, 0));
        l = locomotion({-0.7f, -0.7f}, 0);
        CHECK(l.input.backward && l.input.left && !l.input.forward && near(l.yaw, 0, 1e-3f));
        // Between sectors the residual turns the yaw: 15 degrees right is forward plus 15 degrees.
        l = locomotion({std::sin(0.2617994f), std::cos(0.2617994f)}, 1);
        CHECK(l.input.forward && !l.input.right && near(l.yaw, 1 + 0.2617994f));
        // 30 degrees right is the forward-right sector minus 15 degrees.
        l = locomotion({std::sin(0.5235988f), std::cos(0.5235988f)}, 1);
        CHECK(l.input.forward && l.input.right && near(l.yaw, 1 - 0.2617994f));
        // 60 degrees right is the forward-right sector plus 15 degrees.
        l = locomotion({std::sin(1.0471976f), std::cos(1.0471976f)}, 0);
        CHECK(l.input.forward && l.input.right && near(l.yaw, 1.0471976f - 0.7853982f));
        // Straight back is the backward flag at the reference yaw (either sign of the angle).
        l = locomotion({-1e-6f, -1}, 0.5f);
        CHECK(l.input.backward && !l.input.left && !l.input.right && near(l.yaw, 0.5f, 1e-3f));
        // Invalid input is ignored.
        CHECK(!locomotion({NAN, 1}, 0).moving);
        return true;
    }

    bool edges()
    {
        StickEdge edge;
        CHECK(edge.update(0.5f) == 0 && edge.update(0.8f) == 1 && edge.update(1) == 0 && edge.update(0.5f) == 0);
        CHECK(edge.update(0.2f) == 0 && edge.update(-0.9f) == -1 && edge.update(NAN) == 0 && edge.update(-0.9f) == -1);
        // The weapon threshold: 0.85 to fire.
        CHECK(edge.update(0) == 0 && edge.update(0.84f, .85f) == 0 && edge.update(0.86f, .85f) == 1);
        TriggerButton trigger;
        CHECK(!trigger.update(0.5f) && trigger.update(0.6f) && trigger.update(0.4f) && !trigger.update(0.3f));
        TriggerButton grip; // The right grip: 0.85 to press, 0.6 to release.
        CHECK(!grip.update(0.8f, .85f, .6f) && grip.update(0.9f, .85f, .6f) && grip.update(0.7f, .85f, .6f) &&
              !grip.update(0.5f, .85f, .6f));
        // Smooth axes: zero in the dead zone, then rescaled to +-1.
        CHECK(stick_axis(0.2f) == 0 && stick_axis(-0.1f) == 0 && near(stick_axis(0.6f), 0.5f) &&
              near(stick_axis(-1), -1) && near(stick_axis(1.2f), 1) && stick_axis(NAN) == 0);
        PressEdge press;
        CHECK(press.update(true) && !press.update(true) && !press.update(false) && press.update(true));
        return true;
    }

    bool aim()
    {
        float pitch = 0, yaw = 0;
        // Straight ahead (+Z), to the right (+X) and up (+Y, pitch negative in the original convention).
        CHECK(aim_angles({0, 0, 0}, {0, 0, 5}, pitch, yaw) && near(pitch, 0) && near(yaw, 0));
        CHECK(aim_angles({1, 2, 3}, {11, 2, 3}, pitch, yaw) && near(yaw, quarter) && near(pitch, 0));
        CHECK(aim_angles({0, 0, 0}, {0, 10, 0.001f}, pitch, yaw) && near(pitch, -1.48f));
        CHECK(!aim_angles({1, 1, 1}, {1, 1, 1}, pitch, yaw));
        // The angles reproduce the original forward vector toward the target.
        CHECK(aim_angles({0, 0, 0}, {3, -4, 12}, pitch, yaw));
        const float cp = std::cos(pitch);
        CHECK(near(std::sin(yaw) * cp * 13, 3, 1e-3f) && near(-std::sin(pitch) * 13, -4, 1e-3f) &&
              near(std::cos(yaw) * cp * 13, 12, 1e-3f));
        CHECK(near(forward_yaw(yaw_rotation(0.7f)), 0.7f));
        return true;
    }

    bool hands()
    {
        // A hand 0.4 m in front of the reference lands 0.4 m * scale ahead along the game yaw, and
        // the lean clamp's shift moves it with the head.
        const Pose hand{{0, 1.2f, -0.4f}, {0, 0, 0, 1}};
        auto p = tracked_pose({10, 20, 30}, quarter, {0, 1.6f, 0}, {0, 0, 0}, 50, hand);
        CHECK(near(p.position[0], 10 + 20, 1e-3f) && near(p.position[1], 20 - 20, 1e-3f) &&
              near(p.position[2], 30, 1e-3f));
        CHECK(near(forward_yaw(p.orientation), quarter));
        p = tracked_pose({0, 0, 0}, 0, {0, 0, 0}, {0, 0, -0.1f}, 10, hand);
        CHECK(near(p.position[2], 3, 1e-3f));
        return true;
    }
    bool panel()
    {
        // A 0.7 x 0.525 m panel 0.75 m ahead (OpenXR -Z), facing the user.
        const Pose quad{{0, 1.5f, -0.75f}, {0, 0, 0, 1}};
        std::array<float, 2> uv{};
        float distance = 0;
        CHECK(panel_hit(quad, 0.7f, 0.525f, {0, 1.5f, 0}, {0, 0, -1}, uv, distance));
        CHECK(near(uv[0], 0.5f) && near(uv[1], 0.5f) && near(distance, 0.75f));
        // Toward the upper-left quarter: u and v below one half (v grows downward).
        const float x = -0.175f, y = 0.13f, length = std::sqrt(x * x + y * y + 0.75f * 0.75f);
        CHECK(panel_hit(quad, 0.7f, 0.525f, {0, 1.5f, 0}, {x / length, y / length, -0.75f / length}, uv, distance));
        CHECK(near(uv[0], 0.25f, 1e-3f) && near(uv[1], 0.5f - 0.13f / 0.525f, 1e-3f));
        // Outside the bounds, from behind, parallel or pointing away: no hit.
        CHECK(!panel_hit(quad, 0.7f, 0.525f, {0, 1.5f, 0}, {0.8f, 0, -0.6f}, uv, distance));
        CHECK(!panel_hit(quad, 0.7f, 0.525f, {0, 1.5f, -2}, {0, 0, 1}, uv, distance));
        CHECK(!panel_hit(quad, 0.7f, 0.525f, {0, 1.5f, 0}, {1, 0, 0}, uv, distance));
        CHECK(!panel_hit(quad, 0.7f, 0.525f, {0, 1.5f, 0}, {0, 0, 1}, uv, distance));
        // A panel turned 90 degrees (yaw) to the user's left is hit looking left.
        const Pose left{{-0.75f, 1.5f, 0}, xr_yaw_quat(quarter)};
        CHECK(panel_hit(left, 0.7f, 0.525f, {0, 1.5f, 0}, {-1, 0, 0}, uv, distance) && near(uv[0], 0.5f));
        return true;
    }
} // namespace

int main(int argc, char** argv)
{
    const std::string_view test = argc > 1 ? argv[1] : "";
    const bool ok = test == "sectors" ? sectors()
                    : test == "edges" ? edges()
                    : test == "aim"   ? aim()
                    : test == "hands" ? hands()
                    : test == "panel" ? panel()
                                      : false;
    return ok ? 0 : 1;
}
