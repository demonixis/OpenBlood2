#include "scene_explosion_smoke.h"
#include "game/original/ClientExplosionSFX.h"
namespace OpenBlood2
{
    bool prepare_scene_explosion_smoke(GameSession& g, ObjectId& selected)
    {
        unsigned count = 0;
        for (auto id : g.objects.snapshot())
            if (dynamic_cast<CClientExplosionSFX*>(g.objects.get(id)->game.get()))
            {
                if (!count)
                    selected = id;
                ++count;
            }
        return count == 9;
    }
    bool activate_scene_explosion_smoke(GameSession& g, ObjectId selected)
    {
        const auto* r = g.objects.get(selected);
        if (!r)
            return false;
        auto s = g.server.CreateString("TRIGGER");
        auto m = g.server.StartMessageToObject(nullptr, r->handle, 1000);
        const bool ok = m && g.server.WriteToMessageHString(m, s) == DE_OK && g.server.EndMessage(m) == DE_OK;
        g.server.FreeString(s);
        return ok;
    }
    bool check_scene_explosion_smoke(GameSession& g, ObjectId selected)
    {
        if (g.objects.get(selected) || g.server.scene_explosions.size() != 1)
            return false;
        const auto& report = g.server.scene_explosions.back();
        const auto* v = g.server.audio.get(report.voice);
        return report.targets.null_damager && !report.targets.cancelled && report.targets.delivered == 0 &&
               !report.client_fx_available && report.sound_error.empty() && v && v->world_owned && v->auto_remove &&
               v->cursor > 0 && v->clip->duration() > 0 && g.server.audio.has_output() &&
               g.server.audio.submitted_frames > 0 && g.server.audio.output_error.empty();
    }
} // namespace OpenBlood2
