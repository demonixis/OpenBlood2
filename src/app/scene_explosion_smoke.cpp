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
        const auto& fragments = g.server.explosion_fragments;
        if (!report.fragment_event || !report.fragment_error.empty() || fragments.active.size() < 2 ||
            fragments.active.size() > 5 || fragments.failed || !fragments.update_error.empty())
            return false;
        for (const auto& fx : fragments.active)
            if (!fx.model || !fx.updates || !fx.trail_error.empty() || !fx.error.empty() ||
                (fx.position.x == fx.settings.position.x && fx.position.y == fx.settings.position.y &&
                 fx.position.z == fx.settings.position.z))
                return false;
        if (!fragments.trail_started || fragments.trail_failed || !fragments.trail_particles())
            return false;
        const auto& rings = g.server.explosion_rings;
        if (!report.ring_event || !report.ring_error.empty() || rings.active.size() != 4 || rings.started != 4 ||
            rings.failed || rings.reserved_particles() != 128 || !rings.update_error.empty())
            return false;
        if (rings.active[0].particles.size() != 32 || !rings.active[0].updates || rings.active[0].alpha >= 1 ||
            rings.active[0].particles[0].position.x <= 10)
            return false;
        const auto& lights = g.server.explosion_lights;
        if (!report.light_event || !report.light_error.empty() || lights.active.size() != 2 || lights.started != 2 ||
            lights.failed || lights.reports.size() != 1 || !lights.update_error.empty())
            return false;
        for (const auto& fx : lights.active)
            if (fx.event != report.light_event || !fx.updates || fx.radius >= fx.settings.radius1 || fx.radius <= 0 ||
                fx.color.y <= fx.settings.color1.y || fx.delay || !fx.error.empty())
                return false;
        const auto& models = g.server.explosion_models;
        if (!report.model_event || !report.model_error.empty() || models.active.size() != 1 || models.started != 1 ||
            models.failed || models.reports.size() != 1 || !models.active[0].updates ||
            models.active[0].alpha >= .75f || !models.active[0].error.empty())
            return false;
        const auto& sprites = g.server.explosion_sprites;
        if (!report.sprite_event || !report.sprite_error.empty() || sprites.active.size() != 2 ||
            sprites.started != 2 || sprites.failed || sprites.reports.size() != 1)
            return false;
        for (const auto& fx : sprites.active)
            if (fx.event != report.sprite_event || !fx.sprite || !fx.updates || fx.track.milliseconds <= 0 ||
                fx.alpha >= fx.settings.alpha || !fx.error.empty())
                return false;
        const auto* v = g.server.audio.get(report.voice);
        return report.targets.null_damager && !report.targets.cancelled && report.targets.delivered == 0 &&
               !report.client_fx_available && report.sound_error.empty() && v && v->world_owned && v->auto_remove &&
               v->cursor > 0 && v->clip->duration() > 0 && g.server.audio.has_output() &&
               g.server.audio.submitted_frames > 0 && g.server.audio.output_error.empty();
    }
} // namespace OpenBlood2
