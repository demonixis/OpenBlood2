#include "engine/xr/xr_math.h"
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
    bool near(float a, float b, float epsilon = 1e-5f)
    {
        return std::fabs(a - b) <= epsilon;
    }
    bool near(const Vec3& a, const Vec3& b)
    {
        return near(a[0], b[0]) && near(a[1], b[1]) && near(a[2], b[2]);
    }
    // Right-handed OpenXR rotation about +Y (counter-clockwise seen from above).
    Quat xr_yaw(float radians)
    {
        return {0, std::sin(radians / 2), 0, std::cos(radians / 2)};
    }
    Quat xr_pitch(float radians)
    {
        return {std::sin(radians / 2), 0, 0, std::cos(radians / 2)};
    }

    bool axes()
    {
        // Identity: OpenXR looks down -Z; the engine looks down +Z with the same right/up.
        const Basis identity = basis(engine_orientation({0, 0, 0, 1}));
        CHECK(near(identity.right, {1, 0, 0}) && near(identity.up, {0, 1, 0}) && near(identity.forward, {0, 0, 1}));
        // A point one metre in front of the user, 1.6 m up, keeps its meaning at 40 units/m.
        CHECK(near(engine_position({0.25f, 1.6f, -1}, 40), {10, 64, 40}));
        // Turning the head left in OpenXR (+90 degrees about +Y) looks along -X in both frames.
        const Basis left = basis(engine_orientation(xr_yaw(1.5707963f)));
        CHECK(near(left.forward, {-1, 0, 0}) && near(left.right, {0, 0, 1}) && near(left.up, {0, 1, 0}));
        // Looking up (+90 degrees about +X in OpenXR) points forward along +Y.
        const Basis up = basis(engine_orientation(xr_pitch(1.5707963f)));
        CHECK(near(up.forward, {0, 1, 0}) && near(up.up, {0, 0, -1}));
        // The converted basis equals the reflected OpenXR basis for an arbitrary orientation.
        const float h = 0.5f;
        const Quat mixed{h * 0.3f, h * 0.8f, h * 0.2f, std::sqrt(1 - h * h * (0.09f + 0.64f + 0.04f))};
        const Vec3 xr_forward = rotate(mixed, {0, 0, -1});
        const Vec3 xr_right = rotate(mixed, {1, 0, 0});
        const Basis converted = basis(engine_orientation(mixed));
        CHECK(near(converted.forward, {xr_forward[0], xr_forward[1], -xr_forward[2]}));
        CHECK(near(converted.right, {xr_right[0], xr_right[1], -xr_right[2]}));
        return true;
    }

    bool projections()
    {
        // Symmetric 90 degrees matches the desktop projection {1, aspect 1}.
        const Projection square = projection({-0.7853982f, 0.7853982f, 0.7853982f, -0.7853982f});
        CHECK(near(square.scale_x, 1) && near(square.scale_y, 1) && near(square.offset_x, 0) &&
              near(square.offset_y, 0));
        // An asymmetric eye maps its own tangents to the NDC edges.
        const Fov eye{-0.95f, 0.80f, 0.85f, -0.90f};
        CHECK(valid_fov(eye));
        const Pose origin{};
        const LineUniform u = line_uniform(origin, eye, 0.05f, 100);
        const auto ndc = [&u](const Vec3& p)
        {
            const auto c = clip(u, p);
            return std::array<float, 3>{c[0] / c[3], c[1] / c[3], c[2] / c[3]};
        };
        const float z = 3;
        CHECK(near(ndc({std::tan(eye.left) * z, 0, z})[0], -1, 1e-4f));
        CHECK(near(ndc({std::tan(eye.right) * z, 0, z})[0], 1, 1e-4f));
        CHECK(near(ndc({0, std::tan(eye.up) * z, z})[1], 1, 1e-4f));
        CHECK(near(ndc({0, std::tan(eye.down) * z, z})[1], -1, 1e-4f));
        // Depth spans 0 at the near plane to 1 at the far plane.
        CHECK(near(ndc({0, 0, 0.05f})[2], 0, 1e-4f) && near(ndc({0, 0, 100})[2], 1, 1e-4f));
        // Invalid tracking data is refused before it reaches a projection.
        CHECK(!valid_fov({0.5f, 0.4f, 0.5f, -0.5f}) && !valid_fov({-0.5f, 0.5f, NAN, -0.5f}) &&
              !valid_fov({-1.6f, 0.5f, 0.5f, -0.5f}));
        return true;
    }

    bool eye_pose()
    {
        // A converted pose moves the camera: a point straight ahead of a turned head stays centred.
        const Pose xr_head{{0.1f, 1.7f, 0.3f}, xr_yaw(0.6f)};
        const Pose head = engine_pose(xr_head, 1);
        const Vec3 xr_target = rotate(xr_head.orientation, {0, 0, -2});
        const Vec3 target = engine_position({xr_head.position[0] + xr_target[0], xr_head.position[1] + xr_target[1],
                                             xr_head.position[2] + xr_target[2]},
                                            1);
        const LineUniform u = line_uniform(head, {-0.8f, 0.8f, 0.8f, -0.8f}, 0.05f, 100);
        const auto c = clip(u, target);
        CHECK(near(c[0] / c[3], 0) && near(c[1] / c[3], 0) && near(c[3], 2));
        return true;
    }
    bool stereo()
    {
        // Eyes 64 mm apart at the calibrated reference: the game camera sits between them.
        const std::array<Pose, 2> eyes{Pose{{-0.032f, 1.6f, 0}, {0, 0, 0, 1}}, Pose{{0.032f, 1.6f, 0}, {0, 0, 0, 1}}};
        const Vec3 base{100, 50, -20};
        auto s = stereo_camera(base, 0, eyes, {0, 1.6f, 0}, 45, 0.5f);
        CHECK(near(s.head.position, base));
        CHECK(near(s.eyes[0].position, {100 - 0.032f * 45, 50, -20}) &&
              near(s.eyes[1].position, {100 + 0.032f * 45, 50, -20}));
        // A game yaw of +90 degrees turns forward to +X and the eye baseline to -Z/+Z.
        s = stereo_camera(base, 1.5707963f, eyes, {0, 1.6f, 0}, 45, 0.5f);
        CHECK(near(basis(s.head.orientation).forward, {1, 0, 0}));
        CHECK(near(s.eyes[0].position, {100, 50, -20 + 0.032f * 45}) &&
              near(s.eyes[1].position, {100, 50, -20 - 0.032f * 45}));
        // Standing up 0.1 m raises the eyes; leaning 2 m forward is clamped to 0.5 m.
        std::array<Pose, 2> moved = eyes;
        for (auto& e : moved)
        {
            e.position[1] += 0.1f;
            e.position[2] -= 2;
        }
        s = stereo_camera(base, 0, moved, {0, 1.6f, 0}, 45, 0.5f);
        CHECK(near(s.head.position, {100, 50 + 0.1f * 45, -20 + 0.5f * 45}));
        // Head orientation composes under the game yaw: turning the head left from a +90 game yaw.
        std::array<Pose, 2> turned = eyes;
        for (auto& e : turned)
            e.orientation = xr_yaw(1.5707963f);
        s = stereo_camera(base, 1.5707963f, turned, {0, 1.6f, 0}, 45, 0.5f);
        CHECK(near(basis(s.head.orientation).forward, {0, 0, 1}));
        return true;
    }
} // namespace

int main(int argc, char** argv)
{
    const std::string_view test = argc > 1 ? argv[1] : "";
    const bool ok = test == "axes"         ? axes()
                    : test == "projection" ? projections()
                    : test == "eye_pose"   ? eye_pose()
                    : test == "stereo"     ? stereo()
                                           : false;
    return ok ? 0 : 1;
}
