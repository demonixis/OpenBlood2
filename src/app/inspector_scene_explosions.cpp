#include "game/original/ClientExplosionSFX.h"
#include "inspector.h"
#include <imgui.h>
namespace OpenBlood2
{
    void Inspector::draw_scene_explosions(GameSession& game, ObjectSelection& selection)
    {
        ImGui::TextWrapped(
            "Original CClientExplosionSFX emitters. The original null damager suppresses all damage. "
            "Spatial WAV playback and selected original sprite/model components are implemented. Composite "
            "surface illumination remains unavailable. Fragment/ring/flame/light state "
            "is shown "
            "below.");
        ImGui::Text("Audio: %s | active voices %zu | submitted frames %llu",
                    game.server.audio.has_output() ? "SDL stream" : "offline clock", game.server.audio.voices().size(),
                    (unsigned long long)game.server.audio.submitted_frames);
        if (!game.server.audio.output_error.empty())
            ImGui::TextWrapped("%s", game.server.audio.output_error.c_str());
        if (ImGui::CollapsingHeader("Authored sources / failures / removed instances"))
            for (size_t i = 0; i < game.entries.size(); ++i)
                if (game.world.objects[i].class_name == "CClientExplosionSFX")
                {
                    const auto& e = game.entries[i];
                    const auto* r = game.objects.get(e.object);
                    ImGui::Text("Source %zu: %s / %s", i, entry_state_name(e.state),
                                r ? life_name(r->life) : "no live instance");
                    if (!e.detail.empty())
                        ImGui::TextWrapped("%s", e.detail.c_str());
                }
        for (auto id : game.objects.snapshot())
        {
            const auto* r = game.objects.get(id);
            const auto* e = dynamic_cast<const CClientExplosionSFX*>(r->game.get());
            if (!e)
                continue;
            ImGui::PushID(int(id.slot));
            const auto label = std::string(r->state.m_Name) + " #" + std::to_string(id.slot);
            if (ImGui::Selectable(label.c_str(), scene_explosion_selection == id))
                scene_explosion_selection = selection.id = id;
            if (scene_explosion_selection == id)
            {
                ImGui::Text("%s / %s / %s | scheduled %.3f s", life_name(r->life),
                            r->activity == OBJSTATE_ACTIVE ? "active" : "inactive",
                            r->state.m_Flags & FLAG_VISIBLE ? "visible" : "hidden",
                            r->scheduled ? r->due - game.objects.time() : 0);
                ImGui::Text("Initial position %.1f %.1f %.1f | direction %.1f %.1f %.1f", e->position.x, e->position.y,
                            e->position.z, e->normal.x, e->normal.y, e->normal.z);
                ImGui::Text("Type %u | configured damage %.1f / radius %.1f | delivered %u", e->type, e->damage,
                            e->radius, e->targets.delivered);
                ImGui::Text("Commands %llu / activations %llu | triggered %d", (unsigned long long)e->commands,
                            (unsigned long long)e->activations, e->triggered);
                ImGui::TextWrapped("Sound: %s [%s]", e->sound.empty() ? "none" : e->sound.c_str(),
                                   game.resources.find(e->sound) ? "indexed" : "not indexed");
                if (r->life == Life::Alive && ImGui::Button("TRIGGER"))
                    scene_explosion_request = id;
            }
            ImGui::PopID();
        }
        ImGui::Separator();
        const auto& fragments = game.server.explosion_fragments;
        ImGui::Text("Explosion fragments: %zu active / %llu started / %llu expired / %llu failed",
                    fragments.active.size(), (unsigned long long)fragments.started,
                    (unsigned long long)fragments.expired, (unsigned long long)fragments.failed);
        if (!fragments.update_error.empty())
            ImGui::TextWrapped("%s", fragments.update_error.c_str());
        for (const auto& fx : fragments.active)
        {
            ImGui::Text("Fragment %llu / event %llu: elapsed %.3f, alpha %.3f, moving %s, impacts %llu",
                        (unsigned long long)fx.id, (unsigned long long)fx.event, fx.last_time - fx.start_time, fx.alpha,
                        fx.moving ? "yes" : "resting", (unsigned long long)fx.impacts);
            ImGui::Text("Position %.2f %.2f %.2f / velocity per update %.2f %.2f %.2f", fx.position.x, fx.position.y,
                        fx.position.z, fx.velocity.x, fx.velocity.y, fx.velocity.z);
            ImGui::Text("Normal %.2f %.2f %.2f / last query %d / polygon %u / hit object %s", fx.last_normal.x,
                        fx.last_normal.y, fx.last_normal.z, int(fx.last_query), fx.last_hit.polygon,
                        fx.last_hit.object ? (game.objects.get(fx.last_hit.object) ? "live" : "removed")
                                           : "none/world");
            ImGui::TextWrapped("%s / %s", fx.model_path.c_str(), fx.skin_path.c_str());
            if (!fx.trail_error.empty())
                ImGui::TextWrapped("%s", fx.trail_error.c_str());
            if (explosion_model_view)
                for (const auto& e : explosion_model_view->entries)
                    if (e.fragment_effect == fx.id)
                    {
                        ImGui::Text("%zu triangles / %s", e.triangles,
                                    e.range == UINT32_MAX ? "not prepared" : "prepared");
                        if (!e.error.empty())
                            ImGui::TextWrapped("%s", e.error.c_str());
                    }
        }
        if (ImGui::CollapsingHeader("Last 64 fragment deliveries / failures"))
            for (const auto& r : fragments.reports)
            {
                ImGui::Text("Event %llu / type %u / %zu fragments", (unsigned long long)r.id, r.type,
                            r.fragments.size());
                if (!r.error.empty())
                    ImGui::TextWrapped("%s", r.error.c_str());
                if (!r.unavailable.empty())
                    ImGui::TextWrapped("%s", r.unavailable.c_str());
            }
        ImGui::Separator();
        ImGui::Text("Fragment smoke trails: %zu systems / %zu particles / %llu started / %llu expired / %llu failed",
                    fragments.trails.size(), fragments.trail_particles(), (unsigned long long)fragments.trail_started,
                    (unsigned long long)fragments.trail_expired, (unsigned long long)fragments.trail_failed);
        for (const auto& t : fragments.trails)
        {
            ImGui::Text("Trail %llu / fragment %llu / %s / %zu particles / %llu emitted / deadline %.3f",
                        (unsigned long long)t.id, (unsigned long long)t.parent, t.moving ? "emitting" : "stopped",
                        t.particles.size(), (unsigned long long)t.emitted, t.duration);
            ImGui::Text("Origin %.2f %.2f %.2f / source %.2f %.2f %.2f / elapsed %.3f", t.origin.x, t.origin.y,
                        t.origin.z, t.position.x, t.position.y, t.position.z, t.last_time - t.start_time);
            ImGui::TextWrapped("%s | radius/640 %.3f | alpha %.2f (authored %.2f, unused without ramps)",
                               t.texture.c_str(), t.radius / 640.f, t.alpha, t.requested_alpha);
            unsigned prepared = 0;
            if (sprite_view)
                for (const auto& e : sprite_view->entries)
                    if (e.trail_system == t.id)
                    {
                        prepared += e.range != UINT32_MAX && e.error.empty();
                        if (!e.error.empty())
                            ImGui::TextWrapped("%s", e.error.c_str());
                    }
            ImGui::Text("Prepared quads %u", prepared);
            ImGui::PushID((int)t.id);
            if (ImGui::TreeNode("Smoke particles"))
            {
                for (const auto& p : t.particles)
                    ImGui::Text("Local %.2f %.2f %.2f / velocity %.2f %.2f %.2f / life %.3f / gray %.3f", p.position.x,
                                p.position.y, p.position.z, p.velocity.x, p.velocity.y, p.velocity.z, p.lifetime,
                                p.color.x);
                ImGui::TreePop();
            }
            ImGui::PopID();
        }
        const auto& flames = game.server.explosion_flames;
        ImGui::Text("Explosion flames: %zu systems / %zu particles / %llu started / %llu expired / %llu failed",
                    flames.active.size(), flames.live_particles(), (unsigned long long)flames.started,
                    (unsigned long long)flames.expired, (unsigned long long)flames.failed);
        if (!flames.update_error.empty())
            ImGui::TextWrapped("%s", flames.update_error.c_str());
        for (const auto& fx : flames.active)
        {
            ImGui::Text("Flame %llu / event %llu: elapsed %.3f / next burst %.3f / alpha %.3f",
                        (unsigned long long)fx.id, (unsigned long long)fx.event, fx.last_time - fx.start_time,
                        fx.add_time, fx.alpha);
            ImGui::Text("%zu live / %llu bursts / %llu emitted / origin %.1f %.1f %.1f", fx.particles.size(),
                        (unsigned long long)fx.bursts, (unsigned long long)fx.emitted, fx.settings.position.x,
                        fx.settings.position.y, fx.settings.position.z);
            ImGui::TextWrapped("%s | half-size %.3f | gravity %.2f | stored, unused ramps %.2f / %.2f, flags %u",
                               fx.texture.c_str(), fx.settings.radius / 640.f, fx.settings.gravity, fx.settings.ramp_up,
                               fx.settings.ramp_down, fx.settings.ramp_flags);
            unsigned prepared = 0;
            if (sprite_view)
                for (const auto& e : sprite_view->entries)
                    if (e.flame_system == fx.id)
                    {
                        prepared += e.range != UINT32_MAX && e.error.empty();
                        if (!e.error.empty())
                            ImGui::TextWrapped("%s", e.error.c_str());
                    }
            ImGui::Text("Prepared quads %u", prepared);
            ImGui::PushID((int)fx.id);
            if (ImGui::TreeNode("Flame particles"))
            {
                for (const auto& p : fx.particles)
                    ImGui::Text("Local %.2f %.2f %.2f / velocity %.2f %.2f %.2f / life %.3f / RGB %.3f %.3f %.3f",
                                p.position.x, p.position.y, p.position.z, p.velocity.x, p.velocity.y, p.velocity.z,
                                p.lifetime, p.color.x, p.color.y, p.color.z);
                ImGui::TreePop();
            }
            ImGui::PopID();
        }
        if (ImGui::CollapsingHeader("Last 64 flame deliveries / failures"))
            for (const auto& r : flames.reports)
                ImGui::TextWrapped("Event %llu / type %u / %zu flames / %s", (unsigned long long)r.id, r.type,
                                   r.flames.size(), r.error.empty() ? "accepted" : r.error.c_str());
        const auto& rings = game.server.explosion_rings;
        ImGui::Text("Explosion rings: %zu active / %zu reserved particles / %llu started / %llu expired / %llu failed",
                    rings.active.size(), rings.reserved_particles(), (unsigned long long)rings.started,
                    (unsigned long long)rings.expired, (unsigned long long)rings.failed);
        if (!rings.update_error.empty())
            ImGui::TextWrapped("%s", rings.update_error.c_str());
        for (const auto& fx : rings.active)
        {
            ImGui::Text("Ring %llu / event %llu: %zu particles, alpha %.3f / pending delay %.3f / elapsed %.3f",
                        (unsigned long long)fx.id, (unsigned long long)fx.event, fx.particles.size(), fx.alpha,
                        fx.delay, fx.last_time - fx.start_time);
            ImGui::Text("Origin %.1f %.1f %.1f / half-size %.3f / gravity %.2f", fx.settings.position.x,
                        fx.settings.position.y, fx.settings.position.z, fx.settings.radius / 640.f,
                        fx.settings.gravity);
            ImGui::TextWrapped("%s", fx.texture.c_str());
            if (sprite_view)
            {
                std::size_t prepared = 0, failed = 0;
                for (const auto& e : sprite_view->entries)
                    if (e.particle_system == fx.id)
                    {
                        prepared += e.range != UINT32_MAX;
                        if (!e.error.empty())
                        {
                            if (!failed)
                                ImGui::TextWrapped("%s", e.error.c_str());
                            ++failed;
                        }
                    }
                ImGui::Text("%zu particle quads prepared / %zu resource or pose failures", prepared, failed);
            }
            ImGui::PushID(static_cast<int>(fx.id));
            if (ImGui::TreeNode("Local particle positions / velocities / remaining life"))
            {
                for (unsigned i = 0; i < fx.particles.size(); ++i)
                {
                    const auto& v = fx.particles[i];
                    ImGui::Text("%u: p %.2f %.2f %.2f / v %.2f %.2f %.2f / %.3f s", i, v.position.x, v.position.y,
                                v.position.z, v.velocity.x, v.velocity.y, v.velocity.z, v.lifetime);
                }
                ImGui::TreePop();
            }
            ImGui::PopID();
        }
        if (ImGui::CollapsingHeader("Last 64 ring deliveries / failures"))
            for (const auto& r : rings.reports)
                ImGui::TextWrapped("Event %llu / type %u / %zu rings / %s", (unsigned long long)r.id, r.type,
                                   r.rings.size(), r.error.empty() ? "accepted" : r.error.c_str());
        ImGui::Separator();
        const auto& lights = game.server.explosion_lights;
        ImGui::Text("Explosion point lights: %zu active / %llu started / %llu expired / %llu failed",
                    lights.active.size(), (unsigned long long)lights.started, (unsigned long long)lights.expired,
                    (unsigned long long)lights.failed);
        ImGui::TextWrapped("Evaluated client light state only; no surface illumination, shadows or scorch decals.");
        if (!lights.update_error.empty())
            ImGui::TextWrapped("%s", lights.update_error.c_str());
        for (const auto& fx : lights.active)
        {
            ImGui::PushID(static_cast<int>(fx.id));
            ImGui::Text("Light %llu / event %llu: %.3f / %.3f s | waiting %d / delay %.3f | %llu updates",
                        (unsigned long long)fx.id, (unsigned long long)fx.event, fx.last_time - fx.start_time,
                        fx.settings.duration, fx.delay > 0, fx.delay, (unsigned long long)fx.updates);
            ImGui::Text("Position %.2f %.2f %.2f | radius %.3f (%.1f to %.1f)", fx.settings.position.x,
                        fx.settings.position.y, fx.settings.position.z, fx.radius, fx.settings.radius1,
                        fx.settings.radius2);
            ImGui::ColorButton("Current light color", ImVec4(fx.color.x, fx.color.y, fx.color.z, 1),
                               ImGuiColorEditFlags_NoTooltip, ImVec2(40, 16));
            ImGui::SameLine();
            ImGui::Text("RGB %.4f %.4f %.4f", fx.color.x, fx.color.y, fx.color.z);
            ImGui::PopID();
        }
        if (ImGui::CollapsingHeader("Last 64 light deliveries / failures"))
            for (const auto& r : lights.reports)
                ImGui::TextWrapped("Event %llu / type %u / %s / %zu lights: %s", (unsigned long long)r.id, r.type,
                                   r.source.c_str(), r.lights.size(),
                                   r.error.empty() ? "created; illumination unavailable" : r.error.c_str());
        ImGui::Separator();
        const auto& spells = game.server.object_particles;
        if (ImGui::TreeNode("Original shield / powerup particles"))
        {
            ImGui::Text("Active %zu | particles %zu | started %llu | expired %llu | failed %llu", spells.active.size(),
                        spells.particle_count(), (unsigned long long)spells.started, (unsigned long long)spells.expired,
                        (unsigned long long)spells.failed);
            for (const auto& instance : spells.active)
            {
                const auto& fx = instance.effect;
                if (!ImGui::TreeNode((void*)(uintptr_t)fx.id, "%llu / recipe %u", (unsigned long long)fx.id, fx.recipe))
                    continue;
                ImGui::Text("Owner %s | updates %llu | bursts %llu | live %zu / emitted %llu",
                            game.server.IsObjectAlive(instance.owner) ? "alive" : "removed",
                            (unsigned long long)fx.updates, (unsigned long long)fx.bursts, fx.particles.size(),
                            (unsigned long long)fx.emitted);
                ImGui::Text("Time %.3f / %.3f | alpha %.3f (authored %.3f) | ramp %s %.3f",
                            fx.last_time - fx.start_time, fx.duration, fx.alpha, fx.requested_alpha,
                            fx.ramping ? "down" : "none", fx.ramp_ratio);
                ImGui::Text("System %.2f %.2f %.2f | rotation %.3f %.3f %.3f %.3f", fx.position.x, fx.position.y,
                            fx.position.z, fx.system_rotation.m_Vec.x, fx.system_rotation.m_Vec.y,
                            fx.system_rotation.m_Vec.z, fx.system_rotation.m_Spin);
                ImGui::TextWrapped("%s", fx.texture.c_str());
                if (sprite_view)
                {
                    unsigned prepared = 0;
                    for (const auto& entry : sprite_view->entries)
                        if (entry.object_effect == fx.id)
                        {
                            prepared += entry.range != UINT32_MAX;
                            if (!entry.error.empty())
                                ImGui::TextWrapped("%s", entry.error.c_str());
                        }
                    ImGui::Text("Prepared particles: %u", prepared);
                }
                if (ImGui::TreeNode("Local particle state"))
                {
                    for (const auto& particle : fx.particles)
                        ImGui::Text("p %.2f %.2f %.2f | v %.2f %.2f %.2f | life %.3f | RGB %.2f %.2f %.2f",
                                    particle.position.x, particle.position.y, particle.position.z, particle.velocity.x,
                                    particle.velocity.y, particle.velocity.z, particle.lifetime, particle.color.x,
                                    particle.color.y, particle.color.z);
                    ImGui::TreePop();
                }
                ImGui::TreePop();
            }
            for (const auto& report : spells.reports)
                ImGui::TextWrapped("%llu recipe %u [%s]: %s", (unsigned long long)report.id, report.recipe,
                                   report.source.c_str(),
                                   report.error.empty() ? "created (historical)" : report.error.c_str());
            if (!spells.update_error.empty())
                ImGui::TextWrapped("%s", spells.update_error.c_str());
            ImGui::TreePop();
        }
        const auto& beams = game.server.laser_beams;
        if (ImGui::TreeNode("Original laser beams"))
        {
            ImGui::Text("Active %zu | started %llu | expired %llu | failed %llu", beams.active.size(),
                        (unsigned long long)beams.started, (unsigned long long)beams.expired,
                        (unsigned long long)beams.failed);
            for (const auto& beam : beams.active)
            {
                ImGui::Text("%llu: updates %llu | scale %.3f %.3f %.3f | alpha %.3f", (unsigned long long)beam.id,
                            (unsigned long long)beam.updates, beam.scale.x, beam.scale.y, beam.scale.z, beam.alpha);
                ImGui::TextWrapped("Models/Ammo/beam.abc | %s", beam.skin_path.c_str());
                if (explosion_model_view)
                    for (const auto& entry : explosion_model_view->entries)
                        if (entry.laser_effect == beam.id)
                        {
                            ImGui::Text("%zu triangles | %s", entry.triangles,
                                        entry.range == UINT32_MAX ? "not drawn" : "prepared");
                            if (!entry.error.empty())
                                ImGui::TextWrapped("%s", entry.error.c_str());
                        }
            }
            for (const auto& report : beams.reports)
                ImGui::TextWrapped("%llu [%s] %.1f %.1f %.1f -> %.1f %.1f %.1f | %s", (unsigned long long)report.id,
                                   report.source.c_str(), report.from.x, report.from.y, report.from.z, report.to.x,
                                   report.to.y, report.to.z,
                                   report.error.empty() ? "created (historical)" : report.error.c_str());
            if (!beams.update_error.empty())
                ImGui::TextWrapped("Update: %s", beams.update_error.c_str());
            ImGui::TreePop();
        }
        const auto& models = game.server.explosion_models;
        ImGui::Text("Client explosion models: %zu active / %llu started / %llu expired / %llu failed",
                    models.active.size(), (unsigned long long)models.started, (unsigned long long)models.expired,
                    (unsigned long long)models.failed);
        if (!models.update_error.empty())
            ImGui::TextWrapped("%s", models.update_error.c_str());
        for (const auto& fx : models.active)
        {
            ImGui::Text("Model FX %llu / event %llu: %.3f s, alpha %.3f, scale %.2f %.2f %.2f / wave %u",
                        (unsigned long long)fx.id, (unsigned long long)fx.event, fx.last_time - fx.start_time, fx.alpha,
                        fx.scale.x, fx.scale.y, fx.scale.z, fx.settings.waveform);
            ImGui::TextWrapped("%s | %s", fx.model_path.c_str(), fx.skin_path.c_str());
            if (explosion_model_view)
                for (const auto& entry : explosion_model_view->entries)
                    if (entry.client_effect == fx.id)
                    {
                        ImGui::Text("%zu triangles | %s", entry.triangles,
                                    entry.range == UINT32_MAX ? "not drawn" : "prepared");
                        if (!entry.error.empty())
                            ImGui::TextWrapped("%s", entry.error.c_str());
                    }
        }
        if (ImGui::CollapsingHeader("Last 64 model deliveries / failures"))
            for (const auto& r : models.reports)
                ImGui::TextWrapped("Event %llu / type %u / model %llu / %s", (unsigned long long)r.id, r.type,
                                   (unsigned long long)r.model,
                                   r.error.empty() ? (r.model ? "created" : "no model branch authored")
                                                   : r.error.c_str());
        const auto& effects = game.server.explosion_sprites;
        ImGui::Text("Client explosion sprites: %zu active / %llu started / %llu expired / %llu failed",
                    effects.active.size(), (unsigned long long)effects.started, (unsigned long long)effects.expired,
                    (unsigned long long)effects.failed);
        for (const auto& fx : effects.active)
        {
            ImGui::Text("FX %llu / event %llu: frame %u / %.3f s, alpha %.3f, scale %.3f %.3f %.3f",
                        (unsigned long long)fx.id, (unsigned long long)fx.event, fx.track.frame,
                        fx.last_time - fx.start_time, fx.alpha, fx.scale.x, fx.scale.y, fx.scale.z);
            ImGui::TextWrapped("%s", fx.settings.resource.value_or("Sprites\\Explosn.spr").c_str());
            if (sprite_view)
                for (const auto& entry : sprite_view->entries)
                    if (entry.client_effect == fx.id)
                    {
                        ImGui::Text("Decoded %u x %u | %s", entry.width, entry.height,
                                    entry.range == UINT32_MAX ? "not drawn" : "quad prepared");
                        if (!entry.error.empty())
                            ImGui::TextWrapped("%s", entry.error.c_str());
                    }
        }
        if (ImGui::CollapsingHeader("Last 64 client sprite deliveries / missing composite components"))
            for (const auto& delivery : effects.reports)
            {
                ImGui::Text("Event %llu / type %u / %zu sprites created", (unsigned long long)delivery.id,
                            delivery.type, delivery.sprites.size());
                ImGui::TextWrapped("Unavailable (high-detail preset): %s", delivery.unavailable.c_str());
                if (!delivery.error.empty())
                    ImGui::TextWrapped("%s", delivery.error.c_str());
            }
        ImGui::TextUnformatted("Last 32 activation reports (retained after emitter removal)");
        for (size_t i = 0; i < game.server.scene_explosions.size(); ++i)
        {
            const auto& r = game.server.scene_explosions[i];
            ImGui::PushID(int(i));
            if (ImGui::TreeNodeEx(r.source.c_str(), ImGuiTreeNodeFlags_DefaultOpen))
            {
                ImGui::Text("Type %u | position %.1f %.1f %.1f", r.type, r.position.x, r.position.y, r.position.z);
                ImGui::Text("Configured %.1f / %.1f | candidates %u / in radius %u / delivered %u", r.damage, r.radius,
                            r.targets.candidates, r.targets.in_radius, r.targets.delivered);
                ImGui::Text("Null damager: %s | complete composite unavailable",
                            r.targets.null_damager ? "yes (original guard)" : "no");
                ImGui::Text("Model event %llu | sprite event %llu", (unsigned long long)r.model_event,
                            (unsigned long long)r.sprite_event);
                ImGui::Text("Fragment event %llu", (unsigned long long)r.fragment_event);
                if (!r.fragment_error.empty())
                    ImGui::TextWrapped("%s", r.fragment_error.c_str());
                ImGui::Text("Flame event %llu", (unsigned long long)r.flame_event);
                if (!r.flame_error.empty())
                    ImGui::TextWrapped("%s", r.flame_error.c_str());
                ImGui::Text("Ring event %llu", (unsigned long long)r.ring_event);
                if (!r.ring_error.empty())
                    ImGui::TextWrapped("%s", r.ring_error.c_str());
                ImGui::Text("Light event %llu", (unsigned long long)r.light_event);
                if (!r.light_error.empty())
                    ImGui::TextWrapped("%s", r.light_error.c_str());
                if (!r.model_error.empty())
                    ImGui::TextWrapped("%s", r.model_error.c_str());
                if (!r.sprite_error.empty())
                    ImGui::TextWrapped("%s", r.sprite_error.c_str());
                if (const auto* v = game.server.audio.get(r.voice))
                {
                    ImGui::Text("Voice %llu: %.3f / %.3f s | %u Hz / %u channels", (unsigned long long)v->id,
                                v->cursor / v->clip->rate, v->clip->duration(), v->clip->rate, v->clip->channels);
                    ImGui::Text("Independent tail / dry 3D | radii %.0f / %.0f | L/R %.3f / %.3f",
                                v->request.inner_radius, v->request.outer_radius, v->left, v->right);
                }
                else
                    ImGui::Text("Voice: %s", r.voice           ? "completed or explicitly released"
                                             : r.sound.empty() ? "no sound authored"
                                                               : "start failed");
                if (!r.sound_error.empty())
                    ImGui::TextWrapped("%s", r.sound_error.c_str());
                ImGui::TreePop();
            }
            ImGui::PopID();
        }
    }
} // namespace OpenBlood2
