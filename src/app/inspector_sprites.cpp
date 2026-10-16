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
