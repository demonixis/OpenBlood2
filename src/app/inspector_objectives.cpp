#include "app/inspector.h"
#include "game/original/ExitTrigger.h"
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
    void Inspector::draw_destination(const GameSession& game)
    {
        const auto& d = game.destination_inspection;
        if (!d.attempts)
            return;
        if (!ImGui::TreeNodeEx("Destination preflight (read-only)", ImGuiTreeNodeFlags_DefaultOpen))
            return;
        ImGui::TextWrapped("%s -> %s | requested start: %s | attempt %llu", d.source_world.c_str(),
                           d.destination.c_str(), d.requested_start.c_str(), (unsigned long long)d.attempts);
        ImGui::Text("Source revision %llu / current %llu", (unsigned long long)d.source_revision,
                    (unsigned long long)game.world_revision);
        if (!d.error.empty())
            ImGui::TextWrapped("Scan failed: %s", d.error.c_str());
        if (d.scan_complete)
        {
            ImGui::Text("Parsed %zu models / primary collision %zu triangles / %zu nodes", d.models, d.triangles,
                        d.collision_nodes);
            ImGui::Text("%zu schema checked / %zu editor-only / %zu unknown / %zu invalid properties", d.schema_checked,
                        d.editor_only, d.unknown, d.invalid);
            ImGui::TextWrapped("No destination objects were created. Resources requested by original callbacks, "
                               "difficulty filtering, movers, start triggers and player clearance remain unverified. "
                               "This is not a ready or completed transfer.");
            if (ImGui::TreeNode("Destination source entries"))
            {
                ImGuiListClipper clip;
                clip.Begin((int)d.entries.size());
                while (clip.Step())
                    for (int i = clip.DisplayStart; i < clip.DisplayEnd; ++i)
                    {
                        const auto& e = d.entries[i];
                        ImGui::Text("%d: %s / %s / %s", i, e.class_name.c_str(), e.name.c_str(),
                                    destination_entry_state_name(e.state));
                        if (ImGui::IsItemHovered())
                        {
                            ImGui::BeginTooltip();
                            if (e.has_position)
                                ImGui::Text("Authored position %.2f %.2f %.2f", e.position.x, e.position.y,
                                            e.position.z);
                            if (!e.detail.empty())
                                ImGui::TextWrapped("%s", e.detail.c_str());
                            ImGui::EndTooltip();
                        }
                    }
                ImGui::TreePop();
            }
        }
        ImGui::TreePop();
    }
    void Inspector::draw_level_change(const GameSession& game)
    {
        if (game.pending_level_change)
            ImGui::TextWrapped("NEW_LEVEL queued for the owning frame boundary; current session retained.");
        const auto& d = game.level_change;
        if (!d.attempts || !ImGui::TreeNodeEx("Safe NEW_LEVEL transaction", ImGuiTreeNodeFlags_DefaultOpen))
            return;
        ImGui::Text("Attempts %llu / committed %llu / rejected %llu", (unsigned long long)d.attempts,
                    (unsigned long long)d.committed, (unsigned long long)d.rejected);
        ImGui::TextWrapped("%s -> %s | requested start %s / selected %s%s", d.source_world.c_str(),
                           d.destination.c_str(), d.requested_start.c_str(), d.selected_start.c_str(),
                           d.start_fallback ? " (original fallback)" : "");
        ImGui::Text("Source %llu / destination %llu / current revision %llu", (unsigned long long)d.source_revision,
                    (unsigned long long)d.destination_revision, (unsigned long long)game.world_revision);
        ImGui::Text("Captured %d / loaded %d / entered %d / cleanup %d / source retained %d", d.player_captured,
                    d.world_loaded, d.player_entered, d.cleanup_complete, d.source_retained);
        ImGui::Text("Unsupported %zu / failed %zu / decoded images %zu", d.unknown, d.failed, d.checked_textures);
        ImGui::Text("Omitted missing world models: %zu (no runtime instance)", d.omitted);
        if (d.player_entered)
            ImGui::Text("Destination HP %.1f / armor %.1f / bullets %.1f / keys %zu", d.health, d.armor, d.bullets,
                        d.keys);
        if (!d.error.empty())
            ImGui::TextWrapped("%s", d.error.c_str());
        for (const auto& name : d.unavailable_services)
            ImGui::TextWrapped("Unavailable service: %s", name.c_str());
        if (ImGui::TreeNode("Destination creation results"))
        {
            ImGuiListClipper clip;
            clip.Begin(static_cast<int>(d.entries.size()));
            while (clip.Step())
                for (int i = clip.DisplayStart; i < clip.DisplayEnd; ++i)
                {
                    const auto& row = d.entries[i];
                    ImGui::Text("%s / %s / %s", row.class_name.c_str(), row.name.c_str(), entry_state_name(row.state));
                    if (ImGui::IsItemHovered() && !row.detail.empty())
                        ImGui::SetTooltip("%s", row.detail.c_str());
                }
            ImGui::TreePop();
        }
        ImGui::TextWrapped(
            "Owned historical report. Initialization checks do not certify later gameplay or complete M2.");
        ImGui::TreePop();
    }
    void Inspector::draw_destination_trial(const GameSession& game)
    {
        const auto& d = game.destination_trial;
        if (!d.attempts || !ImGui::TreeNodeEx("Destination initialization rehearsal", ImGuiTreeNodeFlags_DefaultOpen))
            return;
        ImGui::TextWrapped("%s -> %s / requested start %s / selected %s", d.source_world.c_str(), d.destination.c_str(),
                           d.requested_start.c_str(), d.selected_start.c_str());
        ImGui::Text("Attempts %llu / source revision %llu / current %llu", (unsigned long long)d.attempts,
                    (unsigned long long)d.source_revision, (unsigned long long)game.world_revision);
        ImGui::Text("World loaded %s / player entered %s / cleanup %s", d.world_loaded ? "yes" : "no",
                    d.player_entered ? "yes" : "no", d.cleanup_complete ? "complete" : "not completed");
        ImGui::Text("%zu created / %zu sounds / %zu editor / %zu filtered / %zu unknown / %zu failed", d.created,
                    d.sounds, d.editor_only, d.filtered, d.unknown, d.failed);
        ImGui::Text("Omitted missing world models: %zu (no runtime instance)", d.omitted);
        if (d.player_entered)
            ImGui::Text("Restored HP %.1f / armor %.1f / bullets %.1f / keys %zu / active camera %s", d.health, d.armor,
                        d.bullets, d.keys, d.entry_camera_active ? "yes" : "no");
        ImGui::Text("Observed %zu runtime objects / %zu attachments / %zu voices", d.runtime_objects, d.attachments,
                    d.voices);
        ImGui::TextWrapped(
            "The temporary world is discarded after the observed initialization/entry steps. The current session is "
            "retained. "
            "No simulation tick or transfer occurs. Missing classes/services and later callbacks remain unresolved.");
        if (!d.error.empty())
            ImGui::TextWrapped("Failure: %s", d.error.c_str());
        if (ImGui::TreeNode("Rehearsal source results"))
        {
            ImGuiListClipper clip;
            clip.Begin((int)d.entries.size());
            while (clip.Step())
                for (int i = clip.DisplayStart; i < clip.DisplayEnd; ++i)
                {
                    const auto& row = d.entries[i];
                    ImGui::Text("%d: %s / %s / %s", i, row.class_name.c_str(), row.name.c_str(),
                                entry_state_name(row.state));
                    if (ImGui::IsItemHovered() && !row.detail.empty())
                        ImGui::SetTooltip("%s", row.detail.c_str());
                }
            ImGui::TreePop();
        }
        ImGui::TreePop();
    }
    void Inspector::draw_objectives(GameSession& game, ObjectSelection& selection)
    {
        if (ImGui::CollapsingHeader("Original level exits", ImGuiTreeNodeFlags_DefaultOpen))
        {
            const auto& exit = game.client_shell.world_exit;
            ImGui::Text("Received %llu / dead ignored %llu / transfer blocked %llu", (unsigned long long)exit.received,
                        (unsigned long long)exit.ignored_dead, (unsigned long long)exit.blocked);
            ImGui::Text("Queued %llu / completed %llu", (unsigned long long)exit.queued,
                        (unsigned long long)exit.completed);
            ImGui::TextWrapped("Requested world: %s / type %u / server start: %s", exit.request.next_world.c_str(),
                               static_cast<unsigned>(exit.request.type), game.server_shell.start_point_name.c_str());
            ImGui::TextWrapped("%s", exit.error.empty()
                                         ? "No exit error; queued transitions execute at the frame boundary."
                                         : exit.error.c_str());
            draw_destination_trial(game);
            draw_level_change(game);
            draw_destination(game);
            const auto& fields = game.server_shell.player_capture;
            ImGui::Text("Player fields: %llu / %llu captures; %s", (unsigned long long)fields.captures,
                        (unsigned long long)fields.attempts, fields.valid ? "owned subset" : "unavailable");
            if (!fields.error.empty())
                ImGui::TextWrapped("%s", fields.error.c_str());
            if (fields.valid)
            {
                const auto& p = fields.state;
                ImGui::Text("Saved player %s | air %.2f | pitch/yaw %.3f / %.3f",
                            p.name ? (p.name->empty() ? "<empty>" : p.name->c_str()) : "<null>", p.air, p.pitch, p.yaw);
                ImGui::Text("Smell clock offset %.3fs / delay %.3fs | dead %d / blocked %d", p.smell_time_offset,
                            p.smell_delay, p.dead, p.movement_blocked);
                if (ImGui::TreeNode("Saved player field contents"))
                {
                    ImGui::Text("Character %u / skin %u | STR %u SPD %u RES %u MAG %u", p.character, p.skin, p.strength,
                                p.speed, p.resistance, p.magic);
                    ImGui::Text("Move state %u | last crouch %d / forced crouch %d", p.move_state, p.last_crouch,
                                p.forced_crouch);
                    ImGui::TextWrapped("Trigger string: %s", p.trigger ? p.trigger->c_str() : "<null>");
                    ImGui::TextWrapped(
                        "Selected original value block. The NEW_LEVEL transaction combines this with "
                        "engine/model state and restored client association. Power timers remain unavailable.");
                    ImGui::TreePop();
                }
            }
            const auto& saved = game.server_shell.inventory_capture;
            ImGui::Text("Inventory capture: %llu / %llu attempts; %s", (unsigned long long)saved.captures,
                        (unsigned long long)saved.attempts, saved.valid ? "owned snapshot" : "unavailable");
            if (!saved.error.empty())
                ImGui::TextWrapped("%s", saved.error.c_str());
            if (saved.valid)
            {
                const auto& inventory = saved.state;
                ImGui::Text("Saved strength %u / magic %u / infinite %d / damage x%.2f", inventory.strength,
                            inventory.magic, inventory.infinite_ammo, inventory.damage_multiplier);
                ImGui::Text("Selected slot %u / %zu keys (discarded by original keep-alive Load)",
                            inventory.current_slot, inventory.keys.size());
                if (ImGui::TreeNode("Saved inventory contents"))
                {
                    for (unsigned i = 0; i < inventory.right.size(); ++i)
                        ImGui::Text("Slot %u: right %u / left %u", i, inventory.right[i], inventory.left[i]);
                    for (unsigned i = 0; i < inventory.ammo.size(); ++i)
                        ImGui::Text("Ammo %u: %.2f", i, inventory.ammo[i]);
                    if (inventory.medkit_charge)
                        ImGui::Text("Saved medkit: %u charges (retained by keep-alive)", *inventory.medkit_charge);
                    else
                        ImGui::TextUnformatted("Saved medkit: absent");
                    if (inventory.flashlight)
                        ImGui::Text("Saved flashlight: %u / %.3f seconds; restores inactive",
                                    inventory.flashlight->count, inventory.flashlight->remaining);
                    ImGui::Text("Saved binoculars: %s", inventory.binoculars ? "present; restores inactive" : "absent");
                    for (const auto& key : inventory.keys)
                        ImGui::Text("Key %s: %u uses", key.name.c_str(), key.uses);
                    ImGui::TreePop();
                }
            }
            const auto& damage = game.server_shell.damage_capture;
            ImGui::Text("Damage capture: %llu / %llu attempts; %s", (unsigned long long)damage.captures,
                        (unsigned long long)damage.attempts, damage.valid ? "owned snapshot" : "unavailable");
            if (!damage.error.empty())
                ImGui::TextWrapped("%s", damage.error.c_str());
            if (damage.valid)
            {
                const auto& d = damage.state;
                ImGui::Text("Saved HP %.2f / %.2f / mega %.2f; armor %.2f / %.2f / necro %.2f", d.health, d.max_health,
                            d.max_mega_health, d.armor, d.max_armor, d.max_necro_armor);
                ImGui::Text("Death HP %.2f / delay %.3fs / raw dead %d / nigh-invulnerable %d", d.death_health,
                            d.death_delay, d.dead, d.nigh_invulnerable);
                if (ImGui::TreeNode("Saved damage contents"))
                {
                    ImGui::Text("Mass %.2f / resistance %.3f / physics %d / god %d / trigger only %d", d.mass,
                                d.resistance, d.apply_damage_physics, d.god_mode, d.trigger_only);
                    ImGui::Text("Last type %u / location byte %u / amount %.2f / fraction %.3f",
                                static_cast<unsigned>(d.last_type), d.last_location, d.last_amount, d.last_percent);
                    ImGui::Text("Direction %.2f %.2f %.2f", d.last_direction.x, d.last_direction.y, d.last_direction.z);
                    ImGui::TextWrapped("Damage %s -> %s / death %s -> %s", d.damage_target.c_str(),
                                       d.damage_message.c_str(), d.death_target.c_str(), d.death_message.c_str());
                    ImGui::TextWrapped("Spawn %s / velocity %.2f %.2f %.2f", d.spawn_object.c_str(), d.spawn_velocity.x,
                                       d.spawn_velocity.y, d.spawn_velocity.z);
                    ImGui::TextUnformatted("HP >= 1000000 restores CanDamage=false (original sentinel).");
                    ImGui::TreePop();
                }
            }
            ImGui::TextWrapped("Exit snapshots remain partial. The Session tab supports same-world player restoration; "
                               "complete timers/client saves and safe destination transfer remain unavailable.");
            for (auto id : game.objects.snapshot())
            {
                const auto* record = game.objects.get(id);
                const auto* trigger = record ? dynamic_cast<const ExitTrigger*>(record->game.get()) : nullptr;
                if (!trigger)
                    continue;
                ImGui::PushID(static_cast<int>(id.slot));
                if (ImGui::Selectable(record->state.m_Name, selection.id == id))
                    selection.id = id;
                ImGui::TextWrapped("%s / %s; next %s / start %s / type %u", trigger->active ? "active" : "inactive",
                                   record->state.m_Flags & FLAG_VISIBLE ? "visible" : "hidden",
                                   trigger->exit.next_world.c_str(), trigger->start_point.c_str(),
                                   static_cast<unsigned>(trigger->exit.type));
                ImGui::Text("Broadcast attempts %llu / queued %llu / result %d", (unsigned long long)trigger->attempts,
                            (unsigned long long)trigger->deliveries, trigger->last_delivery);
                ImGui::Text("Player %d / object %d / relay %d / delay %.2fs / rejected %llu", trigger->player_activate,
                            trigger->object_activate, trigger->relay_activate, trigger->max_delay,
                            (unsigned long long)trigger->rejected);
                ImGui::BeginDisabled(!game.local_client || !trigger->player_activate);
                if (ImGui::Button("Send exit TRIGGER as player"))
                {
                    trigger_request = id;
                    trigger_command = "TRIGGER";
                }
                ImGui::EndDisabled();
                ImGui::PopID();
            }
            ImGui::Separator();
        }
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
