#include "app/inspector.h"
#include "engine/scene_effects.h"
#include "game/original/ClientLightFX.h"
#include <algorithm>
#include <imgui.h>
namespace OpenBlood2
{
    void Inspector::draw_lights(GameSession& game, ObjectSelection& selection)
    {
        ImGui::TextWrapped("Original ClientLightFX / CLightFX state. Dynamic point lights follow their source objects; "
                           "effect lights are sorted per object/batch (12 mobile, 32 desktop). DirLight spots and the "
                           "implicit main light are separate pending systems.");
        ImGui::Text("%zu evaluated client lights / session time %.3f", game.server.light_effects.size(),
                    game.server.GetTime());
        if (scene_effects)
        {
            ImGui::Text("Scene FX: %zu live lights / %zu per receiver", scene_effects->lights.size(),
                        scene_effects->light_budget);
            const auto& selection = scene_effects->selection;
            ImGui::Text("%zu receivers / %zu selected references / %zu dropped / peak %zu", selection.receivers,
                        selection.selected, selection.dropped, selection.max_selected);
            ImGui::Text("Unimplemented: %zu fog lights / %zu effect-light shadow requests",
                        scene_effects->fog_lights_unsupported, scene_effects->shadow_lights_unshadowed);
            ImGui::Text("Scene fog: %s", scene_effects->invalid_fog            ? "invalid authored interval"
                                         : scene_effects->uniform.fog_color[3] ? "active"
                                                                               : "off");
        }
        if (!ImGui::BeginTable("Light inspection", 2, ImGuiTableFlags_Resizable))
            return;
        ImGui::TableSetupColumn("Sources", ImGuiTableColumnFlags_WidthFixed, 245);
        ImGui::TableSetupColumn("Evaluated state");
        ImGui::TableNextColumn();
        for (auto id : game.objects.snapshot())
        {
            const auto* r = game.objects.get(id);
            const auto* l = r ? dynamic_cast<const ClientLightFX*>(r->game.get()) : nullptr;
            if (!l)
                continue;
            if (!game.objects.get(light_selection))
                light_selection = id;
            std::string label = std::string(r->state.m_Name) + (l->on ? " [on]" : " [off]");
            if (r->life != Life::Alive)
                label += " [pending removal]";
            if (!(r->state.m_Flags & FLAG_VISIBLE))
                label += " [hidden]";
            if (r->activity != OBJSTATE_ACTIVE)
                label += " [inactive]";
            ImGui::PushID(static_cast<int>(id.slot));
            if (ImGui::Selectable(label.c_str(), light_selection == id))
                light_selection = selection.id = id;
            ImGui::PopID();
        }
        ImGui::TableNextColumn();
        if (const auto* r = game.objects.get(light_selection))
            if (const auto* l = dynamic_cast<const ClientLightFX*>(r->game.get()))
            {
                ImGui::Text("%s / HP %.2f / damage allowed %d", r->type->name.c_str(), l->damage.m_fHitPoints,
                            l->damage.can_damage);
                ImGui::Text("Lifetime %.3f / start %.3f / scheduled %d", l->lifetime, l->start_time, r->scheduled);
                ImGui::Text("Initial delay %.3f / dynamic %d / flags 0x%02x", l->initial_delay, l->dynamic,
                            l->settings.flags);
                for (const char* cmd : {"ON", "OFF", "TOGGLE", "KILL"})
                {
                    if (ImGui::Button(cmd))
                    {
                        light_request = light_selection;
                        light_command = cmd;
                    }
                    ImGui::SameLine();
                }
                ImGui::NewLine();
                ImGui::Text("Last command: %s", l->last_command.c_str());
                if (const auto* fx = game.server.GetLightEffect(r->handle))
                {
                    ImGui::Text("Client visible %d / radius %.2f / updates %llu", fx->visible, fx->radius,
                                (unsigned long long)fx->updates);
                    ImGui::Text("Position %.2f %.2f %.2f", fx->position.x, fx->position.y, fx->position.z);
                    ImGui::Text("RGB %.4f %.4f %.4f", fx->color.x, fx->color.y, fx->color.z);
                    ImGui::ColorButton("Evaluated RGB", ImVec4(fx->color.x, fx->color.y, fx->color.z, 1),
                                       ImGuiColorEditFlags_NoTooltip, ImVec2(80, 20));
                    ImGui::TextUnformatted("XZ source placement (selected radius outline; no world lighting)");
                    const auto origin = ImGui::GetCursorScreenPos();
                    const auto available = ImGui::GetContentRegionAvail();
                    const ImVec2 size(std::max(100.0f, available.x), std::clamp(available.y - 28, 80.0f, 140.0f));
                    DVector low = fx->position, high = low;
                    for (const auto& v : game.server.light_effects)
                    {
                        low.x = std::min(low.x, v.position.x);
                        low.z = std::min(low.z, v.position.z);
                        high.x = std::max(high.x, v.position.x);
                        high.z = std::max(high.z, v.position.z);
                    }
                    float scale = std::min((size.x - 20) / std::max(1.0f, high.x - low.x),
                                           (size.y - 20) / std::max(1.0f, high.z - low.z));
                    auto point = [&](DVector v)
                    { return ImVec2(origin.x + 10 + (v.x - low.x) * scale, origin.y + 10 + (v.z - low.z) * scale); };
                    auto* draw = ImGui::GetWindowDrawList();
                    draw->PushClipRect(origin, ImVec2(origin.x + size.x, origin.y + size.y), true);
                    draw->AddRectFilled(origin, ImVec2(origin.x + size.x, origin.y + size.y),
                                        IM_COL32(20, 24, 30, 255));
                    for (const auto& v : game.server.light_effects)
                        draw->AddCircleFilled(
                            point(v.position), v.source == fx->source ? 5 : 3,
                            v.visible ? ImGui::ColorConvertFloat4ToU32(ImVec4(v.color.x, v.color.y, v.color.z, 1))
                                      : IM_COL32(90, 90, 90, 255));
                    draw->AddCircle(point(fx->position), std::max(1.0f, fx->radius * scale),
                                    IM_COL32(240, 180, 40, 255));
                    draw->PopClipRect();
                    ImGui::Dummy(size);
                }
                else
                    ImGui::TextUnformatted("No live client light state.");
                if (ImGui::CollapsingHeader("Wave parameters and ramp resources"))
                {
                    auto wave = [](const char* name, const LightWave& w)
                    {
                        ImGui::Text("%s: wave %u / range %.2f .. %.2f", name, w.kind, w.min, w.max);
                        ImGui::Text("Frequency %.3f rad/s / phase %.3f rad", w.frequency, w.phase);
                        float values[96]{};
                        for (int i = 0; i < 96; ++i)
                            light_wave_value(w.kind, w.min, w.max, 6.283185307f * i / 95, values[i]);
                        ImGui::PushID(name);
                        ImGui::PlotLines("One phase cycle", values, 96, 0, nullptr, std::min(w.min, w.max) - .01f,
                                         std::max(w.min, w.max) + .01f, ImVec2(0, 40));
                        ImGui::PopID();
                    };
                    wave("Intensity", l->settings.intensity);
                    wave("Radius", l->settings.radius);
                    for (const auto* sound : {&l->settings.ramp_up_sound, &l->settings.ramp_down_sound})
                        if (!sound->empty())
                            ImGui::TextWrapped("Ramp resource: %s [%s; not played]", sound->c_str(),
                                               game.resources.find(*sound) ? "indexed" : "missing");
                }
            }
        ImGui::EndTable();
    }
} // namespace OpenBlood2
