#include "game/local_presentation.h"
#include "game/original/OrbProjectile.h"
#include "game/original/SeeingEye.h"
#include "game/server_adapter.h"
#include <cmath>
namespace OpenBlood2
{
    ClientObjectId LocalPresentation::owner(HCLIENT connection) const
    {
        const auto* record = objects_.get(server_.GetClientUserData(connection));
        if (!record || record->life != Life::Alive || !dynamic_cast<const CPlayerObj*>(record->game.get()))
            return {};
        return {client_.world.frame().world, record->id};
    }
    void LocalPresentation::sync_clock()
    {
        client_.time = server_.GetTime();
    }
    bool LocalPresentation::bind(HCLIENT connection)
    {
        sync_clock();
        return client_.hud.bind(owner(connection));
    }
    bool LocalPresentation::publish(HCLIENT connection, HOBJECT eye, HOBJECT orb)
    {
        error.clear();
        sync_clock();
        const auto identity = owner(connection);
        const auto* record = objects_.get(identity.object);
        if (!identity.world || !record || objects_.busy() || !objects_.refresh_attachments() ||
            !movement_.publish_collision())
        {
            error = "Client view publication requires a stable associated world";
            return false;
        }
        const auto* player = dynamic_cast<const CPlayerObj*>(record->game.get());
        const auto target = [](const ObjectRecord& r)
        { return ClientCameraTarget{r.id, r.state.m_Pos, r.state.m_Rotation}; };
        ClientCameraFrame frame;
        frame.world = identity.world;
        frame.sequence = client_.camera.frame().sequence + 1;
        frame.player = target(*record);
        frame.eye_level = player->eye_level;
        frame.time = client_.time;
        frame.alive = !player->m_damage.IsDead();
        eye_ = orb_ = nullptr;
        if (const auto* r = objects_.get(eye); r && r->life == Life::Alive)
            if (const auto* e = dynamic_cast<const SeeingEye*>(r->game.get()); e && e->m_hFiredFrom == record->handle)
            {
                frame.eye = target(*r);
                eye_ = eye;
            }
        if (const auto* r = objects_.get(orb); r && r->life == Life::Alive)
            if (const auto* o = dynamic_cast<const COrbAltProjectile*>(r->game.get());
                o && o->m_hFiredFrom == record->handle)
            {
                frame.orb = target(*r);
                orb_ = orb;
            }
        for (const auto& effect : server_.camera_effects)
            if (const auto* r = objects_.get(effect.object); r && r->life == Life::Alive)
            {
                ClientCameraDescriptor camera;
                static_cast<ClientCameraTarget&>(camera) = target(*r);
                camera.settings = effect.settings;
                camera.active = r->user_flags & 1;
                frame.cameras.push_back(camera);
            }
        return client_.camera.receive(std::move(frame), error);
    }
    bool LocalPresentation::commit(HCLIENT connection, const LocalCameraCommand& command)
    {
        const auto identity = owner(connection);
        const auto& frame = client_.camera.frame();
        if (!identity.world || command.owner != identity || frame.world != identity.world ||
            frame.player.object != identity.object || command.sequence != frame.sequence || !command.sequence ||
            (committed_world_ == identity.world && command.sequence <= committed_sequence_) ||
            !std::isfinite(command.aim.pitch) || !std::isfinite(command.aim.yaw) ||
            std::abs(command.aim.pitch) > 1e6f || std::abs(command.aim.yaw) > 1e6f)
        {
            ++rejected_commands;
            return false;
        }
        auto* record = objects_.get(identity.object);
        auto* player = dynamic_cast<CPlayerObj*>(record->game.get());
        auto selected = server_.GetCameraView();
        HOBJECT remote = nullptr;
        if (!player->m_damage.IsDead())
        {
            if (const auto* r = objects_.get(eye_); r && r->life == Life::Alive)
                if (const auto* e = dynamic_cast<const SeeingEye*>(r->game.get());
                    e && e->m_hFiredFrom == record->handle)
                    remote = eye_;
            if (!remote)
                if (const auto* r = objects_.get(orb_); r && r->life == Life::Alive)
                    if (const auto* o = dynamic_cast<const COrbAltProjectile*>(r->game.get());
                        o && o->m_hFiredFrom == record->handle)
                        remote = orb_;
        }
        // Permissions are derived again on the server, not accepted from the rendered view.
        const auto* selected_record = objects_.get(remote ? remote : selected.object);
        ObjectId expected;
        for (const auto& camera : frame.cameras)
            if (camera.active)
            {
                expected = camera.object;
                break;
            }
        if (frame.alive && frame.eye.object)
            expected = frame.eye.object;
        else if (frame.alive && frame.orb.object)
            expected = frame.orb.object;
        if ((selected_record ? selected_record->id : ObjectId{}) != expected)
        {
            ++rejected_commands;
            return false;
        }
        if (remote && remote == eye_)
        {
            const auto rotation = rotation_from_euler({command.aim.pitch, command.aim.yaw, 0});
            if (server_.SetObjectRotation(remote, &rotation) != DE_OK)
            {
                ++rejected_commands;
                return false;
            }
        }
        player->camera_movement_blocked = remote || (selected.object && !selected.settings.player_movement);
        committed_world_ = identity.world;
        committed_sequence_ = command.sequence;
        ++accepted_commands;
        return true;
    }
    bool LocalPresentation::deliver(HCLIENT connection, const PlayerStats& value)
    {
        return client_.hud.receive(owner(connection), client_.hud.sequence() + 1, value);
    }
    bool LocalPresentation::deliver(HCLIENT connection, const InventoryItems& value)
    {
        return client_.hud.receive(owner(connection), client_.hud.sequence() + 1, value);
    }
} // namespace OpenBlood2
