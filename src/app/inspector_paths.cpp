#include "inspector.h"
#include <cstdio>
#include <imgui.h>
namespace OpenBlood2
{
    void Inspector::draw_paths(GameSession& game, ObjectSelection& selection)
    {
        const auto& paths = game.server_shell.paths;
        if (!ImGui::CollapsingHeader("Original AI path selection", ImGuiTreeNodeFlags_DefaultOpen))
            return;
        if (!paths.error().empty())
            ImGui::TextWrapped("Snapshot failed: %s", paths.error().c_str());
        if (!path_prefix[0] && paths.size())
        {
            const auto& name = paths.entries().front().name;
            const auto digit = name.find_first_of("0123456789");
            if (digit != name.npos && digit > 0)
                std::snprintf(path_prefix, sizeof(path_prefix), "%.*s", int(digit), name.c_str());
        }
        ImGui::InputText("Path prefix", path_prefix, sizeof(path_prefix));
        ImGui::TextWrapped("Case-insensitive prefix + next digit. Source order reverses enumeration; no numeric sort. "
                           "Copied positions/actions, not live AI traversal.");
        PathList matches;
        std::string error;
        const bool found = paths.GetPath(path_prefix, matches, error);
        ImGui::Text("%zu copied points | %zu selected", paths.size(), matches.size());
        if (!found)
            ImGui::TextWrapped("%s", error.c_str());
        auto rows = [&](const PathList& list, const char* table)
        {
            if (!ImGui::BeginTable(table, 4, ImGuiTableFlags_Borders | ImGuiTableFlags_Resizable))
                return;
            for (const char* name : {"Order / source", "Snapshot position", "Action target", "Action message"})
                ImGui::TableSetupColumn(name);
            ImGui::TableHeadersRow();
            for (std::size_t i = 0; i < list.size(); ++i)
            {
                const auto& p = list[i];
                const auto* source = game.objects.get(p.source);
                const bool alive = source && source->life == Life::Alive;
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::PushID(int(i));
                const auto label = std::to_string(i + 1) + ". " + p.name;
                if (ImGui::Selectable(label.c_str(), alive && selection.id == source->id) && alive)
                    selection.id = source->id;
                ImGui::PopID();
                if (!alive)
                    ImGui::TextUnformatted("source removed");
                else if (!source->model)
                    ImGui::TextUnformatted("source model unavailable");
                else if (source->activity != OBJSTATE_ACTIVE)
                    ImGui::TextUnformatted("source inactive");
                else if (!(source->state.m_Flags & FLAG_VISIBLE))
                    ImGui::TextUnformatted("source hidden");
                ImGui::TableNextColumn();
                ImGui::Text("%.1f %.1f %.1f", p.position.x, p.position.y, p.position.z);
                ImGui::TableNextColumn();
                ImGui::TextWrapped("%s", p.action_target ? p.action_target->c_str() : "<absent>");
                ImGui::TableNextColumn();
                ImGui::TextWrapped("%s", p.action_message ? p.action_message->c_str() : "<absent>");
            }
            ImGui::EndTable();
        };
        if (found)
            rows(matches, "Selected AI path");
        if (ImGui::TreeNode("All copied PathPoints"))
        {
            rows(paths.entries(), "AI path snapshot");
            ImGui::TreePop();
        }
    }
} // namespace OpenBlood2
