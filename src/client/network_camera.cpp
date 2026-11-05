#include "client/network_camera.h"
#include <cmath>
#include <limits>

namespace OpenBlood2
{
    bool NetworkCamera::begin(const Net::Joined& owner)
    {
        if (!owner.world || !owner.player || !owner.object || !owner.object.generation || owner.object.slot >= 65536)
            return false;
        ClientCamera next;
        next.xr_view = camera_.xr_view;
        if (!next.begin(owner.world))
            return false;
        camera_ = std::move(next);
        owner_ = owner;
        state_.reset();
        sequence_ = 0;
        time_ = wonky_until_ = 0;
        wonky_no_move_ = dead_no_hide_ = false;
        return true;
    }
    NetworkCamera::Reception NetworkCamera::receive(const Net::PrivateEvent& event, std::string& error)
    {
        error.clear();
        if (!owner_.world || !Net::valid_private(event))
        {
            error = "Invalid network camera event";
            return Reception::Refused;
        }
        if (event.owner != owner_ || event.sequence <= sequence_)
            return Reception::Ignored;
        if (const auto* state = std::get_if<Net::CameraState>(&event.value))
        {
            if (state_ && state->force_revision < state_->force_revision)
                return Reception::Ignored;
            // PlayerDead remains the only trigger for the original death orbit.
            // Damage-dead/slow-death state alone must not start it early.
            if (state->alive && (!state_ || !state_->alive || state->force_revision != state_->force_revision))
            {
                camera_.death = {};
                dead_no_hide_ = false;
            }
            state_ = *state;
        }
        else if (const auto* wonky = std::get_if<WonkyVision>(&event.value))
        {
            const double now = double(event.tick) / 60;
            const float seconds = camera_.death.active ? 0 : wonky->seconds;
            if (!seconds)
            {
                wonky_until_ = 0;
                wonky_no_move_ = false;
            }
            else if (now + seconds > wonky_until_)
            {
                wonky_until_ = now + seconds;
                wonky_no_move_ = wonky->no_move;
            }
        }
        else if (std::holds_alternative<PlayerDead>(event.value))
        {
            if (!camera_.death.active)
                dead_no_hide_ = wonky_no_move_ && double(event.tick) / 60 < wonky_until_;
            wonky_until_ = 0;
            wonky_no_move_ = false;
            camera_.death.Begin(static_cast<float>(double(event.tick) / 60));
        }
        else
            return Reception::Ignored;
        sequence_ = event.sequence;
        return Reception::Applied;
    }
    NetworkCamera::Reception NetworkCamera::update(const ClientWorld& world, const ClientCollisionWorld& collision,
                                                   const ClientMovement& movement, std::uint64_t force_revision,
                                                   float pitch, float yaw, bool zoom, double time, std::string& error)
    {
        error.clear();
        if (!std::isfinite(time) || time < 0 || time > std::numeric_limits<float>::max() || !std::isfinite(pitch) ||
            std::abs(pitch) > 1e6f || !std::isfinite(yaw) || std::abs(yaw) > 1e6f ||
            camera_.frame().sequence == UINT64_MAX || time < time_)
        {
            error = "Invalid network camera clock or aim";
            return Reception::Refused;
        }
        if (!state_ || !owner_.world || world.frame().world != owner_.world ||
            collision.frame().world != owner_.world || movement.owner != ClientObjectId{owner_.world, owner_.object} ||
            force_revision != state_->force_revision)
            return Reception::Deferred;
        // A deliberately delayed presentation clock may not have reached the
        // reliable death event yet. Keep the last view until that source time.
        if (camera_.death.active && static_cast<float>(time) < camera_.death.start)
            return Reception::Deferred;
        const auto* player = world.get(owner_.object);
        if (!player || !collision.get(owner_.object))
            return Reception::Deferred;
        ClientCameraFrame frame;
        frame.world = owner_.world;
        frame.sequence = camera_.frame().sequence + 1;
        frame.time = static_cast<float>(time);
        frame.player = {owner_.object, movement.body.position, player->pose.rotation};
        frame.eye_level = state_->eye_level;
        frame.alive = state_->alive;
        const auto target = [&](ObjectId id, ClientCameraTarget& out)
        {
            if (!id)
                return true;
            const auto* object = world.get(id);
            if (!object || !collision.get(id))
                return false;
            out = {id, object->pose.position, object->pose.rotation};
            return true;
        };
        if (!target(state_->eye, frame.eye) || !target(state_->orb, frame.orb))
            return Reception::Deferred;
        if (state_->scripted)
        {
            ClientCameraDescriptor descriptor;
            if (!target(state_->scripted, descriptor))
                return Reception::Deferred;
            descriptor.settings = state_->settings;
            descriptor.active = true;
            frame.cameras.push_back(descriptor);
        }
        auto next = camera_;
        next.position = movement.body.position;
        next.camera_position = movement.body.position;
        next.camera_position.y += state_->eye_level;
        if (!next.receive(std::move(frame), error) || !next.select(collision, pitch, yaw, zoom, error))
            return Reception::Refused;
        if (next.kind == ClientCameraKind::Death && dead_no_hide_)
            next.player_visible = true;
        camera_ = std::move(next);
        time_ = time;
        return Reception::Applied;
    }
} // namespace OpenBlood2
