#include "game/original/DetailSprite.h"
#include "inspector.h"
#include <algorithm>
#include <imgui.h>
namespace OpenBlood2
{
    void Inspector::draw_sprites(GameSession& game, ObjectSelection& selection)
    {
        ImGui::TextWrapped("Original DetailSprite placement and SPR clock; decoded DTX alpha on world-depth quads. "
                           "Rotatable sprites use their object pose; others face the camera. Unlit replacement "
                           "presentation: glow, view bias, no-Z and sky modes remain unavailable.");
        draw_sparks(game);
        draw_blood_spurts(game);
        draw_blood_splats(game);
        draw_weapon_sprites(game);
        if (sprite_view && sprite_render)
            ImGui::Text("Prepared %zu quads / %zu entries | GPU passes %llu / texture uploads %llu | %zu RGBA bytes",
                        sprite_view->draws.size(), sprite_view->entries.size(),
                        static_cast<unsigned long long>(sprite_render->frames),
                        static_cast<unsigned long long>(sprite_render->uploads), sprite_view->mesh.texture_bytes);
        if (ImGui::CollapsingHeader("Authored sources (including failed / filtered / removed)"))
            for (size_t i = 0; i < game.entries.size(); ++i)
                if (game.world.objects[i].class_name == "DetailSprite")
                {
                    const auto& e = game.entries[i];
                    ImGui::Text("Source %zu: %s", i, entry_state_name(e.state));
                    if (!e.detail.empty())
                        ImGui::TextWrapped("%s", e.detail.c_str());
                }
        if (!ImGui::BeginTable("Sprites", 2, ImGuiTableFlags_Resizable))
            return;
        ImGui::TableSetupColumn("Instances", ImGuiTableColumnFlags_WidthFixed, 220);
        ImGui::TableSetupColumn("State");
        ImGui::TableNextColumn();
        std::vector<const ObjectRecord*> sprites;
        for (auto id : game.objects.snapshot())
        {
            const auto* r = game.objects.get(id);
            if (!r || !r->sprite)
                continue;
            sprites.push_back(r);
            if (!game.objects.get(sprite_selection))
                sprite_selection = id;
            ImGui::PushID(int(id.slot));
            auto label = std::string(r->state.m_Name) + " #" + std::to_string(id.slot);
            if (r->reason == Creation::Runtime)
                label += " (runtime)";
            if (!(r->state.m_Flags & FLAG_VISIBLE))
                label += " hidden";
            if (r->activity != OBJSTATE_ACTIVE)
                label += " inactive";
            if (ImGui::Selectable(label.c_str(), sprite_selection == id))
                sprite_selection = selection.id = id;
            ImGui::PopID();
        }
        ImGui::TableNextColumn();
        if (const auto* r = game.objects.get(sprite_selection); r && r->sprite)
        {
            const auto& s = *r->sprite;
            const auto pos = r->state.m_Pos;
            const auto scale = r->state.m_Scale;
            ImGui::Text("%s / %s", creation_name(r->reason), life_name(r->life));
            ImGui::Text("Position %.2f %.2f %.2f", pos.x, pos.y, pos.z);
            ImGui::Text("Scale %.3f %.3f %.3f | flags 0x%x", scale.x, scale.y, scale.z, r->state.m_Flags);
            const auto q = r->state.m_Rotation;
            ImGui::Text("Rotation quaternion %.3f %.3f %.3f %.3f", q.m_Vec.x, q.m_Vec.y, q.m_Vec.z, q.m_Spin);
            ImGui::Text("SPR %u fps / %zu frames | frame %u | %.3f ms", s.fps, s.frames.size(), r->sprite_track.frame,
                        r->sprite_track.milliseconds);
            ImGui::Text("Transparent %d / translucent %d / color key 0x%x", s.transparent, s.translucent, s.color_key);
            ImGui::TextWrapped("Manifest: %s", r->state.m_Filename);
            const auto& path = s.frames[r->sprite_track.frame];
            ImGui::TextWrapped("Frame: %s [%s]", path.c_str(), game.resources.find(path) ? "indexed" : "missing");
            if (sprite_view)
                for (const auto& entry : sprite_view->entries)
                    if (entry.object == r->id)
                    {
                        ImGui::Text("Decoded %u x %u | zero alpha %zu / partial alpha %zu", entry.width, entry.height,
                                    entry.zero_alpha, entry.partial_alpha);
                        ImGui::Text("Presentation: %s / %s", entry.rotatable ? "object rotation" : "camera facing",
                                    entry.range != UINT32_MAX ? "quad prepared" : "not drawn");
                        if (!entry.error.empty())
                            ImGui::TextWrapped("%s", entry.error.c_str());
                    }
            if (const auto* d = dynamic_cast<const DetailSprite*>(r->game.get()))
            {
                ImGui::Text("Flush %d / rotatable %d / chromakey %d / repeat %d", d->flush, d->rotatable, d->chromakey,
                            d->repeat);
                ImGui::Text("Repeat %u x %u | spacing %.2f %.2f | generated %zu", d->repeat_x, d->repeat_y,
                            d->spacing_x, d->spacing_y, d->children.size());
                if (d->flush)
                    ImGui::Text("Ray: %s | polygon %u",
                                d->flush_result == SegmentResult::Hit    ? "hit"
                                : d->flush_result == SegmentResult::Miss ? "miss"
                                                                         : "failed",
                                d->flush_hit.polygon);
                if (!d->error.empty())
                    ImGui::TextWrapped("%s", d->error.c_str());
                if (!d->children.empty() && ImGui::CollapsingHeader("Independent repeat instances"))
                    for (auto h : d->children)
                    {
                        auto* child = game.objects.get(h);
                        if (child)
                            ImGui::Text("#%u:%u / %s / %.1f %.1f %.1f", child->id.slot, child->id.generation,
                                        life_name(child->life), child->state.m_Pos.x, child->state.m_Pos.y,
                                        child->state.m_Pos.z);
                        else
                            ImGui::TextUnformatted("removed");
                    }
            }
            ImGui::TextUnformatted("XZ placement and authored forward axes (diagnostic overlay)");
            const auto origin = ImGui::GetCursorScreenPos();
            const auto available = ImGui::GetContentRegionAvail();
            ImVec2 size(std::max(100.0f, available.x), std::clamp(available.y - 15, 80.0f, 180.0f));
            DVector low = pos, high = pos;
            for (auto* v : sprites)
            {
                low.x = std::min(low.x, v->state.m_Pos.x);
                low.z = std::min(low.z, v->state.m_Pos.z);
                high.x = std::max(high.x, v->state.m_Pos.x);
                high.z = std::max(high.z, v->state.m_Pos.z);
            }
            const float factor = std::min((size.x - 30) / std::max(1.0f, high.x - low.x),
                                          (size.y - 30) / std::max(1.0f, high.z - low.z));
            auto point = [&](DVector p)
            { return ImVec2(origin.x + 15 + (p.x - low.x) * factor, origin.y + 15 + (p.z - low.z) * factor); };
            auto* draw = ImGui::GetWindowDrawList();
            draw->PushClipRect(origin, ImVec2(origin.x + size.x, origin.y + size.y), true);
            draw->AddRectFilled(origin, ImVec2(origin.x + size.x, origin.y + size.y), IM_COL32(20, 24, 30, 255));
            for (auto* v : sprites)
            {
                auto p = point(v->state.m_Pos);
                auto dir = rotate_vector({0, 0, 1}, v->state.m_Rotation);
                draw->AddCircleFilled(p, v == r ? 5 : 3,
                                      v == r ? IM_COL32(240, 170, 50, 255) : IM_COL32(120, 180, 210, 255));
                draw->AddLine(p, ImVec2(p.x + dir.x * 18, p.y + dir.z * 18), IM_COL32(150, 190, 210, 255));
            }
            draw->PopClipRect();
            ImGui::Dummy(size);
        }
        ImGui::EndTable();
    }
} // namespace OpenBlood2

namespace OpenBlood2
{
    void Inspector::draw_weapon_sprites(const GameSession& game)
    {
        if (!ImGui::CollapsingHeader("Original weapon impact flash / smoke"))
            return;
        const auto& effects = game.server.weapon_sprites;
        ImGui::Text("Active: %zu flashes / %zu smoke | started %llu / expired %llu / failed %llu",
                    effects.flashes.size(), effects.smoke.size(), (unsigned long long)effects.started,
                    (unsigned long long)effects.expired, (unsigned long long)effects.failed);
        ImGui::TextWrapped("Wood/stone bullet impacts only. Bubbles, marks, sparks, fragments and other composite "
                           "effects remain unavailable.");
        if (!effects.update_error.empty())
            ImGui::TextWrapped("Update failure: %s", effects.update_error.c_str());
        auto presentation = [&](std::uint64_t id)
        {
            if (sprite_view)
                for (const auto& entry : sprite_view->entries)
                    if (entry.weapon_effect == id)
                    {
                        ImGui::TextWrapped("Frame %s | %u x %u | %s", entry.frame_path.c_str(), entry.width,
                                           entry.height, entry.range == UINT32_MAX ? "not prepared" : "quad prepared");
                        if (!entry.error.empty())
                            ImGui::TextWrapped("%s", entry.error.c_str());
                    }
        };
        for (const auto& fx : effects.flashes)
        {
            ImGui::Text("Flash %llu / event %llu | age %.3f / %.3f | alpha %.3f", (unsigned long long)fx.id,
                        (unsigned long long)fx.event, fx.last_time - fx.start_time, fx.settings.duration, fx.alpha);
            presentation(fx.id);
        }
        for (const auto& fx : effects.smoke)
        {
            ImGui::Text("Smoke %llu / event %llu | first update %s | delay %.3f | age %.3f / %.3f",
                        (unsigned long long)fx.id, (unsigned long long)fx.event, fx.first_update ? "pending" : "done",
                        fx.delay, fx.first_update ? 0 : fx.last_time - fx.start_time, fx.settings.duration);
            ImGui::Text("Position %.3f %.3f %.3f | scale %.3f %.3f %.3f", fx.position.x, fx.position.y, fx.position.z,
                        fx.scale.x, fx.scale.y, fx.scale.z);
            ImGui::Text("RGB %.2f %.2f %.2f | alpha %.3f | moving %s | callbacks %llu", fx.color.x, fx.color.y,
                        fx.color.z, fx.alpha, fx.settings.move ? "yes" : "no", (unsigned long long)fx.updates);
            presentation(fx.id);
        }
        for (const auto& r : effects.reports)
        {
            ImGui::Text("Event %llu / %s | flash %llu / smoke %llu", (unsigned long long)r.id, r.source.c_str(),
                        (unsigned long long)r.flash, (unsigned long long)r.smoke);
            if (r.settings.flash)
                ImGui::TextWrapped("Flash resource: %s", r.settings.flash->resource.value_or("default").c_str());
            if (r.settings.smoke)
                ImGui::TextWrapped("Smoke resource: %s", r.settings.smoke->resource.value_or("default").c_str());
            if (!r.error.empty())
                ImGui::TextWrapped("%s", r.error.c_str());
        }
    }
} // namespace OpenBlood2

namespace OpenBlood2
{
    void Inspector::draw_blood_splats(const GameSession& game)
    {
        if (!ImGui::CollapsingHeader("Original blood splats / corpse pools"))
            return;
        const auto& effects = game.server.blood_splats;
        ImGui::Text("Active %zu | created %llu / reused %llu / expired %llu / failed %llu", effects.active.size(),
                    (unsigned long long)effects.started, (unsigned long long)effects.reused,
                    (unsigned long long)effects.expired, (unsigned long long)effects.failed);
        ImGui::TextWrapped("Detached rotatable sprites. Original blood spurts and gib impacts are connected; general "
                           "SFX-list eviction "
                           "remain unavailable.");
        if (!effects.update_error.empty())
            ImGui::TextWrapped("%s", effects.update_error.c_str());
        for (const auto& fx : effects.active)
        {
            ImGui::Text("Splat %llu / event %llu | grow clock %.3f | %s | alpha %.3f | callbacks %llu",
                        (unsigned long long)fx.id, (unsigned long long)fx.event, fx.grow_time,
                        fx.shrinking ? "shrinking" : "persistent", fx.alpha, (unsigned long long)fx.updates);
            ImGui::Text("Position %.3f %.3f %.3f | scale %.3f %.3f %.3f", fx.position.x, fx.position.y, fx.position.z,
                        fx.scale.x, fx.scale.y, fx.scale.z);
            ImGui::TextWrapped("%s", fx.resource.c_str());
            if (sprite_view)
                for (const auto& entry : sprite_view->entries)
                    if (entry.blood_splat == fx.id)
                        ImGui::TextWrapped("Frame %s | %u x %u | %s | %s", entry.frame_path.c_str(), entry.width,
                                           entry.height, entry.range == UINT32_MAX ? "not prepared" : "quad prepared",
                                           entry.error.c_str());
        }
        for (const auto& report : effects.reports)
            ImGui::TextWrapped("Event %llu / %s -> splat %llu (%s): %s", (unsigned long long)report.id,
                               report.source.c_str(), (unsigned long long)report.effect,
                               report.reused ? "moved existing" : "creation", report.error.c_str());
    }
} // namespace OpenBlood2

namespace OpenBlood2
{
    void Inspector::draw_blood_spurts(const GameSession& game)
    {
        if (!ImGui::CollapsingHeader("Original blood spurts"))
            return;
        const auto& effects = game.server.blood_spurts;
        ImGui::Text("Systems %zu / 128 | drops %zu / 16384 | started %llu / ended %llu / failed %llu",
                    effects.active.size(), effects.live_particles(), (unsigned long long)effects.started,
                    (unsigned long long)effects.expired, (unsigned long long)effects.failed);
        ImGui::TextWrapped("Six samples/two drops, half-gravity emitter, one impact then fade. Detached from actors. "
                           "Outside-world classification remains unavailable.");
        ImGui::TextWrapped("%s", effects.update_error.c_str());
        for (const auto& fx : effects.active)
        {
            ImGui::PushID(static_cast<int>(fx.id));
            if (ImGui::TreeNode("Spurt", "#%llu %s", (unsigned long long)fx.id, fx.texture.c_str()))
            {
                ImGui::Text("Emitter %.2f %.2f %.2f | velocity %.2f %.2f %.2f", fx.position.x, fx.position.y,
                            fx.position.z, fx.velocity.x, fx.velocity.y, fx.velocity.z);
                ImGui::Text("Emitting %d / liquid %d | drops %zu | alpha %.3f | updates %llu", fx.emitting, fx.liquid,
                            fx.particles.size(), fx.alpha, (unsigned long long)fx.updates);
                ImGui::Text("Impact splat event %llu | lifetime %.2f / fade %.2f", (unsigned long long)fx.splat_event,
                            fx.settings.lifetime, fx.settings.fade_time);
                ImGui::TextWrapped("%s | %s | %s", fx.error.c_str(), fx.splat_error.c_str(), fx.sound_error.c_str());
                if (sprite_view)
                    for (const auto& entry : sprite_view->entries)
                        if (entry.blood_spurt == fx.id && !entry.error.empty())
                        {
                            ImGui::TextWrapped("Texture: %s", entry.error.c_str());
                            break;
                        }
                ImGui::TreePop();
            }
            ImGui::PopID();
        }
        for (const auto& r : effects.reports)
            ImGui::TextWrapped("Event %llu %s -> #%llu | immediate %d / ended %d | drops %llu / impacts %llu / "
                               "splat %llu | %s %s %s",
                               (unsigned long long)r.id, r.source.c_str(), (unsigned long long)r.effect, r.immediate,
                               r.ended, (unsigned long long)r.emitted, (unsigned long long)r.impacts,
                               (unsigned long long)r.splat_event, r.error.c_str(), r.splat_error.c_str(),
                               r.sound_error.c_str());
    }
} // namespace OpenBlood2

namespace OpenBlood2
{
    void Inspector::draw_sparks(const GameSession& game)
    {
        if (!ImGui::CollapsingHeader("Original impact sparks / blood burst"))
            return;
        const auto& store = game.server.sparks;
        ImGui::Text("Systems %zu / 128 | particles %zu / 16384 | started %llu / retired %llu / failed %llu",
                    store.active.size(), store.live_particles(), (unsigned long long)store.started,
                    (unsigned long long)store.retired, (unsigned long long)store.failed);
        ImGui::TextWrapped(
            "One original burst, outgoing component bounds, gravity -150. Detached; particles have no collision. Empty "
            "systems are reclaimed after duration; shared SFX eviction is unavailable. %s",
            store.update_error.c_str());
        for (const auto& fx : store.active)
        {
            ImGui::PushID(static_cast<int>(fx.id));
            if (ImGui::TreeNode("Spark", "#%llu %s", (unsigned long long)fx.id, fx.settings.texture.c_str()))
            {
                ImGui::Text("Position %.2f %.2f %.2f | count %zu / %u | alpha %.3f", fx.settings.position.x,
                            fx.settings.position.y, fx.settings.position.z, fx.particles.size(), fx.settings.count,
                            fx.alpha);
                ImGui::Text("Velocity min %.3f %.3f %.3f / max %.3f %.3f %.3f", fx.min_velocity.x, fx.min_velocity.y,
                            fx.min_velocity.z, fx.max_velocity.x, fx.max_velocity.y, fx.max_velocity.z);
                ImGui::Text("Duration %.2f | emission radius %.2f | draw radius %.2f / 640 | updates %llu",
                            fx.settings.duration, fx.settings.emission_radius, fx.settings.radius,
                            (unsigned long long)fx.updates);
                ImGui::TextWrapped("%s", fx.error.c_str());
                if (sprite_view)
                    for (const auto& entry : sprite_view->entries)
                        if (entry.spark_system == fx.id && !entry.error.empty())
                        {
                            ImGui::TextWrapped("Texture: %s", entry.error.c_str());
                            break;
                        }
                ImGui::TreePop();
            }
            ImGui::PopID();
        }
        for (const auto& r : store.reports)
            ImGui::TextWrapped("Event %llu %s -> #%llu | particles %u / duration %.2f | ended %d | %s",
                               (unsigned long long)r.id, r.source.c_str(), (unsigned long long)r.effect,
                               r.settings.count, r.settings.duration, r.ended, r.error.c_str());
    }
} // namespace OpenBlood2
