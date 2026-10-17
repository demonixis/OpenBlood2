#include "app/inspector.h"
#include "game/original/AI_Mgr.h"
#include "game/original/AmmoPickups.h"
#include "game/original/HandWeaponModel.h"
#include <imgui.h>
namespace OpenBlood2
{
    void Inspector::draw_pickups(GameSession& game, ObjectSelection& selection)
    {
        ImGui::TextWrapped(
            "Original single-player pickups and spatial collection sounds. Recent results include keys "
            "and remain visible after collection. Medkit: H uses collected charges (release Shift first). "
            "Timed bonuses and pickup FX remain incomplete.");
        if (!ImGui::BeginTable("Pickup inspection", 2, ImGuiTableFlags_Resizable))
            return;
        ImGui::TableSetupColumn("Objects", ImGuiTableColumnFlags_WidthFixed, 250);
        ImGui::TableSetupColumn("State");
        ImGui::TableNextColumn();
        ImGui::BeginChild("Pickup list", {0, 100});
        for (auto id : game.objects.snapshot())
        {
            const auto* r = game.objects.get(id);
            if (!r || !dynamic_cast<const PickupObject*>(r->game.get()))
                continue;
            if (!game.objects.get(pickup_selection))
                pickup_selection = id;
            const auto label = std::string(r->state.m_Name) + " / " + r->type->name +
                               ((r->state.m_Flags & FLAG_VISIBLE) ? "" : " [hidden]") +
                               (r->activity == OBJSTATE_ACTIVE ? "" : " [inactive]") +
                               (r->life == Life::Pending ? " [pending]" : "");
            ImGui::PushID(static_cast<int>(id.slot));
            if (ImGui::Selectable(label.c_str(), pickup_selection == id))
                pickup_selection = selection.id = id;
            ImGui::PopID();
        }
        ImGui::EndChild();
        ImGui::Text("Ammo list: %u", AmmoPickup::m_dwNumPickups);
        if (auto* player = game.server_shell.Player(game.local_client))
        {
            ImGui::TextUnformatted("Player ammo / capacity");
            for (unsigned i = 1; i <= 8; ++i)
                ImGui::Text("%u: %.0f / %.0f", i, player->m_InventoryMgr.GetAmmoCount(i),
                            player->m_InventoryMgr.MaxAmmo(i));
            if (const auto& item = player->m_InventoryMgr.flashlight)
            {
                const auto& fx = game.client_shell.flashlight;
                ImGui::Text("Flashlight (L): %u / 100, %.3f seconds, active %d, child alive %d", item->count,
                            item->remaining, item->active, game.server.IsObjectAlive(item->light));
                ImGui::Text("Ray %d / visible %d / distance %.2f / radius %.2f / intensity %.3f", int(fx.result),
                            fx.visible, fx.distance, fx.light.radius, fx.light.color.x);
                ImGui::Text("Light at %.2f %.2f %.2f / updates %llu", fx.light.position.x, fx.light.position.y,
                            fx.light.position.z, (unsigned long long)fx.updates);
                ImGui::TextWrapped("Point shade unavailable: original fallback. Radial brightening, no shadows. "
                                   "Resource 301 / flashlight.pcx HUD unavailable.");
                ImGui::TextWrapped("Item: %s / effect: %s", item->error.c_str(), fx.error.c_str());
            }
            else
                ImGui::TextUnformatted("Flashlight: not collected (L after collection)");
            if (const auto& item = player->m_InventoryMgr.medkit)
            {
                ImGui::Text("Medkit: %u / 100 charges", item->count);
                ImGui::TextWrapped("Resource 302 / medkit.pcx (HUD unavailable)");
                ImGui::TextWrapped("Requested %.3f / healed %.3f", item->requested_gain, item->actual_gain);
                ImGui::Text("Source result: %d", item->activation_result);
                ImGui::Text("Attempts %llu / queued actions %llu", (unsigned long long)item->attempts,
                            (unsigned long long)item->action_messages);
                ImGui::TextWrapped("Item diagnostic: %s", item->error.c_str());
            }
            else
                ImGui::TextUnformatted("Medkit: not collected (H after collection)");
            ImGui::Text("Item actions %llu / sounds %llu", (unsigned long long)game.client_shell.item_actions,
                        (unsigned long long)game.client_shell.item_sounds);
            ImGui::TextWrapped("Item audio: %s", game.client_shell.item_audio_error.c_str());
            const auto& d = player->m_damage;
            ImGui::Text("Health %.0f / %.0f (mega %.0f)", d.m_fHitPoints, d.m_fMaxHitPoints, d.m_fMaxMegaHitPoints);
            ImGui::Text("Armor %.0f / %.0f (necro %.0f)", d.m_fArmorPoints, d.m_fMaxArmorPoints,
                        d.m_fMaxNecroArmorPoints);
        }
        ImGui::TableNextColumn();
        auto* r = game.objects.get(pickup_selection);
        auto* p = r ? dynamic_cast<const PickupObject*>(r->game.get()) : nullptr;
        if (p)
        {
            ImGui::Text("%s / %s", r->state.m_Name, r->type->name.c_str());
            ImGui::Text("%s / type %u / amount %.0f",
                        p->value.kind == PickupKind::Ammo     ? "Ammo"
                        : p->value.kind == PickupKind::Weapon ? "Weapon"
                        : p->value.kind == PickupKind::Item   ? "Inventory item"
                                                              : "Powerup",
                        p->value.type, p->value.amount);
            ImGui::Text("%s (resource %d)", p->display_name.c_str(), p->resource_id);
            ImGui::TextWrapped("Model: %s", r->state.m_Filename);
            ImGui::TextWrapped("Skin: %s (%s)", r->state.m_SkinName,
                               game.resources.find(r->state.m_SkinName) ? "indexed" : "missing");
            ImGui::Text("Position %.1f %.1f %.1f", r->state.m_Pos.x, r->state.m_Pos.y, r->state.m_Pos.z);
            ImGui::Text("Velocity %.1f %.1f %.1f", r->velocity.x, r->velocity.y, r->velocity.z);
            ImGui::Text("Dimensions %.1f %.1f %.1f", r->dimensions.x, r->dimensions.y, r->dimensions.z);
            ImGui::Text("User flags 0x%08x / glow requested %s (not rendered)", r->user_flags,
                        r->user_flags & (1u << 7) ? "yes" : "no");
            ImGui::Text("Rotate FX requested: %s (not rendered)", p->rotate_fx ? "yes" : "no");
            ImGui::Text("Attempts %llu / consumed %s", static_cast<unsigned long long>(p->attempts),
                        p->picked_up ? "yes" : "no");
            ImGui::TextWrapped("Trigger: %s -> %s", p->pickup_target.c_str(), p->pickup_command.c_str());
            ImGui::TextWrapped("Sound on accepted collection: %s", p->pickup_sound.c_str());
            if (!p->error.empty())
                ImGui::TextWrapped("Pickup diagnostic: %s", p->error.c_str());
            if (!r->model_error.empty())
                ImGui::TextWrapped("Model diagnostic: %s", r->model_error.c_str());
        }
        else
            ImGui::TextUnformatted("No live pickup selected.");
        ImGui::SeparatorText("Recent pickups / latest 32");
        for (const auto& v : game.server.pickup_reports)
        {
            if (v.value)
                ImGui::TextWrapped("%s / %s: %.0f (type %u)", v.name.c_str(), v.class_name.c_str(), v.value->amount,
                                   v.value->type);
            else
                ImGui::TextWrapped("%s / %s: key %s", v.name.c_str(), v.class_name.c_str(), v.key.c_str());
            ImGui::TextWrapped("Sound: %s | voice %llu | %s", v.sound.c_str(), (unsigned long long)v.voice,
                               v.sound.empty()          ? "not requested"
                               : !v.sound_error.empty() ? "failed"
                                                        : "started");
            ImGui::Text("Fixed position %.1f %.1f %.1f | radii 30 / 300 | priority 2 / reverb requested (dry)",
                        v.position.x, v.position.y, v.position.z);
            if (const auto* voice = game.server.audio.get(v.voice))
                ImGui::Text("Decoded frames %llu / %llu | gains %.3f %.3f", (unsigned long long)voice->cursor,
                            (unsigned long long)voice->clip->frames(), voice->left, voice->right);
            else if (v.voice)
                ImGui::TextUnformatted("Voice retired; start record retained.");
            if (!v.sound_error.empty())
                ImGui::TextWrapped("Audio failure: %s", v.sound_error.c_str());
        }
        ImGui::SeparatorText("Destruction spawns / latest 32");
        for (const auto& v : game.server.spawn_reports)
        {
            ImGui::TextWrapped("%s -> %s: %s", v.source.c_str(), v.request.c_str(), v.outcome.c_str());
            ImGui::Text("Child currently %s", game.server.IsObjectAlive(v.child) ? "alive" : "absent");
        }
        ImGui::SeparatorText("NPC weapon drops");
        for (auto id : game.objects.snapshot())
        {
            const auto* r = game.objects.get(id);
            if (!r)
                continue;
            if (const auto* ai = dynamic_cast<const AI_Mgr*>(r->game.get()); ai && ai->m_InventoryMgr.drop_attempts)
            {
                const auto& inv = ai->m_InventoryMgr;
                ImGui::TextWrapped("%s: %llu/%llu drops / %s", r->state.m_Name,
                                   static_cast<unsigned long long>(inv.drops),
                                   static_cast<unsigned long long>(inv.drop_attempts), inv.drop_error.c_str());
            }
            if (const auto* hand = dynamic_cast<const CHandWeaponModel*>(r->game.get());
                hand && (hand->dropped || !hand->drop_error.empty()))
            {
                ImGui::Text("%s / %s / %s / %s", r->state.m_Name, hand->dropped ? "dropped" : "held",
                            r->state.m_Flags & FLAG_VISIBLE ? "visible" : "hidden",
                            r->life == Life::Pending         ? "pending"
                            : r->activity == OBJSTATE_ACTIVE ? "active"
                                                             : "inactive");
                ImGui::Text("Position %.1f %.1f %.1f / velocity %.1f %.1f %.1f", r->state.m_Pos.x, r->state.m_Pos.y,
                            r->state.m_Pos.z, r->velocity.x, r->velocity.y, r->velocity.z);
                ImGui::Text("Scheduled: %s", r->scheduled ? "yes" : "no");
                ImGui::Text("Dimensions %.1f %.1f %.1f / next update %.2f", r->dimensions.x, r->dimensions.y,
                            r->dimensions.z, r->due);
                ImGui::TextWrapped("Pickup ammo: %s / %s", hand->player_owned ? "none" : "12 bullets",
                                   hand->drop_error.c_str());
            }
        }
        if (auto* player = game.server_shell.Player(game.local_client))
            ImGui::Text("Weapon pickup voice requests: %llu (not played)",
                        static_cast<unsigned long long>(player->weapon_pickup_voice_requests));
        ImGui::EndTable();
    }
} // namespace OpenBlood2
