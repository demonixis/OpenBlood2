#include "app/inspector.h"
#include "game/original/CameraObj.h"
#include <imgui.h>
namespace OpenBlood2
{
    void Inspector::draw_cameras(GameSession& game, ObjectSelection& selection)
    {
        ImGui::TextWrapped(
            "Original scripted cameras: activation, tracking and client view selection. Camera poses and "
            "presentation/listener requests are inspected here; cinematic rendering and audio remain pending.");
        if (!ImGui::BeginTable("Camera inspection", 2, ImGuiTableFlags_Resizable))
            return;
        ImGui::TableSetupColumn("Cameras", ImGuiTableColumnFlags_WidthFixed, 370);
        ImGui::TableSetupColumn("Selected view");
        ImGui::TableNextColumn();
        for (auto id : game.objects.snapshot())
        {
            const auto* r = game.objects.get(id);
            const auto* c = r ? dynamic_cast<const CameraObj*>(r->game.get()) : nullptr;
            if (!c)
                continue;
            if (!game.objects.get(camera_selection))
                camera_selection = id;
            auto label = std::string(r->state.m_Name) + (c->IsActive() ? " [on]" : " [off]");
            if (!(r->state.m_Flags & FLAG_VISIBLE))
                label += " [hidden]";
            if (r->activity != OBJSTATE_ACTIVE)
                label += " [inactive object]";
            ImGui::PushID(static_cast<int>(id.slot));
            if (ImGui::Selectable(label.c_str(), camera_selection == id))
                camera_selection = selection.id = id;
            ImGui::PopID();
        }
        ImGui::Text("Source list %u / descriptors %zu", CameraObj::m_dwNumCameras, game.server.camera_effects.size());
        if (const auto* r = game.objects.get(camera_selection))
            if (const auto* c = dynamic_cast<const CameraObj*>(r->game.get()))
            {
                ImGui::SeparatorText("Source properties/state");
                ImGui::Text("Type %u / published %u", c->m_nType, c->published.type);
                ImGui::Text("ActiveTime %.3f / deadline %.3f", c->m_fActiveTime, c->m_fDeactivateTime);
                ImGui::Text("Scheduled %s / active %s", r->scheduled ? "yes" : "no", c->IsActive() ? "yes" : "no");
                ImGui::Text("Listener: source %d / published %d", c->IsListener(), c->published.listener);
                ImGui::Text("Movement %d / hide player %d", c->m_bPlayerMovement, c->m_bHidePlayer);
                ImGui::Text("Position %.1f %.1f %.1f", r->state.m_Pos.x, r->state.m_Pos.y, r->state.m_Pos.z);
                const auto forward = rotate_vector({0, 0, 1}, r->state.m_Rotation);
                ImGui::Text("Forward %.3f %.3f %.3f", forward.x, forward.y, forward.z);
                const auto* target = game.objects.get(c->GetLinkObject());
                ImGui::TextWrapped("Track: %s / updates %llu", target ? target->state.m_Name : "none",
                                   (unsigned long long)c->tracking_updates);
                if (!c->tracking_error.empty())
                    ImGui::TextWrapped("%s", c->tracking_error.c_str());
                if (!r->model_error.empty())
                    ImGui::TextWrapped("Debug model: %s", r->model_error.c_str());
                if (ImGui::Button("ON"))
                {
                    camera_request = r->id;
                    camera_command = "ON";
                }
                ImGui::SameLine();
                if (ImGui::Button("OFF"))
                {
                    camera_request = r->id;
                    camera_command = "OFF";
                }
                ImGui::InputText("Target name", camera_link_name, sizeof(camera_link_name));
                if (ImGui::Button("LINK"))
                {
                    camera_request = r->id;
                    camera_command = std::string("LINK ") + camera_link_name;
                }
                ImGui::SameLine();
                if (ImGui::Button("Unlink"))
                {
                    camera_request = r->id;
                    camera_command = "LINK";
                }
            }
        ImGui::TableNextColumn();
        const auto& client = game.client_shell;
        const auto* viewed = game.objects.get(client.view.object);
        const auto* listener = game.objects.get(g_hActiveCamera);
        ImGui::TextWrapped("Client camera: %s", viewed ? viewed->state.m_Name : "player eye");
        ImGui::TextWrapped("Server active listener camera: %s",
                           listener && listener->life == Life::Alive ? listener->state.m_Name : "none");
        ImGui::Text("View %.1f %.1f %.1f", client.view.position.x, client.view.position.y, client.view.position.z);
        const auto f = rotate_vector({0, 0, 1}, client.view.rotation);
        ImGui::Text("View forward %.3f %.3f %.3f", f.x, f.y, f.z);
        ImGui::Text("Viewport request: %s",
                    client.view.object && client.view.settings.type == 1 ? "letterbox (top/bottom 1/6)" : "fullscreen");
        ImGui::Text("Player visible %d / weapons visible %d", client.player_visible, client.weapons_visible);
        ImGui::Text("Listener request %.1f %.1f %.1f", client.listener_position.x, client.listener_position.y,
                    client.listener_position.z);
        if (const auto* owner = game.server_shell.Player(game.local_client))
            ImGui::Text("Camera blocks movement / USE / fire: %s", owner->camera_movement_blocked ? "yes" : "no");
        ImGui::Separator();
        ImGui::TextWrapped(
            "Client selection uses the first active camera descriptor; server listener selection follows "
            "the original camera list. Retail worlds suppress the client IsListener flag. Camera "
            "controls are diagnostic; Keyframes inspects path motion. CinematicTrigger remains pending.");
        ImGui::EndTable();
    }
} // namespace OpenBlood2
