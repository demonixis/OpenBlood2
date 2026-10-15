#include "app/inspector.h"
#include "game/original/AmmoPickups.h"
#include <imgui.h>
namespace OpenBlood2
{
    void Inspector::draw_pickups(GameSession& game, ObjectSelection& selection)
    {
        ImGui::TextWrapped("Original single-player ammunition, health and armor. Inventory items, timed bonuses, audio "
                           "and pickup FX remain pending.");
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
            ImGui::Text("%s / type %u / amount %.0f", p->value.kind == PickupKind::Ammo ? "Ammo" : "Powerup",
                        p->value.type, p->value.amount);
            ImGui::Text("%s (resource %d)", p->display_name.c_str(), p->resource_id);
            ImGui::TextWrapped("Model: %s", r->state.m_Filename);
            ImGui::TextWrapped("Skin: %s (%s)", r->state.m_SkinName,
                               game.resources.find(r->state.m_SkinName) ? "indexed" : "missing");
            ImGui::Text("Position %.1f %.1f %.1f", r->state.m_Pos.x, r->state.m_Pos.y, r->state.m_Pos.z);
            ImGui::Text("Velocity %.1f %.1f %.1f", r->velocity.x, r->velocity.y, r->velocity.z);
            ImGui::Text("Dimensions %.1f %.1f %.1f", r->dimensions.x, r->dimensions.y, r->dimensions.z);
            ImGui::Text("Rotate FX requested: %s (not rendered)", p->rotate_fx ? "yes" : "no");
            ImGui::Text("Attempts %llu / consumed %s", static_cast<unsigned long long>(p->attempts),
                        p->picked_up ? "yes" : "no");
            ImGui::TextWrapped("Trigger: %s -> %s", p->pickup_target.c_str(), p->pickup_command.c_str());
            ImGui::TextWrapped("Pickup sound request: %s (not played)", p->pickup_sound.c_str());
            if (!p->error.empty())
                ImGui::TextWrapped("Pickup diagnostic: %s", p->error.c_str());
            if (!r->model_error.empty())
                ImGui::TextWrapped("Model diagnostic: %s", r->model_error.c_str());
        }
        else
            ImGui::TextUnformatted("No live pickup selected.");
        ImGui::SeparatorText("Recent pickups / latest 32");
        for (const auto& v : game.server.pickup_reports)
            ImGui::TextWrapped("%s / %s: %.0f (type %u)", v.name.c_str(), v.class_name.c_str(), v.value.amount,
                               v.value.type);
        ImGui::SeparatorText("Destruction spawns / latest 32");
        for (const auto& v : game.server.spawn_reports)
        {
            ImGui::TextWrapped("%s -> %s: %s", v.source.c_str(), v.request.c_str(), v.outcome.c_str());
            ImGui::Text("Child currently %s", game.server.IsObjectAlive(v.child) ? "alive" : "absent");
        }
        ImGui::EndTable();
    }
} // namespace OpenBlood2
