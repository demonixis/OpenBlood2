#include "app/inspector.h"
#include "game/original/KeyFramer.h"
#include <imgui.h>
namespace OpenBlood2
{
    void Inspector::draw_keyframes(GameSession& game, ObjectSelection& selection)
    {
        ImGui::TextWrapped(
            "Original KeyFramer timelines. Keys are copied, then their source objects are removed. "
            "Camera/ordinary-object paths run; world-model/container motion and key audio remain unavailable.");
        if (!ImGui::BeginTable("Keyframe inspection", 2, ImGuiTableFlags_Resizable))
            return;
        ImGui::TableSetupColumn("Controllers", ImGuiTableColumnFlags_WidthFixed, 270);
        ImGui::TableSetupColumn("Owned keys and targets");
        ImGui::TableNextColumn();
        for (auto id : game.objects.snapshot())
        {
            const auto* r = game.objects.get(id);
            const auto* f = r ? dynamic_cast<const KeyFramer*>(r->game.get()) : nullptr;
            if (!f)
                continue;
            if (!game.objects.get(keyframe_selection))
                keyframe_selection = id;
            std::string label = std::string(r->state.m_Name) + (f->active ? " [on]" : " [off]");
            if (!(r->state.m_Flags & FLAG_VISIBLE))
                label += " [hidden]";
            if (r->activity != OBJSTATE_ACTIVE)
                label += " [inactive]";
            ImGui::PushID(static_cast<int>(id.slot));
            if (ImGui::Selectable(label.c_str(), keyframe_selection == id))
                keyframe_selection = selection.id = id;
            ImGui::PopID();
        }
        ImGui::TableNextColumn();
        if (const auto* r = game.objects.get(keyframe_selection))
            if (const auto* f = dynamic_cast<const KeyFramer*>(r->game.get()))
            {
                ImGui::Text("Target name: %s / key prefix: %s", f->object_name.c_str(), f->base_key_name.c_str());
                ImGui::Text("Initialized %d / start %d / loop %d / active %d", f->initialized, f->start_active,
                            f->looping, f->active);
                ImGui::Text("Time %.4f / next %zu / processed %llu", f->time, f->next,
                            (unsigned long long)f->processed);
                ImGui::Text("Schedule %d / alive %d", r->scheduled, r->life == Life::Alive);
                if (!f->error.empty())
                    ImGui::TextWrapped("Diagnostic: %s", f->error.c_str());
                ImGui::Text("Targets %zu / copied keys %zu", f->targets.size(), f->keys.size());
                for (const auto& t : f->targets)
                {
                    const auto* target = game.objects.get(t.object);
                    const char* result = !t.attempts                        ? "not attempted"
                                         : t.result == MoverResult::Moved   ? "reached"
                                         : t.result == MoverResult::Blocked ? "blocked/clipped"
                                         : t.result == MoverResult::Invalid ? "invalid/removed"
                                                                            : "unavailable";
                    ImGui::TextWrapped("%s (%s): %s / reached %llu", target ? target->state.m_Name : "removed",
                                       target ? target->type->name.c_str() : "none", result,
                                       (unsigned long long)t.moves);
                    if (target)
                        ImGui::Text("Position %.2f %.2f %.2f / offset %.2f %.2f %.2f", target->state.m_Pos.x,
                                    target->state.m_Pos.y, target->state.m_Pos.z, t.offset.x, t.offset.y, t.offset.z);
                }
                ImGui::Text("Message keys %llu / last matches %zu / unresolved %llu",
                            (unsigned long long)f->message_keys, f->last_message_matches,
                            (unsigned long long)f->unresolved_messages);
                ImGui::TextWrapped("Sound requests %llu (not played): %s", (unsigned long long)f->sound_requests,
                                   f->last_sound.c_str());
                ImGui::TextWrapped("Print requests %llu: %s", (unsigned long long)f->print_requests,
                                   f->last_print.c_str());
                ImGui::SeparatorText("Owned original key values");
                ImGui::BeginChild("Copied keys", ImVec2(0, 200));
                for (const auto& key : f->keys)
                {
                    ImGui::PushID(&key);
                    if (ImGui::TreeNode(key.name.c_str()))
                    {
                        ImGui::Text("Relative %.4f / cumulative %.4f", key.timestamp, key.time);
                        ImGui::Text("Position %.2f %.2f %.2f", key.position.x, key.position.y, key.position.z);
                        ImGui::Text("Quaternion %.3f %.3f %.3f %.3f", key.rotation.m_Vec.x, key.rotation.m_Vec.y,
                                    key.rotation.m_Vec.z, key.rotation.m_Spin);
                        ImGui::TextWrapped("Message %s: %s", key.message_target.c_str(), key.message.c_str());
                        ImGui::TextWrapped("Sound %s / radius %.1f / loop %d", key.sound.c_str(), key.sound_radius,
                                           key.loop_sound);
                        ImGui::TextWrapped("Print: %s", key.print.c_str());
                        ImGui::TreePop();
                    }
                    ImGui::PopID();
                }
                ImGui::EndChild();
            }
        ImGui::EndTable();
    }
} // namespace OpenBlood2
