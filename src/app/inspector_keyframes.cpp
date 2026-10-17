#include "app/inspector.h"
#include "engine/world_model.h"
#include "game/original/KeyFramer.h"
#include <algorithm>
#include <imgui.h>
namespace OpenBlood2
{
    void Inspector::draw_keyframes(GameSession& game, ObjectSelection& selection)
    {
        ImGui::TextWrapped("Original KeyFramer timelines. Keys are copied, then their source objects are removed. "
                           "Camera, ordinary-object and translating BoxPhysics brush paths run. Angular brush paths, "
                           "moving containers remain unavailable. Sound keys replace one retained attached voice.");
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
            if (r->life != Life::Alive)
                label += " [pending removal]";
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
                for (const char* cmd : {"ON", "OFF"})
                {
                    if (ImGui::Button(cmd))
                    {
                        keyframe_request = keyframe_selection;
                        keyframe_command = cmd;
                    }
                    ImGui::SameLine();
                }
                ImGui::NewLine();
                ImGui::TextWrapped("Last sound key: %s", f->last_sound.c_str());
                ImGui::Text("Sound attempts %llu / starts %llu / failures %llu", (unsigned long long)f->sound_requests,
                            (unsigned long long)f->sound_starts, (unsigned long long)f->sound_failures);
                const auto* speaker = game.objects.get(f->sound_target);
                ImGui::Text("Last attempt target %s / radius %.1f / loop %d / retained voice %llu",
                            speaker ? speaker->state.m_Name : "none/removed", f->sound_radius, f->sound_loop,
                            (unsigned long long)f->sound_voice);
                if (const auto* voice = game.server.audio.get(f->sound_voice))
                {
                    ImGui::Text("Decoded frames %llu / cursor %.1f / loops %llu / done %d",
                                (unsigned long long)(voice->clip ? voice->clip->frames() : 0), voice->cursor,
                                (unsigned long long)voice->loops, voice->done);
                    ImGui::Text("Last mixed position %.1f %.1f %.1f", voice->position.x, voice->position.y,
                                voice->position.z);
                }
                else
                    ImGui::TextUnformatted("No live retained voice (not started, failed, or owner removed).");
                if (!f->sound_error.empty())
                    ImGui::TextWrapped("Sound failure: %s", f->sound_error.c_str());
                ImGui::TextUnformatted("Dry playback; reverb and priority arbitration unavailable.");
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
                    {
                        ImGui::Text("Position %.2f %.2f %.2f / offset %.2f %.2f %.2f", target->state.m_Pos.x,
                                    target->state.m_Pos.y, target->state.m_Pos.z, t.offset.x, t.offset.y, t.offset.z);
                        if (target->world_model)
                        {
                            const auto center = object_bounds_center(*target);
                            ImGui::Text("Brush center %.2f %.2f %.2f / half %.1f %.1f %.1f", center.x, center.y,
                                        center.z, target->dimensions.x, target->dimensions.y, target->dimensions.z);
                            const auto* blocker = game.objects.get(target->mover_blocker);
                            ImGui::Text(
                                "Moves %llu / pushes %llu / riders %llu / blocks %llu",
                                (unsigned long long)target->mover_moves, (unsigned long long)target->mover_pushes,
                                (unsigned long long)target->mover_riders, (unsigned long long)target->mover_blocks);
                            if (target->mover_blocks)
                                ImGui::Text("Last blocker: %s", blocker                 ? blocker->state.m_Name
                                                                : target->mover_blocker ? "removed object"
                                                                                        : "primary world");
                            ImGui::TextUnformatted("Plan view (Z right, X down): path / collision bounds");
                            const auto origin = ImGui::GetCursorScreenPos();
                            const ImVec2 size(std::max(100.0f, ImGui::GetContentRegionAvail().x), 80);
                            auto low = vector_subtract(center, target->dimensions),
                                 high = vector_add(center, target->dimensions);
                            for (const auto& k : f->keys)
                            {
                                const auto p = vector_add(k.position, t.offset);
                                low.x = std::min(low.x, p.x);
                                low.z = std::min(low.z, p.z);
                                high.x = std::max(high.x, p.x);
                                high.z = std::max(high.z, p.z);
                            }
                            const float scale = std::min((size.x - 20) / std::max(1.0f, high.z - low.z),
                                                         (size.y - 20) / std::max(1.0f, high.x - low.x));
                            auto point = [&](DVector p)
                            {
                                return ImVec2(origin.x + 10 + (p.z - low.z) * scale,
                                              origin.y + 10 + (p.x - low.x) * scale);
                            };
                            auto* draw = ImGui::GetWindowDrawList();
                            draw->PushClipRect(origin, ImVec2(origin.x + size.x, origin.y + size.y), true);
                            draw->AddRectFilled(origin, ImVec2(origin.x + size.x, origin.y + size.y),
                                                IM_COL32(20, 24, 30, 255));
                            for (std::size_t i = 0; i < f->keys.size(); ++i)
                            {
                                auto p = point(vector_add(f->keys[i].position, t.offset));
                                draw->AddCircleFilled(p, 3, IM_COL32(100, 180, 250, 255));
                                if (i)
                                    draw->AddLine(point(vector_add(f->keys[i - 1].position, t.offset)), p,
                                                  IM_COL32(100, 180, 250, 255));
                            }
                            draw->AddRect(point(vector_subtract(center, target->dimensions)),
                                          point(vector_add(center, target->dimensions)), IM_COL32(240, 180, 40, 255));
                            draw->AddCircleFilled(point(target->state.m_Pos), 4, IM_COL32(240, 180, 40, 255));
                            draw->PopClipRect();
                            ImGui::Dummy(size);
                        }
                    }
                }
                ImGui::Text("Message keys %llu / last matches %zu / unresolved %llu",
                            (unsigned long long)f->message_keys, f->last_message_matches,
                            (unsigned long long)f->unresolved_messages);
                ImGui::TextWrapped("Print requests %llu: %s", (unsigned long long)f->print_requests,
                                   f->last_print.c_str());
                if (ImGui::CollapsingHeader("Owned original key values"))
                {
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
            }
        ImGui::EndTable();
    }
} // namespace OpenBlood2
