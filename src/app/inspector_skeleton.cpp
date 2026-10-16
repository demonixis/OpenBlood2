#include "game/original/AI_Mgr.h"
#include "inspector.h"
#include <algorithm>
#include <imgui.h>
namespace OpenBlood2
{
    void Inspector::draw_skeleton(GameSession& game, ObjectSelection& selection)
    {
        ImGui::TextWrapped("Current ABC node transforms and original CAnim_Sound tables. Spheres are the original "
                           "initialization radii, shown at live node positions; damage localization and complete AI "
                           "behavior remain unported.");
        if (ImGui::CollapsingHeader("Live attachments", ImGuiTreeNodeFlags_DefaultOpen))
        {
            ImGui::TextWrapped("Held models follow current parent nodes. Physical/nested attachments and dropped "
                               "weapons remain unavailable.");
            for (const auto& [id, a] : game.objects.attachments())
            {
                const auto* parent = game.objects.get(a.parent);
                const auto* child = game.objects.get(a.child);
                if (!parent || !child)
                    continue;
                ImGui::Text("#%llu %s -> %s | %s | %s", (unsigned long long)id, parent->state.m_Name,
                            child->state.m_Name, a.node.empty() ? "object origin (no gun node)" : a.node.c_str(),
                            a.resolved ? "resolved" : "POSE UNAVAILABLE");
                ImGui::Text("Child %.2f %.2f %.2f | %s / %s", child->state.m_Pos.x, child->state.m_Pos.y,
                            child->state.m_Pos.z, child->state.m_Flags & FLAG_VISIBLE ? "visible" : "hidden",
                            child->activity == OBJSTATE_ACTIVE ? "active" : "inactive");
                ImGui::TextWrapped("%s | skin indexed: %s", child->state.m_Filename,
                                   game.resources.find(child->state.m_SkinName) ? "yes" : "NO");
            }
            if (game.objects.attachments().empty())
                ImGui::TextUnformatted("No live attachments");
        }
        if (!game.objects.get(skeleton_selection))
        {
            if (const auto* player = game.server_shell.Player(game.local_client))
                skeleton_selection = game.objects.get(player->m_hObject)->id;
        }
        if (!ImGui::BeginTable("Skeleton inspection", 2, ImGuiTableFlags_Resizable))
            return;
        ImGui::TableSetupColumn("Models", ImGuiTableColumnFlags_WidthFixed, 210);
        ImGui::TableNextColumn();
        for (auto id : game.objects.snapshot())
        {
            const auto* r = game.objects.get(id);
            if (!r || r->state.m_ObjectType != OT_MODEL)
                continue;
            auto label = std::string(r->state.m_Name) + " #" + std::to_string(id.slot);
            if (!(r->state.m_Flags & FLAG_VISIBLE))
                label += " hidden";
            if (r->activity != OBJSTATE_ACTIVE)
                label += " inactive";
            if (!r->model)
                label += " missing model";
            if (ImGui::Selectable(label.c_str(), skeleton_selection == id))
                skeleton_selection = selection.id = id;
        }
        ImGui::TableNextColumn();
        const auto* r = game.objects.get(skeleton_selection);
        const auto* target_ai = r ? dynamic_cast<const AI_Mgr*>(r->game.get()) : nullptr;
        if (target_ai && ImGui::CollapsingHeader("Original AI perception", ImGuiTreeNodeFlags_DefaultOpen))
        {
            const auto& ai = *target_ai;
            ImGui::Text("Faction: %s | Cabal %u / Monster %u (player in both)", ai.m_bCabal ? "Cabal" : "Monster",
                        AI_Mgr::m_dwNumCabal, AI_Mgr::m_dwNumMonster);
            ImGui::Text("Ranges: sight %.1f / smell %.1f / sense %.1f", ai.m_fSeeingDist, ai.m_fSmellingDist,
                        ai.m_fSensingDist);
            ImGui::Text("Stimulus %d | damage memory %s | calls %llu / failures %llu", ai.stimulus,
                        ai.from_damage ? "used" : "not used", (unsigned long long)ai.perception_calls,
                        (unsigned long long)ai.perception_failures);
            ImGui::Text("Strength: sight %.3f / smell %.3f / sense %.3f / health %.3f", ai.m_fStimuli[0],
                        ai.m_fStimuli[2], ai.m_fStimuli[3], ai.m_fStimuli[5]);
            auto report = [&](const char* label, const AIFindReport& query)
            {
                const auto* selected = game.objects.get(query.selected);
                ImGui::Text("%s: %s | selected %s", label, query.valid ? "evaluated" : "not evaluated / failed",
                            selected && selected->life == Life::Alive ? selected->state.m_Name : "none / removed");
                ImGui::Text("Visited %u / eligible %u / rays %u | distance %.2f | origin %s", query.visited,
                            query.eligible, query.rays, query.distance, query.origin_head ? "head" : "body");
                if (!query.error.empty())
                    ImGui::TextWrapped("%s", query.error.c_str());
            };
            report("Sight", ai.stimulus_queries[0]);
            report("Smell", ai.stimulus_queries[1]);
            report("Sense", ai.stimulus_queries[2]);
            if (ImGui::TreeNode("Last radius query"))
            {
                report("Query", ai.find);
                ImGui::TreePop();
            }
            if (!ai.perception_error.empty())
                ImGui::TextWrapped("%s", ai.perception_error.c_str());
            ImGui::TextWrapped("Recorded source perception commands. No automatic NPC update/state machine, navigation "
                               "or attack yet.");
        }
        if (target_ai && ImGui::CollapsingHeader("Original AI target / facing"))
        {
            const auto* target = game.objects.get(target_ai->GetTarget());
            ImGui::Text("Target: %s | %s", target ? target->state.m_Name : "none / removed",
                        target ? (target->life == Life::Alive ? "alive" : "pending removal") : "no link");
            ImGui::Text("Named request: %s | outgoing links: %zu", target_ai->target_name.c_str(),
                        game.objects.link_count(r->handle));
            const auto pos = target_ai->m_vTargetPos;
            ImGui::Text("Cached position: %.2f %.2f %.2f | %s", pos.x, pos.y, pos.z,
                        target_ai->target_position_valid ? "sampled" : "unavailable");
            ImGui::Text("Metacmd %d | turns %llu | last local yaw %.4f rad", target_ai->Metacmd,
                        (unsigned long long)target_ai->face_commands, target_ai->face_yaw_delta);
            ImGui::Text("Target changes %llu | broken links %llu | failures %llu",
                        (unsigned long long)target_ai->target_changes,
                        (unsigned long long)target_ai->target_link_breaks,
                        (unsigned long long)target_ai->target_failures);
            if (!target_ai->target_error.empty())
                ImGui::TextWrapped("%s", target_ai->target_error.c_str());
            ImGui::TextWrapped(
                "Original target commands. State-driven updates, tracking and AI states remain unavailable.");
        }
        if (!r)
            ImGui::TextUnformatted("No selected model");
        else if (!r->model)
            ImGui::TextWrapped("Model unavailable: %s", r->model_error.c_str());
        else
        {
            const auto* character = dynamic_cast<const CBaseCharacter*>(r->game.get());
            const auto& model = *r->model;
            const auto* ai = dynamic_cast<const AI_Mgr*>(character);
            const auto* anim = ai ? ai->AnimSound() : character ? &character->m_Anim_Sound : nullptr;
            ImGui::Text("%s | %zu nodes | %zu clips", r->state.m_Name, model.nodes.size(), model.animations.size());
            if (r->animation.animation < model.animations.size())
                ImGui::Text("%s / %.1f ms", model.animations[r->animation.animation].name.c_str(), r->animation.time);
            if (anim)
            {
                ImGui::Text("Initial hit spheres: %s", anim->hit_spheres_valid ? "computed" : "unavailable");
                if (!anim->hit_sphere_error.empty())
                    ImGui::TextWrapped("%s", anim->hit_sphere_error.c_str());
            }
            else
                ImGui::TextUnformatted("No original character animation table on this object");
            if (ai && ImGui::CollapsingHeader("Original AI visibility", ImGuiTreeNodeFlags_DefaultOpen))
            {
                const auto& s = ai->sight;
                ImGui::Text("%s | %u rays | %llu queries", ai_sight_outcome_name(s.outcome), s.rays,
                            (unsigned long long)ai->sight_queries);
                const auto* target = game.objects.get(s.target);
                ImGui::Text("Target: %s",
                            target && target->life == Life::Alive ? target->state.m_Name : "removed / none");
                ImGui::Text("Origin (%s): %.2f %.2f %.2f", s.origin_head ? "animated head" : "object position",
                            s.origin.x, s.origin.y, s.origin.z);
                ImGui::Text("Body: %.2f %.2f %.2f | head retry: %s", s.body.x, s.body.y, s.body.z,
                            s.retried_head ? "yes" : "no");
                if (s.retried_head)
                    ImGui::Text("Head: %.2f %.2f %.2f", s.head.x, s.head.y, s.head.z);
                auto hit = [&](const char* name, const SegmentHit& h, SegmentResult result)
                {
                    if (result != SegmentResult::Hit)
                        return;
                    const auto* record = game.objects.get(h.object);
                    ImGui::Text("%s hit: %s | fraction %.4f | polygon %u", name,
                                h.object ? (record ? record->state.m_Name : "removed object") : "primary world",
                                h.fraction, h.polygon);
                };
                hit("Body", s.body_hit, s.body_result);
                hit("Head", s.head_hit, s.head_result);
                ImGui::TextWrapped("Last simulation query. Requires a target hit; skips VolumeBrush and destructible "
                                   "glass brushes. No field-of-view, distance, hearing or target acquisition implied.");
            }
            if (ai && ImGui::CollapsingHeader("Original AI model-key dispatch"))
            {
                ImGui::Text("State %d | keys %llu | audio starts %llu / keys %llu | failures %llu", ai->m_nState,
                            (unsigned long long)ai->model_keys, (unsigned long long)ai->audio_starts,
                            (unsigned long long)ai->audio_keys, (unsigned long long)ai->key_failures);
                ImGui::TextWrapped("%s | %s", ai_key_outcome_name(ai->key_outcome), ai->ai_key.c_str());
                if (!ai->ai_key_error.empty())
                    ImGui::TextWrapped("%s", ai->ai_key_error.c_str());
                ImGui::TextWrapped("Sound: %s", ai->ai_sound_resource.c_str());
                if (const auto* voice = anim ? game.server.audio.get(anim->m_hVoxSound) : nullptr)
                    ImGui::Text("Shared voice %s | %.3f / %.3f s | owner matches: %s", voice->done ? "done" : "playing",
                                voice->cursor / voice->clip->rate, voice->clip->duration(),
                                voice->owner == r->handle ? "yes" : "no");
                ImGui::Text("Variant %d (source B2_LITE: 1..3) | chance draw %d", ai->selected_variant,
                            ai->chance_roll);
                ImGui::Text("Suppressed: camera %llu / script %llu / chance %llu / shared voice %llu",
                            (unsigned long long)ai->camera_suppressed, (unsigned long long)ai->script_suppressed,
                            (unsigned long long)ai->chance_suppressed, (unsigned long long)ai->busy_voices);
                ImGui::Text("No target %llu | unavailable actions %llu", (unsigned long long)ai->no_target,
                            (unsigned long long)ai->unavailable_keys);
                ImGui::TextWrapped("Key dispatcher only. AI states, attacks and special actions "
                                   "remain unavailable.");
            }
            if (character && ImGui::CollapsingHeader("Character motion keys",
                                                     ai ? ImGuiTreeNodeFlags_None : ImGuiTreeNodeFlags_DefaultOpen))
            {
                ImGui::Text("Keys %llu | applied actions %llu | failures %llu | last mask 0x%02x",
                            (unsigned long long)character->motion_keys, (unsigned long long)character->motion_actions,
                            (unsigned long long)character->motion_failures, character->motion_applied);
                ImGui::TextWrapped("Last key: %s", character->motion_key.c_str());
                ImGui::Text("Position %.2f %.2f %.2f | velocity %.2f %.2f %.2f", r->state.m_Pos.x, r->state.m_Pos.y,
                            r->state.m_Pos.z, r->velocity.x, r->velocity.y, r->velocity.z);
                ImGui::Text("Half dimensions %.2f %.2f %.2f", r->dimensions.x, r->dimensions.y, r->dimensions.z);
                if (!character->motion_error.empty())
                    ImGui::TextWrapped("%s", character->motion_error.c_str());
                ImGui::TextWrapped(dynamic_cast<const CPlayerObj*>(character)
                                       ? "Player override excludes base motion keys, as in the source."
                                       : "Source order: local yaw, forward/right/up velocity overwrite, world delta, "
                                         "centered dimensions.");
            }
            if (anim && ImGui::CollapsingHeader("Character audio",
                                                ai ? ImGuiTreeNodeFlags_None : ImGuiTreeNodeFlags_DefaultOpen))
            {
                ImGui::TextWrapped("Root: %s | silent flag: %s (source helper does not test it)",
                                   anim->GetSoundRoot().c_str(), anim->m_bSilent ? "set" : "clear");
                ImGui::Text("Voice helper: %llu starts / %llu attempts / %llu failures / %llu busy",
                            (unsigned long long)anim->sound_starts, (unsigned long long)anim->sound_attempts,
                            (unsigned long long)anim->sound_failures, (unsigned long long)anim->voice_busy);
                if (!anim->sound_error.empty())
                    ImGui::TextWrapped("%s", anim->sound_error.c_str());
                auto voice = [&](SoundId id)
                {
                    const auto* v = game.server.audio.get(id);
                    if (!v)
                    {
                        ImGui::Text("Voice %llu: none / released", (unsigned long long)id);
                        return;
                    }
                    ImGui::TextWrapped("%s | %s | %.3f / %.3f s | attached %.1f %.1f %.1f", v->request.resource.c_str(),
                                       v->done ? "done" : "playing", v->cursor / v->clip->rate, v->clip->duration(),
                                       v->position.x, v->position.y, v->position.z);
                };
                voice(anim->m_hVoxSound);
                if (const auto* player = dynamic_cast<const CPlayerObj*>(character))
                {
                    ImGui::Text("Player keys: %llu | starts %llu / attempts %llu / failures %llu",
                                (unsigned long long)player->sound_keys, (unsigned long long)player->sound_starts,
                                (unsigned long long)player->sound_attempts, (unsigned long long)player->sound_failures);
                    ImGui::TextWrapped("Last request: %s", player->sound_resource.c_str());
                    if (!player->sound_error.empty())
                        ImGui::TextWrapped("%s", player->sound_error.c_str());
                    voice(player->m_hCurSound);
                    ImGui::Text("Footsteps: %llu speed-suppressed / %llu unmapped",
                                (unsigned long long)player->suppressed_footsteps,
                                (unsigned long long)player->unresolved_footsteps);
                    ImGui::Text("Ground material: %d | foot material: %d | volume: %u | next foot: %s",
                                player->m_eLastSurface, player->footstep_surface, unsigned(player->footstep_container),
                                player->m_bNextFootstepLeft ? "left" : "right");
                    ImGui::Text("AI sound alerts unavailable: %llu",
                                (unsigned long long)player->unavailable_sound_alerts);
                }
                ImGui::TextWrapped("Dry audio only. AI hearing, voice-manager events and reverb remain unavailable.");
            }
            std::vector<NodePose> pose;
            if (sample_pose(model, r->animation.animation, r->animation.time, pose))
            {
                const auto origin = ImGui::GetCursorScreenPos();
                const ImVec2 size{std::max(180.f, ImGui::GetContentRegionAvail().x), 220};
                ImGui::InvisibleButton("Live skeleton front projection", size);
                auto* draw = ImGui::GetWindowDrawList();
                draw->AddRectFilled(origin, {origin.x + size.x, origin.y + size.y}, IM_COL32(12, 17, 23, 255));
                draw->PushClipRect(origin, {origin.x + size.x, origin.y + size.y}, true);
                const float scale = std::min(size.x / 160.f, size.y / 125.f);
                auto project = [&](DVector p)
                { return ImVec2{origin.x + size.x * .5f + p.x * scale, origin.y + size.y * .5f - p.y * scale}; };
                for (size_t i = 0; i < pose.size(); ++i)
                {
                    const auto point = project(pose[i].position);
                    const auto parent = model.nodes[i].parent;
                    if (parent >= 0)
                        draw->AddLine(point, project(pose[parent].position), IM_COL32(130, 200, 220, 255));
                    draw->AddCircleFilled(point, 2, IM_COL32(200, 235, 255, 255));
                    if (anim && anim->hit_spheres_valid)
                        for (unsigned j = 0; j < NUM_STD_NODES; ++j)
                            if (anim->hit_nodes_found[j] && model.nodes[i].name == szNodes[j])
                                draw->AddCircle(point, anim->m_fHitSpheres[j] * scale, IM_COL32(255, 180, 80, 180));
                }
                draw->PopClipRect();
            }
            ImGui::TextWrapped(
                "Local front projection. Orange radii are initialization measurements, not collision geometry.");
            if (ImGui::CollapsingHeader("World node positions and rotations", ImGuiTreeNodeFlags_DefaultOpen))
            {
                if (ImGui::BeginTable("Nodes", 3, ImGuiTableFlags_Borders | ImGuiTableFlags_ScrollY, ImVec2(0, 135)))
                {
                    ImGui::TableSetupColumn("Node");
                    ImGui::TableSetupColumn("World position");
                    ImGui::TableSetupColumn("Quaternion x y z w");
                    ImGui::TableHeadersRow();
                    for (const auto& node : model.nodes)
                    {
                        DVector p;
                        DRotation q;
                        const bool found = game.server.GetModelNodeTransform(r->handle, node.name.c_str(), &p, &q);
                        ImGui::TableNextRow();
                        ImGui::TableNextColumn();
                        ImGui::TextUnformatted(node.name.c_str());
                        ImGui::TableNextColumn();
                        if (found)
                            ImGui::Text("%.2f %.2f %.2f", p.x, p.y, p.z);
                        else
                            ImGui::TextUnformatted("unavailable / ambiguous");
                        ImGui::TableNextColumn();
                        if (found)
                            ImGui::Text("%.2f %.2f %.2f %.2f", q.m_Vec.x, q.m_Vec.y, q.m_Vec.z, q.m_Spin);
                    }
                    ImGui::EndTable();
                }
            }
            if (anim && ImGui::CollapsingHeader("Standard hit-sphere measurements"))
                for (unsigned i = 0; i < NUM_STD_NODES; ++i)
                    ImGui::Text("%s: radius %.3f | %s", szNodes[i], anim->m_fHitSpheres[i],
                                !anim->hit_spheres_valid   ? "unavailable"
                                : anim->hit_nodes_found[i] ? "found"
                                                           : "previous-node fallback");
            if (anim && ImGui::CollapsingHeader("Original animation table (including missing and default slots)"))
                for (auto binding : anim->InspectAnimations())
                {
                    if (binding.index < model.animations.size())
                        ImGui::Text("%s = %u / %s", binding.field, binding.index,
                                    model.animations[binding.index].name.c_str());
                    else
                        ImGui::Text("%s = unavailable (%u)", binding.field, binding.index);
                }
        }
        ImGui::EndTable();
    }
} // namespace OpenBlood2
