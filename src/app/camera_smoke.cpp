#include "camera_smoke.h"
#include "game/original/CameraObj.h"
namespace OpenBlood2
{
    bool finish_camera_intro_for_smoke(GameSession& game)
    {
        // Diagnostic gameplay scenarios begin after the authored camera expires.
        // Advance original callbacks, retaining the actual gate and timer.
        for (int i = 0; i < 1800 && game.client_shell.view.object; ++i)
            if (!game.step(1.0 / 60))
                return false;
        const auto* player = game.server_shell.Player(game.local_client);
        return !game.client_shell.view.object && player && !player->camera_movement_blocked;
    }
    bool prepare_camera_smoke(GameSession& g, ObjectId& selected)
    {
        for (auto id : g.objects.snapshot())
            if (auto* r = g.objects.get(id); r)
                if (auto* c = dynamic_cast<CameraObj*>(r->game.get()))
                {
                    selected = id;
                    c->SetActive(DTRUE);
                    return c->SetLinkObject(g.client_shell.player);
                }
        return false;
    }
    bool check_camera_smoke(GameSession& g, ObjectId selected)
    {
        const auto* r = g.objects.get(selected);
        const auto* c = r ? dynamic_cast<const CameraObj*>(r->game.get()) : nullptr;
        const auto* player = g.server_shell.Player(g.local_client);
        if (!c || !player || !c->tracking_updates || !c->tracking_error.empty() ||
            g.client_shell.view.object != r->handle || !player->camera_movement_blocked)
            return false;
        const auto delta = vector_subtract(g.client_shell.view.position, r->state.m_Pos);
        return vector_dot(delta, delta) < .0001f && !g.client_shell.weapons_visible && !c->published.listener;
    }
} // namespace OpenBlood2
