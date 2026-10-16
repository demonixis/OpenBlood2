#include "game/original/AI_Mgr.h"
#include "game/original/Corpse.h"
#include "game/original/CultistAI.h"
#include "inspector.h"
#include <algorithm>
#include <imgui.h>
namespace OpenBlood2
{
    void Inspector::draw_skeleton(GameSession& game, ObjectSelection& selection)
    {
        ImGui::TextWrapped(
            "Original hit spheres at current ABC node poses. Initialized NPCs use them for hit "
            "localization. Ordinary corpse animation is integrated; fire/gibs and blood-pool FX remain unavailable.");
        if (ImGui::CollapsingHeader("Live attachments"))
        {
            ImGui::TextWrapped("Held models follow current parent nodes. Physical/nested attachments and dropped "
                               "weapon attachment chains remain unavailable.");
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
        if (r && r->model)
        {
            const auto hidden =
                std::count_if(r->hidden_nodes.begin(), r->hidden_nodes.end(), [](auto value) { return value != 0; });
            ImGui::Text("Model nodes: %zu total / %zu hidden (geometry and hit candidates)", r->model->nodes.size(),
                        static_cast<std::size_t>(hidden));
        }
        const auto* target_ai = r ? dynamic_cast<const AI_Mgr*>(r->game.get()) : nullptr;
        const auto* character = r ? dynamic_cast<const CBaseCharacter*>(r->game.get()) : nullptr;
        if (const auto* corpse = r ? dynamic_cast<const CCorpse*>(r->game.get()) : nullptr)
        {
            const auto& c = *corpse;
            ImGui::TextUnformatted("Original CCorpse (independent lifetime)");
            ImGui::Text("Setup %s | animation %s | updates %llu", c.configured ? "ready" : "FAILED",
                        c.animation_done ? "done" : "running", (unsigned long long)c.updates);
            ImGui::Text("Clip %u | time %.3f | removal deadline %.3f", r->animation.animation,
                        double(r->animation.time), c.remove_time);
            ImGui::Text("Raw dimensions %.2f %.2f %.2f | visual scale %.2f %.2f %.2f", r->dimensions.x, r->dimensions.y,
                        r->dimensions.z, r->state.m_Scale.x, r->state.m_Scale.y, r->state.m_Scale.z);
            ImGui::Text("Sound keys %llu / starts %llu | last voice %llu", (unsigned long long)c.sound_attempts,
                        (unsigned long long)c.sound_starts, (unsigned long long)c.last_sound);
            ImGui::TextWrapped("%s | %s", c.sound_resource.c_str(), c.sound_error.c_str());
            ImGui::Text("Blood-pool ray %d | request %s", int(c.pool_result), c.pool_requested ? "yes" : "no");
            ImGui::TextWrapped("%s", c.pool_error.c_str());
            ImGui::Text("Damage refused %llu | limb-loss requested %s", (unsigned long long)c.damage_refusals,
                        c.limb_loss ? "yes" : "no");
            ImGui::TextWrapped("Corpse damage, limb hiding, gibs, fire and splat FX remain unavailable. %s %s",
                               c.error.c_str(), c.damage_error.c_str());
        }
        if (const auto* player = character ? dynamic_cast<const CPlayerObj*>(character) : nullptr)
        {
            ImGui::Text("Player restore initialization %d / selected fields loaded %d / associated %d",
                        player->restore_initialization, player->restore_fields_loaded, player->GetClient() != nullptr);
            ImGui::Text("Load model/flags/velocity tail: %s", player->restore_model_loaded ? "applied" : "not applied");
            if (!player->restore_model_error.empty())
                ImGui::TextWrapped("%s", player->restore_model_error.c_str());
            ImGui::Text("Air %.2f | smell timestamp %.3f / delay %.3f", player->m_fAirLevel, player->m_fSmellTime,
                        player->m_fSmellDelay);
            if (player->restore_initialization)
                ImGui::TextWrapped(
                    "Selected restore operations only; body, scheduling and client restoration remain incomplete.");
        }
        if (target_ai && ImGui::CollapsingHeader("Original NPC death"))
        {
            const auto& a = *target_ai;
            ImGui::Text("Starts %llu | corpses %llu | slot %d / clip %u", (unsigned long long)a.death_starts,
                        (unsigned long long)a.corpses_created, a.death_slot, a.m_nCorpseType);
            ImGui::Text("Frame damage fraction %.3f | optional kill-voice requests %llu (unavailable)",
                        a.m_damage.m_fLastDamagePercent, (unsigned long long)a.kill_voice_requests);
            ImGui::TextWrapped("%s", a.death_error.c_str());
        }
        if (target_ai && target_ai->npc_type != AI_Mgr::NPCType::Diagnostic &&
            ImGui::CollapsingHeader("Original NPC lifecycle", ImGuiTreeNodeFlags_DefaultOpen))
        {
            const auto& a = *target_ai;
            ImGui::Text("Initialization %s | update %s", a.runtime_initialized ? "complete" : "incomplete",
                        a.runtime_failed ? "FAILED / suspended" : "ready");
            ImGui::Text("AI_Update %llu | camera pauses %llu | wake delays %llu", (unsigned long long)a.runtime_updates,
                        (unsigned long long)a.runtime_camera_pauses, (unsigned long long)a.runtime_desyncs);
            ImGui::Text("Activity %u | pings %llu | timeout %.3f", r->activity, (unsigned long long)r->activation_pings,
                        r->deactivate_at);
            ImGui::Text("State %d | phase %d | targeted %s | target losses %llu", a.m_nState, a.Metacmd,
                        a.m_bHasTargeted ? "yes" : "no", (unsigned long long)a.runtime_target_losses);
            ImGui::Text("HP %.2f / %.2f | damage factor %.3f", a.m_damage.m_fHitPoints, a.m_damage.m_fMaxHitPoints,
                        a.m_InventoryMgr.damage_multiplier);
            ImGui::Text("Death HP %.2f / delay %.3fs / raw dead %d / effective dead %d / nigh-invulnerable %d",
                        a.m_damage.m_fDeathHitPoints, a.m_damage.m_fDeathDelay, a.m_damage.m_bDead, a.m_damage.IsDead(),
                        a.m_damage.m_bNighInvulnerable);
            ImGui::TextWrapped("Activation uses replacement center-ray visibility, not retail PVS. Remaining states "
                               "may fail explicitly.");
            if (!a.runtime_error.empty())
                ImGui::TextWrapped("Failure: %s", a.runtime_error.c_str());
        }
        if (target_ai && ImGui::CollapsingHeader("Original NPC hit and recoil", ImGuiTreeNodeFlags_DefaultOpen))
        {
            const auto& a = *target_ai;
            const auto& d = a.m_damage;
            const auto& h = d.hit;
            ImGui::Text("Localization %s | accepted packets %llu | queries %llu",
                        d.ai_hit_localization ? "enabled" : "disabled", (unsigned long long)d.accepted_damage,
                        (unsigned long long)d.hit_queries);
            ImGui::Text("Last query %s | raw node %d | region %d | side %d", h.valid ? "valid" : "unavailable/none",
                        h.raw_node, h.node, h.side);
            ImGui::Text("Random fallback %s | missing later nodes %u | distance %.3f | multiplier %.2f",
                        h.random_fallback ? "yes" : "no", h.missing_nodes, h.distance, h.multiplier);
            ImGui::Text("Head kill %s | injured leg %d | accumulated damage %.3f", h.head_kill ? "yes" : "no",
                        a.m_nInjuredLeg, d.m_fLastDamageAmount);
            ImGui::Text("Reactions %llu | recoil starts %llu | selected node %d / slot %d",
                        (unsigned long long)a.damage_reactions, (unsigned long long)a.recoil_starts, a.recoil_node,
                        a.recoil_slot);
            if (!h.error.empty())
                ImGui::TextWrapped("Localization failure: %s", h.error.c_str());
            if (!a.damage_reaction_error.empty())
                ImGui::TextWrapped("Reaction failure: %s", a.damage_reaction_error.c_str());
            ImGui::TextWrapped(
                "Visible-node hit spheres; no limb hiding. Death/corpses/gibs and NPC impulses remain unavailable.");
        }
        if (target_ai && ImGui::CollapsingHeader("Original escape and guard", ImGuiTreeNodeFlags_DefaultOpen))
        {
            const auto& a = *target_ai;
            ImGui::TextWrapped(
                "Original Civilian/Cultist flee/hide and Guard. Diagnostic drivers are not full NPC AI_Update.");
            const auto* tracked = game.objects.get(a.GetTrackedObject());
            ImGui::TextWrapped("Tracked: %s", tracked ? tracked->state.m_Name : "none/pending/removed");
            ImGui::Text("Sampled object position: %s", tracked && a.tracked_position_valid ? "yes (cached)" : "no");
            ImGui::Text("Track position (%.2f %.2f %.2f)", a.m_vTrackObjPos.x, a.m_vTrackObjPos.y, a.m_vTrackObjPos.z);
            ImGui::Text("Guard position (%.2f %.2f %.2f)", a.m_vGuardPos.x, a.m_vGuardPos.y, a.m_vGuardPos.z);
            ImGui::Text("Hint dot %.3f | roll speed %.2f | escape/hide updates %llu", a.escape_hint_dot, a.m_fRollSpeed,
                        (unsigned long long)a.escape_updates);
            ImGui::Text("Track assignments %llu | invalidations %llu", (unsigned long long)a.tracked_changes,
                        (unsigned long long)a.tracked_invalidations);
        }
        if (target_ai && ImGui::CollapsingHeader("Original navigation and recovery", ImGuiTreeNodeFlags_DefaultOpen))
        {
            const auto& a = *target_ai;
            ImGui::TextWrapped("Original jump/crouch/crawl and selected route states. Smell following "
                               "and shade-based detours remain unavailable.");
            ImGui::Text("Last driven state %d | current %d | phase %d", a.navigation_state, a.m_nState, a.Metacmd);
            ImGui::Text("Updates %llu | arrivals %llu | failures %llu", (unsigned long long)a.navigation_updates,
                        (unsigned long long)a.navigation_arrivals, (unsigned long long)a.navigation_failures);
            ImGui::Text("ToPos destination (%.2f %.2f %.2f)", a.m_vDestPos.x, a.m_vDestPos.y, a.m_vDestPos.z);
            ImGui::Text("Last distance %.3f / threshold %.3f | jump speed %.2f", a.navigation_distance,
                        a.navigation_threshold, a.m_fJumpSpeed);
            ImGui::Text("Remaining points %zu | failed-plan guard %s", a.m_MoveObj.path().size(),
                        a.navigation_plan_failed ? "blocked" : "clear");
            DVector next;
            if (a.m_MoveObj.GetNextPathPoint(next))
                ImGui::Text("Next point (%.2f %.2f %.2f)", next.x, next.y, next.z);
            if (!a.navigation_error.empty())
                ImGui::TextWrapped("Last failure: %s", a.navigation_error.c_str());
        }
        if (target_ai && ImGui::CollapsingHeader("Original autonomous locomotion", ImGuiTreeNodeFlags_DefaultOpen))
        {
            const auto& a = *target_ai;
            ImGui::TextWrapped("Original walk/run/moving fire and selected Cultist attack sequencing. Remaining "
                               "shade recovery and death actions remain unavailable.");
            ImGui::Text("Command %d | calls %llu | failures %llu", a.locomotion_command,
                        (unsigned long long)a.locomotion_calls, (unsigned long long)a.locomotion_failures);
            ImGui::Text("Obstruction %s | speed %.3f | cached vertical change %.4f | checks %llu",
                        a.obstructed ? "yes" : "no", a.obstruction_velocity, a.obstruction_vertical,
                        (unsigned long long)a.obstruction_checks);
            ImGui::Text("Block flags 0x%02x | nine-ray queries %llu", a.block_flags,
                        (unsigned long long)a.obstacle_queries);
            auto ray = [&](const char* name, const AI_Mgr::ObstacleRay& q)
            {
                const auto* hit = game.objects.get(q.hit.object);
                const char* status = q.result == SegmentResult::Hit       ? "hit"
                                     : q.result == SegmentResult::Miss    ? "miss"
                                     : q.result == SegmentResult::Invalid ? "invalid"
                                                                          : "unavailable/not queried";
                ImGui::Text("%s: %s | (%.1f %.1f %.1f) -> (%.1f %.1f %.1f)", name, status, q.from.x, q.from.y, q.from.z,
                            q.to.x, q.to.y, q.to.z);
                if (q.result == SegmentResult::Hit)
                    ImGui::Text("Hit %s | polygon %u",
                                hit            ? hit->state.m_Name
                                : q.hit.object ? "removed object"
                                               : "world",
                                q.hit.polygon);
            };
            const char* names[]{"Right upper",  "Right middle", "Right lower", "Center upper", "Center middle",
                                "Center lower", "Left upper",   "Left middle", "Left lower"};
            if (ImGui::TreeNode("Nine obstruction rays"))
            {
                for (unsigned i = 0; i < a.obstacle_rays.size(); ++i)
                    ray(names[i], a.obstacle_rays[i]);
                ImGui::TreePop();
            }
            ImGui::Text("Last ledge %s | sampled on last call %s | time %.3f | queries %llu", a.ledge ? "yes" : "no",
                        a.ledge_sampled ? "yes" : "no", a.last_ledge_check, (unsigned long long)a.ledge_queries);
            ray("Ledge", a.ledge_ray);
            if (const auto* cultist = dynamic_cast<const CultistAI*>(target_ai))
                ImGui::Text("Cultist attack: updates %llu | distance %.2f / threshold %.2f | height %.2f",
                            (unsigned long long)cultist->attack_updates, cultist->attack_distance,
                            cultist->attack_threshold, cultist->attack_height);
            if (!a.locomotion_error.empty())
                ImGui::TextWrapped("Last failure: %s", a.locomotion_error.c_str());
        }
        if (target_ai && ImGui::CollapsingHeader("Original AI script interpreter", ImGuiTreeNodeFlags_DefaultOpen))
        {
            const auto& a = *target_ai;
            const char* outcomes[]{"None", "Prepared", "Running", "Completed", "State changed", "Failed"};
            ImGui::TextWrapped("Explicit script-state driver. Production SCRIPT triggers also enter AI_Update.");
            ImGui::Text("%s | flags %u | next index %zu | remaining %zu",
                        outcomes[static_cast<unsigned>(a.script_outcome)], a.m_dwScriptFlags, a.script_index,
                        a.ScriptCommands().size());
            const auto& cmd = a.CurrentScriptCommand();
            ImGui::TextWrapped("Current: %s / %s", cmd.name.empty() ? "<exhausted>" : cmd.name.c_str(),
                               cmd.argument.c_str());
            ImGui::Text("Wait deadline %.3f | select next %s", a.script_wait_end, a.m_bUpdateScriptCmd ? "yes" : "no");
            ImGui::Text("Updates %llu | selections %llu | sounds %llu | failures %llu",
                        (unsigned long long)a.script_updates, (unsigned long long)a.script_selections,
                        (unsigned long long)a.script_sounds, (unsigned long long)a.script_failures);
            for (const auto& instruction : a.ScriptCommands())
                ImGui::TextWrapped("%s %s%s", instruction.name.c_str(), instruction.argument.c_str(),
                                   instruction.command == AI_Mgr::ScriptCommand::Done ? " (DONE mapping)" : "");
            if (!a.script_error.empty())
                ImGui::TextWrapped("Failed: %s", a.script_error.c_str());
        }
        if (target_ai && ImGui::CollapsingHeader("Original script movement", ImGuiTreeNodeFlags_DefaultOpen))
        {
            const auto& a = *target_ai;
            const char* commands[]{"None", "FOLLOWPATH", "MOVETOOBJECT"};
            const char* outcomes[]{"None", "Selected", "Moving", "Arrived", "Completed", "Failed"};
            ImGui::TextWrapped("Original movement helpers; the enclosing driver owns command sequencing. Runtime "
                               "dispatch is enabled for Cultist/Civilian.");
            ImGui::Text("%s | %s | mode %s | Metacmd %d", commands[static_cast<unsigned>(a.script_motion_command)],
                        outcomes[static_cast<unsigned>(a.script_motion_outcome)],
                        a.m_eScriptMovement == AI_Mgr::SM_WALK  ? "WALK"
                        : a.m_eScriptMovement == AI_Mgr::SM_RUN ? "RUN"
                                                                : "INVALID",
                        a.Metacmd);
            ImGui::TextWrapped("Argument: %s", a.script_motion_name.c_str());
            ImGui::Text("Speeds %.2f / %.2f | injured node %d | done %s", a.m_fWalkSpeed, a.m_fRunSpeed,
                        a.m_nInjuredLeg, a.m_bUpdateScriptCmd ? "yes" : "no");
            ImGui::Text("Distance %.3f / arrival threshold %.3f", a.script_motion_distance, a.script_motion_threshold);
            const auto d = a.script_motion_destination, v = a.script_motion_velocity;
            ImGui::Text("Destination %.2f %.2f %.2f", d.x, d.y, d.z);
            ImGui::Text("Requested velocity %.2f %.2f %.2f", v.x, v.y, v.z);
            ImGui::Text("Updates %llu / failed %llu | velocity writes %llu",
                        (unsigned long long)a.script_motion_updates, (unsigned long long)a.script_motion_failures,
                        (unsigned long long)a.velocity_commands);
            ImGui::Text("Arrivals %llu | trigger deliveries %llu | removals %llu | remaining %zu",
                        (unsigned long long)a.path_arrivals, (unsigned long long)a.path_deliveries,
                        (unsigned long long)a.path_removals, a.ScriptPath().size());
            for (const auto& p : a.ScriptPath())
            {
                const auto* source = game.objects.get(p.source);
                ImGui::TextWrapped("%s (%.2f %.2f %.2f), source %s; target %s / message %s", p.name.c_str(),
                                   p.position.x, p.position.y, p.position.z,
                                   source && game.server.IsObjectAlive(p.source) ? "live" : "removed",
                                   p.action_target ? p.action_target->c_str() : "<self>",
                                   p.action_message ? p.action_message->c_str() : "<none>");
            }
            if (!a.script_motion_error.empty())
                ImGui::TextWrapped("Unavailable: %s", a.script_motion_error.c_str());
        }
        if (character && ImGui::CollapsingHeader("Original CMovement cache / path", ImGuiTreeNodeFlags_DefaultOpen))
        {
            const auto& m = character->m_MoveObj;
            const char* outcomes[]{"None",
                                   "Direct path",
                                   "Detour path",
                                   "Blocked / partial path",
                                   "Invalid",
                                   "Unavailable",
                                   "Budget exhausted / partial path",
                                   "Clear side rays",
                                   "Obstructed side ray"};
            ImGui::Text("Initialized: %s | sampled: %s | %s", m.initialized ? "yes" : "no", m.sampled ? "yes" : "no",
                        outcomes[static_cast<unsigned>(m.outcome)]);
            const auto p = m.GetPos(), last = m.GetLastPos(), f = m.GetForwardVector();
            ImGui::Text("Cache %.2f %.2f %.2f | previous %.2f %.2f %.2f", p.x, p.y, p.z, last.x, last.y, last.z);
            ImGui::Text("Forward %.2f %.2f %.2f | samples %llu / failed %llu", f.x, f.y, f.z,
                        (unsigned long long)m.samples, (unsigned long long)m.sample_failures);
            ImGui::Text("Plans %llu | failures %llu | rays %u / iterations %u", (unsigned long long)m.calculations,
                        (unsigned long long)m.failures, m.rays, m.iterations);
            ImGui::Text("Remaining points %zu | source accumulated %u", m.path().size(), m.added_points);
            ImGui::Text("Requested destination %.2f %.2f %.2f", m.requested_destination.x, m.requested_destination.y,
                        m.requested_destination.z);
            for (std::size_t i = 0; i < m.path().size(); ++i)
                ImGui::Text("%zu: %.2f %.2f %.2f", i, m.path()[i].x, m.path()[i].y, m.path()[i].z);
            if (!m.error.empty())
                ImGui::TextWrapped("%s", m.error.c_str());
            ImGui::TextWrapped("Two side rays only; partial paths are not usable success. Planning does not move an "
                               "actor. Player cache remains uninitialized, as in source. Shade-based "
                               "position selection remains unavailable.");
        }
        if (target_ai && ImGui::CollapsingHeader("Original AI commands / hearing", ImGuiTreeNodeFlags_DefaultOpen))
        {
            const auto& ai = *target_ai;
            const char* outcomes[]{"None",
                                   "Ignored",
                                   "Heard location",
                                   "Turned",
                                   "State selected",
                                   "Target selected",
                                   "Animation started",
                                   "Sound started",
                                   "Relay delivered",
                                   "Failed"};
            ImGui::Text("Result: %s | commands %llu / failures %llu",
                        outcomes[static_cast<unsigned>(ai.trigger_outcome)], (unsigned long long)ai.trigger_commands,
                        (unsigned long long)ai.trigger_failures);
            ImGui::TextWrapped("Last command: %s", ai.trigger_text.c_str());
            ImGui::Text("Hearing %.1f | events %llu / reactions %llu / ignored %llu", ai.m_fHearingDist,
                        (unsigned long long)ai.hearing_events, (unsigned long long)ai.hearing_reactions,
                        (unsigned long long)ai.hearing_ignored);
            const auto* sender = game.objects.get(ai.heard_sender);
            ImGui::Text("Sound %d | sender %s | distance %.2f", ai.heard_sound,
                        sender && sender->life == Life::Alive ? sender->state.m_Name : "none / removed",
                        ai.heard_distance);
            ImGui::Text("Heard position %.2f %.2f %.2f", ai.heard_position.x, ai.heard_position.y, ai.heard_position.z);
            ImGui::Text("Relay: %s | delivered %llu", ai.trigger_relay_target.c_str(),
                        (unsigned long long)ai.trigger_relays);
            if (!ai.trigger_error.empty())
                ImGui::TextWrapped("%s", ai.trigger_error.c_str());
            ImGui::TextWrapped("State selection does not execute missing search/dodge/attack actions. Complete combat "
                               "and missing recovery/death actions remain unavailable.");
        }
        if (target_ai && ImGui::CollapsingHeader("Original AI weapon actions", ImGuiTreeNodeFlags_DefaultOpen))
        {
            const auto& ai = *target_ai;
            const auto* weapon = ai.m_InventoryMgr.CurrentWeapon();
            ImGui::TextWrapped("Original standing fire and weapon choice, called by production Cultist updates. "
                               "Complete combat reactions and death remain unavailable.");
            ImGui::Text("Weapon %u | last selection: alternative %u / distance %.2f", weapon ? weapon->data.m_nType : 0,
                        ai.best_weapon, ai.weapon_distance);
            ImGui::Text("Attempts %llu | shots %llu | dry %llu | blocked %llu | failures %llu",
                        (unsigned long long)ai.fire_calls, (unsigned long long)ai.fire_shots,
                        (unsigned long long)ai.fire_dry, (unsigned long long)ai.fire_blocked,
                        (unsigned long long)ai.fire_failures);
            const char* result = "invalid";
            switch (ai.fire_result)
            {
                case FireResult::Fired:
                    result = "fired";
                    break;
                case FireResult::Dry:
                    result = "dry (accepted by source AI)";
                    break;
                case FireResult::Blocked:
                    result = "blocked";
                    break;
                case FireResult::Unavailable:
                    result = "unavailable";
                    break;
                case FireResult::DeliveryFailed:
                    result = "damage delivery failed";
                    break;
                case FireResult::Invalid:
                    break;
            }
            ImGui::Text("Last attempt: %s", ai.fire_calls ? result : "none");
            ImGui::Text("Origin %s: %.2f %.2f %.2f | cadence %.3f / last %.3f",
                        ai.fire_gun_node ? "r_gun" : "body fallback", ai.fire_origin.x, ai.fire_origin.y,
                        ai.fire_origin.z, ai.m_fAttackLoadTime, ai.m_fLoadTimeStart);
            ImGui::Text("Clearance: %s | rays %u | head retry %s", ai_clear_outcome_name(ai.clear_shot.outcome),
                        ai.clear_shot.rays, ai.clear_shot.retried_head ? "yes" : "no");
            if (weapon)
            {
                const auto& shot = weapon->last_shot;
                ImGui::Text("Ammo %.0f | damage %.2f | shot seed %u",
                            ai.m_InventoryMgr.GetAmmoCount(weapon->data.m_nAmmoType), shot.damage,
                            unsigned(shot.request.seed));
                ImGui::TextWrapped("Damage message: %s",
                                   shot.damage_delivered ? "delivered (not a health assertion)" : "none");
            }
            if (!ai.fire_error.empty())
                ImGui::TextWrapped("Failed: %s", ai.fire_error.c_str());
        }
        if (target_ai && ImGui::CollapsingHeader("Original AI state actions", ImGuiTreeNodeFlags_DefaultOpen))
        {
            const auto& ai = *target_ai;
            ImGui::TextWrapped("Original decisions and registered Cultist/Civilian updates. Remaining actions and "
                               "full combat/death remain unavailable.");
            ImGui::Text("State %d | last %d | requested %d | dispatched %d", ai.m_nState, ai.m_nLastState,
                        ai.requested_state, ai.dispatched_state);
            ImGui::Text("Authored %s | metacmd %d / current %d | animating %s", ai.m_szAIState.c_str(), ai.Metacmd,
                        ai.m_nCurMetacmd, ai.m_bAnimating ? "yes" : "no");
            ImGui::Text("Transitions %llu | dispatches %llu | failures %llu", (unsigned long long)ai.state_changes,
                        (unsigned long long)ai.state_dispatches, (unsigned long long)ai.state_failures);
            ImGui::Text("Idle deadline %.2f | has targeted %s | trapped %d", ai.m_fWaitForIdleTime,
                        ai.m_bHasTargeted ? "yes" : "no", ai.m_nTrapped);
            ImGui::Text("Clip %u / requested %u | choice %d | starts %llu / finishes %llu", ai.m_nCurAnimation,
                        ai.requested_animation, ai.animation_choice, (unsigned long long)ai.animation_starts,
                        (unsigned long long)ai.animation_finishes);
            ImGui::Text("Cached half extents %.2f %.2f %.2f | scale %.2f %.2f %.2f", ai.m_vDims.x, ai.m_vDims.y,
                        ai.m_vDims.z, ai.m_vScale.x, ai.m_vScale.y, ai.m_vScale.z);
            if (!ai.state_error.empty())
                ImGui::TextWrapped("Unavailable/failed: %s", ai.state_error.c_str());
        }
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
                               "or attack-state scheduler yet.");
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
            ImGui::TextWrapped("Original target commands. Full state-driven updates and tracking remain unavailable.");
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
                    ImGui::Text("AI sound alert failures: %llu", (unsigned long long)player->unavailable_sound_alerts);
                    ImGui::Text("Alert attempts %llu | last messages delivered %u / eligible %u | %s",
                                (unsigned long long)player->sound_alerts, player->sound_alert.delivered,
                                player->sound_alert.eligible, player->sound_alert.complete ? "complete" : "incomplete");
                    if (!player->sound_alert.error.empty())
                        ImGui::TextWrapped("%s", player->sound_alert.error.c_str());
                }
                ImGui::TextWrapped("Dry audio only. AI hearing is separate from playback; voice-manager events and "
                                   "reverb remain unavailable.");
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
                    const bool hidden = r->hidden_nodes.size() == model.nodes.size() && r->hidden_nodes[i];
                    draw->AddCircleFilled(point, 2,
                                          hidden ? IM_COL32(130, 130, 130, 255) : IM_COL32(200, 235, 255, 255));
                    if (anim && anim->hit_spheres_valid)
                        for (unsigned j = 0; j < NUM_STD_NODES; ++j)
                            if (anim->hit_nodes_found[j] && model.nodes[i].name == szNodes[j])
                                draw->AddCircle(point, anim->m_fHitSpheres[j] * scale, IM_COL32(255, 180, 80, 180));
                }
                draw->PopClipRect();
            }
            ImGui::TextWrapped("Local front projection includes hidden nodes (gray). Orange radii are initialization "
                               "measurements, not collision geometry.");
            if (ImGui::CollapsingHeader("World node positions and rotations", ImGuiTreeNodeFlags_DefaultOpen))
            {
                if (ImGui::BeginTable("Nodes", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_ScrollY, ImVec2(0, 135)))
                {
                    ImGui::TableSetupColumn("Node");
                    ImGui::TableSetupColumn("Visibility");
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
                        DBOOL hidden = DFALSE;
                        const auto visibility =
                            game.server.GetModelNodeHideStatus(r->handle, node.name.c_str(), &hidden);
                        ImGui::TextUnformatted(visibility == DE_OK ? (hidden ? "hidden" : "visible") : "unavailable");
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
