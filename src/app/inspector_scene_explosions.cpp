#include "game/original/ClientExplosionSFX.h"
#include "inspector.h"
#include <imgui.h>
namespace OpenBlood2
{
    void Inspector::draw_scene_explosions(GameSession& game, ObjectSelection& selection)
    {
        ImGui::TextWrapped(
            "Original CClientExplosionSFX emitters. The original null damager suppresses all damage. "
            "Spatial WAV playback is implemented; client explosion FX and their rendering are UNAVAILABLE.");
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
                ImGui::Text("Null damager: %s | client FX unavailable",
                            r.targets.null_damager ? "yes (original guard)" : "no");
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
