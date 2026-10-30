#include "client/camera.h"
#include <algorithm>
#include <cmath>
#include <set>
namespace OpenBlood2
{
    bool ClientCamera::begin(std::uint64_t world)
    {
        if (!world || world <= frame_.world)
            return false;
        const bool device_xr = xr_view; // Device mode survives world changes, as in the original shell.
        *this = {};
        xr_view = device_xr;
        frame_.world = world;
        return true;
    }
    bool ClientCamera::receive(ClientCameraFrame&& next, std::string& error)
    {
        error.clear();
        const auto valid = [](const ClientCameraTarget& target)
        {
            const auto& r = target.rotation;
            const double norm = double(r.m_Vec.x) * r.m_Vec.x + double(r.m_Vec.y) * r.m_Vec.y +
                                double(r.m_Vec.z) * r.m_Vec.z + double(r.m_Spin) * r.m_Spin;
            return (!target.object || (target.object.generation && target.object.slot < ClientWorld::object_limit)) &&
                   physics_vector(target.position) && std::isfinite(norm) && std::abs(norm - 1) < .01;
        };
        if (next.world != frame_.world || !next.world || !next.sequence || next.sequence <= frame_.sequence ||
            !next.player.object || !valid(next.player) || !valid(next.eye) || !valid(next.orb) ||
            !std::isfinite(next.time) || next.time < 0 || !std::isfinite(next.eye_level) ||
            std::abs(next.eye_level) > 1e7f || next.cameras.size() > 4096)
            error = "Invalid/stale client camera frame";
        if (!error.empty())
            return false;
        std::set<std::uint32_t> slots;
        for (const auto& camera : next.cameras)
            if (!camera.object || !valid(camera) || camera.settings.type > 2 ||
                !slots.insert(camera.object.slot).second)
                error = "Invalid/duplicate client camera descriptor";
        if (!error.empty())
            return false;
        frame_ = std::move(next);
        return true;
    }
    bool ClientCamera::can_toggle(bool in_world) const
    {
        return in_world && frame_.player.object && frame_.alive && !xr_view && !death.active && !view.object;
    }
    bool ClientCamera::select(const ClientCollisionWorld& collision, float pitch, float yaw, bool zoom,
                              std::string& error)
    {
        error.clear();
        if (!frame_.sequence || frame_.world != collision.frame().world || !collision.get(frame_.player.object) ||
            !std::isfinite(pitch) || !std::isfinite(yaw))
        {
            error = "Client camera owner/scene unavailable";
            return false;
        }
        const auto live = [&](ObjectId id) { return !id || collision.get(id); };
        if (!live(frame_.eye.object) || !live(frame_.orb.object) ||
            std::any_of(frame_.cameras.begin(), frame_.cameras.end(), [&](const auto& c) { return !live(c.object); }))
        {
            error = "Stale client camera target";
            return false;
        }
        third_person_active = false;
        if (xr_view)
            third_person = false;
        view = {};
        kind = ClientCameraKind::FirstPerson;
        for (const auto& camera : frame_.cameras)
            if (camera.active)
            {
                static_cast<ClientCameraTarget&>(view) = camera;
                view.settings = camera.settings;
                kind = ClientCameraKind::Scripted;
                break;
            }
        const auto* remote = frame_.alive && frame_.eye.object   ? &frame_.eye
                             : frame_.alive && frame_.orb.object ? &frame_.orb
                                                                 : nullptr;
        if (remote)
        {
            static_cast<ClientCameraTarget&>(view) = *remote;
            view.settings = {0, false, false, true};
            view.rotation = rotation_from_euler({pitch, yaw, 0});
            kind = remote == &frame_.eye ? ClientCameraKind::Eye : ClientCameraKind::Orb;
        }
        player_visible = bool(view.object) && !view.settings.hide_player && !death.active;
        weapons_visible = !view.object && !death.active;
        if (!view.object)
        {
            view.position = camera_position;
            view.rotation.m_Vec = {};
            const float cp = std::cos(pitch);
            const DVector forward{std::sin(yaw) * cp, -std::sin(pitch), std::cos(yaw) * cp};
            if (!align_rotation(forward, view.rotation))
                return false;
            if (death.active)
            {
                if (!death.Update(collision, {frame_.world, frame_.player.object}, frame_.player.position,
                                  frame_.player.rotation, frame_.time, camera_position))
                {
                    error = death.error;
                    return false;
                }
                view.position = death.position;
                view.rotation = death.rotation;
                kind = ClientCameraKind::Death;
            }
            else if (third_person && !xr_view && !zoom && frame_.alive)
            {
                if (!chase(collision, error))
                    return false;
                player_visible = third_person_active;
                weapons_visible = !third_person_active;
                if (third_person_active)
                    kind = ClientCameraKind::Chase;
            }
        }
        listener_position = view.object && view.settings.listener ? view.position : position;
        return true;
    }
    bool ClientCamera::chase(const ClientCollisionWorld& collision, std::string& error)
    {
        using namespace OpenBlood2;
        const auto forward = rotate_vector(view.rotation, {0, 0, 1});
        const auto boom = rotate_vector(view.rotation, {0, 20, -150});
        const auto requested = vector_add(camera_position, boom);
        // Center and four near-plane corners share the shortest travel. This bounded
        // clearance envelope also retracts for moving brushes, without moving the player.
        float fraction = 1;
        for (const auto offset : std::array<DVector, 5>{{{}, {-4, -4, 0}, {4, -4, 0}, {-4, 4, 0}, {4, 4, 0}}})
        {
            const auto clearance = rotate_vector(view.rotation, offset);
            ClientSegmentHit hit;
            const auto result = collision.intersect(
                vector_add(camera_position, clearance), vector_add(requested, clearance),
                ClientObjectId{frame_.world, frame_.player.object}, hit, SegmentObjects::SolidOnly, true);
            if (result == SegmentResult::Invalid || result == SegmentResult::Unavailable)
            {
                error = "Third-person camera obstruction query failed";
                return false;
            }
            if (result == SegmentResult::Hit)
                fraction =
                    std::min(fraction, std::max(0.f, float(hit.fraction) - 2.f / std::sqrt(vector_dot(boom, boom))));
        }
        // In a tight corner return to the eye rather than drawing inside the head.
        // Keep the selection so the chase view returns when there is room again.
        if (fraction * std::sqrt(vector_dot(boom, boom)) < 20)
            return true;
        third_person_active = true;
        view.position = vector_add(camera_position, vector_scale(boom, fraction));
        // Use the same eye ray/filter as player FireSpread, without spread or random
        // draws. Only the view turns toward this target: gameplay still fires from the eye.
        auto aim = vector_add(camera_position, vector_scale(forward, 10000));
        ClientSegmentHit hit;
        const auto result =
            collision.intersect(camera_position, aim, ClientObjectId{frame_.world, frame_.player.object}, hit,
                                SegmentObjects::SolidOrRayHit, true, SegmentStart::SkipInterior);
        if (result == SegmentResult::Invalid || result == SegmentResult::Unavailable)
        {
            error = "Third-person camera aim query failed";
            return false;
        }
        if (result == SegmentResult::Hit)
            aim = hit.point;
        const auto direction = vector_subtract(aim, view.position);
        return vector_dot(direction, direction) < 1e-12f || align_rotation(direction, view.rotation);
    }

} // namespace OpenBlood2
