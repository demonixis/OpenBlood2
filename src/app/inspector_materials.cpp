#include "app/inspector.h"
#include <imgui.h>

namespace OpenBlood2
{
    // Enhanced renderer materials (roadmap 5.5): the resolved class and its binding, maps,
    // emission and the DAT's fully bright faces, with the fallbacks that need attention.
    void Inspector::draw_materials()
    {
        if (!materials || !ImGui::TreeNode("Enhanced materials (classes, maps, emission)"))
            return;
        const auto& r = *materials;
        if (r.materials.empty())
        {
            ImGui::TextWrapped("No report: select RENDERER [ENHANCED] in Options > Display, or no world is loaded.");
            ImGui::TreePop();
            return;
        }
        ImGui::Text("%zu texture materials: %zu default class, %zu failed resources, %zu generated normal maps",
                    r.materials.size(), r.defaults, r.failed, r.generated);
        ImGui::Text("Emission: %zu manifest-emissive materials; %zu materials with %zu fully bright DAT faces "
                    "(%zu both)",
                    r.emissive, r.fullbright_materials, r.fullbright_faces, r.emissive_fullbright);
        std::string classes;
        for (const auto& [name, count] : r.classes)
            classes += (classes.empty() ? "" : ", ") + name + " " + std::to_string(count);
        ImGui::TextWrapped("Classes: %s", classes.c_str());
        if (r.unbound_overrides.empty())
            ImGui::TextUnformatted("Texture overrides: every override names a mounted resource.");
        else
        {
            ImGui::TextColored({1, .6f, .3f, 1},
                               "%zu texture overrides name no mounted resource:", r.unbound_overrides.size());
            for (const auto& path : r.unbound_overrides)
                ImGui::BulletText("%s", path.c_str());
        }
        ImGui::Checkbox("Default class only", &materials_defaults_only);
        ImGui::SameLine();
        ImGui::Checkbox("Emissive or fully bright only", &materials_emissive_only);
        constexpr auto flags = ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY |
                               ImGuiTableFlags_Resizable | ImGuiTableFlags_SizingFixedFit;
        if (ImGui::BeginTable("materials", 7, flags, {0, 320}))
        {
            ImGui::TableSetupScrollFreeze(0, 1);
            for (const char* name :
                 {"Texture", "Class", "Binding", "Rough / metal / F0", "Normal map", "Emission", "Bright faces"})
                ImGui::TableSetupColumn(name);
            ImGui::TableHeadersRow();
            for (const auto& m : r.materials)
            {
                if ((materials_defaults_only && m.source != SurfaceSource::Default) ||
                    (materials_emissive_only && !m.emissive() && !m.fullbright_faces))
                    continue;
                const auto& v = m.values;
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(m.path.c_str());
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(v.name.c_str());
                ImGui::TableNextColumn();
                ImGui::Text("%s%s",
                            m.source == SurfaceSource::Rule      ? "rule "
                            : m.source == SurfaceSource::Texture ? "override "
                                                                 : "default",
                            m.binding.c_str());
                ImGui::TableNextColumn();
                ImGui::Text("%.2f / %.2f / %.2f", double(v.roughness), double(v.metallic), double(v.specular));
                ImGui::TableNextColumn();
                ImGui::Text("%s x%.1f", normal_map_state_name(m.normal_map), double(v.normal));
                ImGui::TableNextColumn();
                if (m.emissive())
                    ImGui::Text("%.1f above %.2f", double(v.emissive), double(v.emissive_threshold));
                else
                    ImGui::TextUnformatted("-");
                ImGui::TableNextColumn();
                ImGui::Text("%u", m.fullbright_faces);
            }
            ImGui::EndTable();
        }
        ImGui::TextWrapped("Replacement data: the original game has no material classes. Fully bright faces are "
                           "drawn with the original output; manifest emission adds a glow in the enhanced shaders.");
        ImGui::TreePop();
    }
} // namespace OpenBlood2
