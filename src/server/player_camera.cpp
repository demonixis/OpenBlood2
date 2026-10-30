#include "game/original/OrbProjectile.h"
#include "game/original/SeeingEye.h"
#include "server/session.h"

namespace OpenBlood2
{
    bool ServerSession::refresh_camera(Player& peer, Net::CameraState& out, std::string& error)
    {
        auto* player = game_.server_shell.Player(peer.client);
        const auto* owner = game_.objects.get(peer.object);
        if (!player || !owner || owner->life != Life::Alive)
        {
            error = "Camera owner is unavailable";
            return false;
        }
        const auto selected = game_.server.GetCameraView();
        if (const auto* camera = game_.objects.get(selected.object); camera && camera->life == Life::Alive)
        {
            out.scripted = camera->id;
            out.settings = selected.settings;
        }
        if (const auto* record = game_.objects.get(peer.camera_eye); record && record->life == Life::Alive)
            if (const auto* eye = dynamic_cast<const SeeingEye*>(record->game.get());
                eye && eye->m_hFiredFrom == owner->handle)
                out.eye = record->id;
        if (const auto* record = game_.objects.get(peer.camera_orb); record && record->life == Life::Alive)
            if (const auto* orb = dynamic_cast<const COrbAltProjectile*>(record->game.get());
                orb && orb->m_hFiredFrom == owner->handle)
                out.orb = record->id;
        peer.camera_eye = out.eye;
        peer.camera_orb = out.orb;
        out.alive = !player->m_damage.IsDead();
        out.eye_level = player->eye_level;
        // Original LocalPresentation::commit permissions, derived exclusively
        // from checked server objects. A peer cannot nominate its own camera.
        player->camera_movement_blocked =
            (out.alive && (out.eye || out.orb)) || (out.scripted && !out.settings.player_movement);
        return true;
    }
} // namespace OpenBlood2
