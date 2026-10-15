#include "app/inspector.h"
#include "game/original/ObjectivesTrigger.h"
#include <imgui.h>
namespace OpenBlood2
{
    namespace
    {
        void objective_text(const std::string& text)
        {
            std::string display = text;
            for (char& c : display)
                if (c == '|')
                    c = '\n';
            ImGui::TextWrapped("%s", display.c_str());
        }
    } // namespace
    void Inspector::draw_objectives(GameSession& game, ObjectSelection& selection)
    {
        ImGui::TextWrapped("Original mission objectives. Localized text comes from external cres.dll; '|' marks a line "
                           "break. Audio and original HUD remain pending.");
        if (!ImGui::BeginTable("Objectives", 2, ImGuiTableFlags_Resizable))
            return;
        ImGui::TableSetupColumn("Authored triggers", ImGuiTableColumnFlags_WidthFixed, 280);
        ImGui::TableSetupColumn("Client objective");
        ImGui::TableNextColumn();
        for (auto id : game.objects.snapshot())
        {
            const auto* r = game.objects.get(id);
            const auto* t = r ? dynamic_cast<const ObjectivesTrigger*>(r->game.get()) : nullptr;
            if (!t)
                continue;
            if (!game.objects.get(objective_selection))
                objective_selection = id;
            std::string label = std::string(r->state.m_Name) + (t->active ? "" : " [inactive]");
            if (!(r->state.m_Flags & FLAG_VISIBLE))
                label += " [hidden]";
            ImGui::PushID(static_cast<int>(id.slot));
            if (ImGui::Selectable(label.c_str(), objective_selection == id))
                objective_selection = selection.id = id;
            ImGui::PopID();
        }
        if (const auto* r = game.objects.get(objective_selection))
            if (const auto* t = dynamic_cast<const ObjectivesTrigger*>(r->game.get()))
            {
                ImGui::SeparatorText("Selected source");
                ImGui::Text("ResourceNum: %u", t->objectives.resource);
                ImGui::TextWrapped("Authored title: %s", t->objectives.title.c_str());
                ImGui::TextWrapped("Authored text: %s", t->objectives.text.c_str());
                ImGui::Text("Activation attempts: %llu / rejected %llu", (unsigned long long)t->activations,
                            (unsigned long long)t->rejected);
                ImGui::Text("Queued: %llu / last result %d", (unsigned long long)t->deliveries, t->last_delivery);
                ImGui::TextWrapped("Sound: %s / %llu requests (not played)", t->sound.c_str(),
                                   (unsigned long long)t->objective_sound_requests);
                if (game.client_shell.m_bInWorld && ImGui::Button("Send TRIGGER as player"))
                {
                    trigger_request = r->id;
                    trigger_command = "TRIGGER";
                }
            }
        ImGui::TableNextColumn();
        const auto& client = game.client_shell;
        ImGui::Text("Received: %llu / %s", (unsigned long long)client.objective_messages,
                    client.objectives_resolved ? "resolved" : "not resolved");
        ImGui::Text("ResourceNum: %u", client.objectives.resource);
        ImGui::Separator();
        objective_text(client.objectives.title);
        ImGui::Separator();
        objective_text(client.objectives.text);
        if (!client.objectives_error.empty())
            ImGui::TextWrapped("%s", client.objectives_error.c_str());
        ImGui::SeparatorText("Resource reader");
        const auto* resource = game.resources.find("cres.dll");
        ImGui::Text("cres.dll: %s", resource ? "indexed" : "missing");
        if (resource)
            ImGui::TextWrapped("Archive: %s", path_text(resource->archive).c_str());
        ImGui::Text("Decoded: %zu strings / language 0x%04x", game.server.client_strings.strings.size(),
                    game.server.client_strings.language);
        ImGui::TextWrapped("%s", game.server.client_strings_error.c_str());
        ImGui::EndTable();
    }
} // namespace OpenBlood2
