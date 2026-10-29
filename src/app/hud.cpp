#include "app/hud.h"
#include "game/menu.h"
#include "game/original/ClientCrosshair.h"
#include "game/original/WeaponDefs.h"
#include <algorithm>
#include <cmath>
#include <imgui.h>
namespace OpenBlood2
{
    HudState hud_state(const PlayerStats& s, bool in_world, bool external_camera, bool dead, bool show_crosshair,
                       DBYTE crosshair, DBYTE ctf_team, DBYTE ctf_flag)
    {
        HudState h;
        h.stats = s;
        h.crosshair = crosshair;
        h.ctf_team = ctf_team;
        h.ctf_flag = ctf_flag;
        h.crosshair_visible = show_crosshair && original_crosshair_visible(in_world, external_camera, dead);
        if (!in_world || external_camera)
            return h;
        h.visible = weapon_definition(s.weapon) && std::isfinite(s.health) && std::isfinite(s.armor) &&
                    std::isfinite(s.ammo) && std::isfinite(s.alternate_ammo) && std::isfinite(s.max_health) &&
                    std::isfinite(s.max_armor);
        return h;
    }
    bool draw_screen_fade(float scale, bool in_world)
    {
        if (!in_world || !std::isfinite(scale) || scale < 0 || scale >= 1)
            return false;
        const auto* viewport = ImGui::GetMainViewport();
        const auto alpha = static_cast<int>(std::lround((1.f - scale) * 255.f));
        ImGui::GetBackgroundDrawList()->AddRectFilled(
            viewport->Pos, {viewport->Pos.x + viewport->Size.x, viewport->Pos.y + viewport->Size.y},
            IM_COL32(0, 0, 0, alpha));
        return true;
    }
    bool draw_letterbox(bool letterbox)
    {
        if (!letterbox)
            return false;
        // The scene pass clears to the fog colour; the original letterbox rows are black. Cover
        // the presenter's integer height / 6 pixel rows (ScenePresenter::record) in UI units.
        const auto& io = ImGui::GetIO();
        const auto* viewport = ImGui::GetMainViewport();
        const float density = io.DisplayFramebufferScale.y > 0 ? io.DisplayFramebufferScale.y : 1.f;
        const float pixels = std::round(viewport->Size.y * density);
        const float bar = std::floor(pixels / 6) / density;
        if (!(bar > 0))
            return false;
        auto* list = ImGui::GetBackgroundDrawList();
        const ImVec2 min = viewport->Pos, max{viewport->Pos.x + viewport->Size.x, viewport->Pos.y + viewport->Size.y};
        list->AddRectFilled(min, {max.x, min.y + bar}, IM_COL32(0, 0, 0, 255));
        list->AddRectFilled({min.x, max.y - bar}, max, IM_COL32(0, 0, 0, 255));
        return true;
    }
    void draw_touch_controls(const TouchControls& touch)
    {
        const auto* viewport = ImGui::GetMainViewport();
        const float h = viewport->Size.y, aspect = h > 0 ? viewport->Size.x / h : 0;
        if (aspect <= 0)
            return;
        auto* list = ImGui::GetForegroundDrawList();
        auto* font = ImGui::GetFont();
        const auto at = [&](float x, float y) { return ImVec2{viewport->Pos.x + x * h, viewport->Pos.y + y * h}; };
        for (std::size_t i = 0; i < touch_layout.size(); ++i)
        {
            const auto& c = touch_layout[i];
            const auto center = at(TouchControls::center_x(c, aspect), 1 - c.y);
            const float radius = c.radius * h;
            if (touch.pressed[i])
                list->AddCircleFilled(center, radius, IM_COL32(255, 255, 255, 70));
            list->AddCircle(center, radius, IM_COL32(255, 255, 255, 130), 0, std::max(1.f, h / 360));
            const float size = std::max(8.f, radius * .45f);
            const auto text = font->CalcTextSizeA(size, FLT_MAX, 0, c.label);
            list->AddText(font, size, {center.x - text.x / 2, center.y - text.y / 2}, IM_COL32(255, 255, 255, 170),
                          c.label);
        }
        if (touch.stick_active)
        {
            const auto origin = at(touch.origin_x, touch.origin_y);
            list->AddCircle(origin, touch_stick_radius * h, IM_COL32(255, 255, 255, 110), 0, std::max(1.f, h / 360));
            list->AddCircleFilled(at(touch.origin_x + touch.stick_x * touch_stick_radius,
                                     touch.origin_y + touch.stick_y * touch_stick_radius),
                                  touch_stick_radius * h * .4f, IM_COL32(255, 255, 255, 90));
        }
    }
    bool draw_credits(const CCredits& credits)
    {
        const auto& drawn = credits.drawn;
        if (!drawn.credit || !std::isfinite(drawn.color))
            return false;
        // SetupColor1(m_fColor, 0, 0): black is the transparent key over the world and
        // invisible on the cleared screen, so a zero red channel draws nothing.
        const int red = std::clamp(static_cast<int>(drawn.color * 255.f), 0, 255);
        if (!red)
            return false;
        const auto* viewport = ImGui::GetMainViewport();
        const auto fit = MenuViewport::fit(viewport->Size.x, viewport->Size.y);
        if (fit.scale <= 0)
            return false;
        auto* font = ImGui::GetFont();
        auto* list = ImGui::GetBackgroundDrawList();
        const int count = drawn.credit->GetNumStrings(), height = credits.font_height;
        for (int i = 0; i < count; ++i)
        {
            const auto* source = drawn.credit->GetString(i);
            if (!source)
                continue;
            std::string text; // Windows-1252 resource bytes; Latin-1 to UTF-8 for display.
            for (const unsigned char c : *source)
            {
                if (c < 0x80)
                    text += char(c);
                else
                {
                    text += char(0xc0 | (c >> 6));
                    text += char(0x80 | (c & 0x3f));
                }
            }
            // Original DrawString: rcDraw.left = x - xString / 2, top = y - yString / 2.
            const int width = static_cast<int>(
                std::ceil(font->CalcTextSizeA(float(height), FLT_MAX, 0, text.c_str(), text.c_str() + text.size()).x));
            const int center = credit_line_center(240, count, i, height, credits.font_spacing);
            const int left = 320 - width / 2, top = center - height / 2;
            list->AddText(font, height * fit.scale,
                          {viewport->Pos.x + fit.x + left * fit.scale, viewport->Pos.y + fit.y + top * fit.scale},
                          IM_COL32(red, 0, 0, 255), text.c_str(), text.c_str() + text.size());
        }
        return true;
    }
    HudRenderer::~HudRenderer()
    {
        if (device_)
            SDL_WaitForGPUIdle(device_);
        if (texture_)
            SDL_ReleaseGPUTexture(device_, texture_);
        if (transfer_)
            SDL_ReleaseGPUTransferBuffer(device_, transfer_);
    }
    bool HudRenderer::upload(std::string& error)
    {
        constexpr unsigned width = atlas_width, height = atlas_height;
        if (!texture_)
        {
            SDL_GPUTextureCreateInfo info{};
            info.type = SDL_GPU_TEXTURETYPE_2D;
            info.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
            info.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER;
            info.width = width;
            info.height = height;
            info.layer_count_or_depth = 1;
            info.num_levels = 1;
            info.sample_count = SDL_GPU_SAMPLECOUNT_1;
            texture_ = SDL_CreateGPUTexture(device_, &info);
        }
        if (!transfer_)
        {
            SDL_GPUTransferBufferCreateInfo info{};
            info.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
            info.size = width * height * 4;
            transfer_ = SDL_CreateGPUTransferBuffer(device_, &info);
        }
        if (!texture_ || !transfer_)
        {
            error = SDL_GetError();
            return false;
        }
        auto* data = static_cast<std::uint8_t*>(SDL_MapGPUTransferBuffer(device_, transfer_, true));
        if (!data)
        {
            error = SDL_GetError();
            return false;
        }
        std::fill_n(data, width * height * 4, 0);
        unsigned offset = 0;
        for (const auto* tab : status_->tabs())
        {
            const auto& surface = *tab->hTab;
            if (surface.width > width - offset || surface.height > height ||
                surface.rgba.size() != std::size_t(surface.width) * surface.height * 4)
            {
                SDL_UnmapGPUTransferBuffer(device_, transfer_);
                error = "Original HUD surface exceeds atlas bounds";
                return false;
            }
            for (unsigned y = 0; y < surface.height; ++y)
                for (unsigned x = 0; x < surface.width; ++x)
                {
                    const auto s = (y * surface.width + x) * 4, d = (y * width + offset + x) * 4;
                    std::copy_n(surface.rgba.data() + s, 4, data + d);
                    if (data[d] == 255 && data[d + 1] == 0 && data[d + 2] == 255)
                        data[d + 3] = 0;
                }
            offset += surface.width;
        }
        if (!crosshair_ || crosshair_->width > width - crosshair_x || crosshair_->height > height)
        {
            SDL_UnmapGPUTransferBuffer(device_, transfer_);
            error = "Original crosshair exceeds atlas bounds";
            return false;
        }
        // Unlike the status bar's magenta key, PostUpdate passes NULL (black).
        for (unsigned y = 0; y < crosshair_->height; ++y)
            for (unsigned x = 0; x < crosshair_->width; ++x)
            {
                const auto s = (y * crosshair_->width + x) * 4;
                const auto d = (y * width + crosshair_x + x) * 4;
                std::copy_n(crosshair_->rgba.data() + s, 4, data + d);
                if (!data[d] && !data[d + 1] && !data[d + 2])
                    data[d + 3] = 0;
            }
        // Item tabs at 0/75/150, the current item at 225 and its name at 275 (draw() uses the same).
        const auto tabs = status_->item_tabs();
        if (tabs[0]->hTab && tabs[1]->hTab && tabs[2]->hTab && tabs[3]->hTab && status_->item_name())
        {
            unsigned item_offset = 0;
            for (const auto* s :
                 {tabs[0]->hTab, tabs[1]->hTab, tabs[2]->hTab, tabs[3]->hTab, static_cast<HudSurface*>(nullptr)})
            {
                if (!s)
                    s = status_->item_name();
                if (s->width > width - item_offset || s->height > height - items_y ||
                    s->rgba.size() != std::size_t(s->width) * s->height * 4)
                {
                    SDL_UnmapGPUTransferBuffer(device_, transfer_);
                    error = "Original item tab exceeds atlas bounds";
                    return false;
                }
                for (unsigned y = 0; y < s->height; ++y)
                    for (unsigned x = 0; x < s->width; ++x)
                    {
                        const auto from = (y * s->width + x) * 4, to = ((items_y + y) * width + item_offset + x) * 4;
                        std::copy_n(s->rgba.data() + from, 4, data + to);
                        if (data[to] == 255 && data[to + 1] == 0 && data[to + 2] == 255)
                            data[to + 3] = 0;
                    }
                item_offset += s->width;
            }
        }
        for (unsigned slot = 0; slot < WEAPTAB_SLOTS; ++slot)
        {
            const auto* s = status_->weapon_tab(slot).hTab;
            if (!s)
                break;
            const unsigned x0 = (slot % 5) * 150, y0 = weapons_y + (slot / 5) * 25;
            if (s->width > 150 || s->height > 25 || s->rgba.size() != std::size_t(s->width) * s->height * 4)
            {
                SDL_UnmapGPUTransferBuffer(device_, transfer_);
                error = "Original weapon tab exceeds atlas bounds";
                return false;
            }
            for (unsigned y = 0; y < s->height; ++y)
                for (unsigned x = 0; x < s->width; ++x)
                {
                    const auto from = (y * s->width + x) * 4, to = ((y0 + y) * width + x0 + x) * 4;
                    std::copy_n(s->rgba.data() + from, 4, data + to);
                    if (data[to] == 255 && data[to + 1] == 0 && data[to + 2] == 255)
                        data[to + 3] = 0;
                }
        }
        for (auto [surface, slot] : {std::pair{status_->team_icon, team_x}, std::pair{status_->flag_icon, flag_x}})
        {
            if (!surface)
                continue;
            if (surface->width > 128 || surface->height > height)
            {
                SDL_UnmapGPUTransferBuffer(device_, transfer_);
                error = "CTF icon exceeds atlas bounds";
                return false;
            }
            for (unsigned y = 0; y < surface->height; ++y)
                for (unsigned x = 0; x < surface->width; ++x)
                {
                    const auto from = (y * surface->width + x) * 4, to = (y * width + slot + x) * 4;
                    std::copy_n(surface->rgba.data() + from, 4, data + to);
                    if (data[to] == 255 && data[to + 1] == 0 && data[to + 2] == 255)
                        data[to + 3] = 0;
                }
        }
        SDL_UnmapGPUTransferBuffer(device_, transfer_);
        auto* commands = SDL_AcquireGPUCommandBuffer(device_);
        if (!commands)
        {
            error = SDL_GetError();
            return false;
        }
        auto* pass = SDL_BeginGPUCopyPass(commands);
        if (!pass)
        {
            SDL_CancelGPUCommandBuffer(commands);
            error = SDL_GetError();
            return false;
        }
        SDL_GPUTextureTransferInfo source{};
        source.transfer_buffer = transfer_;
        source.pixels_per_row = width;
        source.rows_per_layer = height;
        SDL_GPUTextureRegion destination{};
        destination.texture = texture_;
        destination.w = width;
        destination.h = height;
        destination.d = 1;
        SDL_UploadToGPUTexture(pass, &source, &destination, true);
        SDL_EndGPUCopyPass(pass);
        if (!SDL_SubmitGPUCommandBuffer(commands))
        {
            error = SDL_GetError();
            return false;
        }
        uploaded_revision_ = status_->revision;
        return true;
    }
    bool HudRenderer::draw(const HudState& h, const Resources& resources, std::string& error)
    {
        error.clear();
        if (!h.visible && !h.crosshair_visible)
            return true;
        const auto* viewport = ImGui::GetMainViewport();
        if (viewport->Size.x < 135 || viewport->Size.y < 100)
            return true;
        if (!status_ || resources_ != &resources)
        {
            status_ = std::make_unique<CNewStatusBar>(resources);
            resources_ = &resources;
            uploaded_revision_ = UINT64_MAX;
            item_messages_ = UINT64_MAX;
            item_shows_ = h.item_shows;
            crosshair_ = nullptr;
            if (!status_->Init())
            {
                error = status_->canvas.error;
                return false;
            }
        }
        if (!crosshair_ || crosshair_number_ != h.crosshair)
        {
            // Original PostUpdate reloads the bitmap whenever m_nCrosshair changes.
            crosshair_ = status_->canvas.CreateSurfaceFromBitmap(original_crosshair_path(h.crosshair).c_str());
            crosshair_number_ = h.crosshair;
            uploaded_revision_ = UINT64_MAX;
            if (!crosshair_)
            {
                error = status_->canvas.error;
                return false;
            }
        }
        if (h.visible && !status_->Update(h.stats, unsigned(viewport->Size.x), unsigned(viewport->Size.y)))
        {
            error = status_->canvas.error.empty() ? "Invalid original HUD state" : status_->canvas.error;
            return false;
        }
        if (h.item_messages != item_messages_)
        {
            // One original UpdateInv per SMSG_ITEMCHANGED; names resolve cres.dll IDS_ITEM_*.
            auto items = h.items;
            for (auto* item : {&items.current, &items.previous, &items.next})
                if (*item && (*item)->name_id)
                {
                    if (!item_names_read_)
                    {
                        item_names_read_ = true;
                        std::vector<std::uint8_t> bytes;
                        std::string read_error;
                        if (const auto* dll = resources.find("cres.dll");
                            !dll || dll->size > 32 * 1024 * 1024 || !resources.read("cres.dll", bytes, read_error) ||
                            !read_string_resources(bytes, item_names_, read_error))
                            status_->item_error = read_error.empty() ? "cres.dll item names unavailable" : read_error;
                    }
                    const auto name = item_names_.strings.find((*item)->name_id);
                    (*item)->name = name != item_names_.strings.end() ? name->second : std::string{};
                }
            status_->UpdateInv(items, h.item_shows != item_shows_, h.time);
            item_messages_ = h.item_messages;
            item_shows_ = h.item_shows;
        }
        if (h.world_revision != world_revision_)
        {
            status_->ResetWeapons();
            world_revision_ = h.world_revision;
        }
        if (!status_->UpdateCTF(h.ctf_team, h.ctf_flag))
        {
            error = status_->canvas.error.empty() ? "Invalid CTF HUD state" : status_->canvas.error;
            return false;
        }
        if (uploaded_revision_ != status_->revision && !upload(error))
            return false;
        auto* draw = ImGui::GetBackgroundDrawList();
        unsigned offset = 0, i = 0;
        for (const auto* tab : status_->tabs())
        {
            const auto& s = *tab->hTab;
            if (h.visible && (i != 3 || tab->value))
            {
                const ImVec2 p{viewport->Pos.x + tab->locX, viewport->Pos.y + tab->locY};
                draw->AddImage((ImTextureID)(intptr_t)texture_, p, {p.x + s.width, p.y + s.height},
                               {float(offset) / atlas_width, 0},
                               {float(offset + s.width) / atlas_width, float(s.height) / atlas_height});
            }
            offset += s.width;
            ++i;
        }
        if (h.visible)
        {
            // Original Draw: the current item tab with the stat tabs, then DrawInvTabs.
            const auto layout = status_->DrawInvTabs(h.time);
            const auto tabs = status_->item_tabs();
            const auto image = [&](const HudSurface* s, unsigned atlas_x, float x, float y)
            {
                draw->AddImage((ImTextureID)(intptr_t)texture_, {viewport->Pos.x + x, viewport->Pos.y + y},
                               {viewport->Pos.x + x + s->width, viewport->Pos.y + y + s->height},
                               {float(atlas_x) / atlas_width, float(items_y) / atlas_height},
                               {float(atlas_x + s->width) / atlas_width, float(items_y + s->height) / atlas_height});
            };
            if (layout.current_item)
                image(tabs[3]->hTab, 225, float(tabs[3]->locX), float(tabs[3]->locY));
            for (unsigned t = 0; t < 3; ++t)
                if (layout.visible[t])
                    image(tabs[t]->hTab, 75 * t, float(layout.x[t]), float(layout.y[t]));
            if (layout.name)
                image(status_->item_name(), 275, float(layout.name_x), float(layout.name_y));
            // Then DrawWeaponTabs: every slot's tab at the same sliding x.
            const auto weapons = status_->DrawWeaponTabs(h.time);
            for (unsigned slot = 0; weapons.visible && slot < WEAPTAB_SLOTS; ++slot)
            {
                const auto* s = status_->weapon_tab(slot).hTab;
                if (!s)
                    break;
                const unsigned x0 = (slot % 5) * 150, y0 = weapons_y + (slot / 5) * 25;
                const ImVec2 p{viewport->Pos.x + weapons.x, viewport->Pos.y + weapons.y[slot]};
                draw->AddImage((ImTextureID)(intptr_t)texture_, p, {p.x + s->width, p.y + s->height},
                               {float(x0) / atlas_width, float(y0) / atlas_height},
                               {float(x0 + s->width) / atlas_width, float(y0 + s->height) / atlas_height});
            }
        }
        if (h.visible && h.ctf_team)
        {
            const unsigned team_height = status_->team_icon ? status_->team_icon->height : 0;
            for (auto [surface, slot] : {std::pair{status_->team_icon, team_x}, std::pair{status_->flag_icon, flag_x}})
            {
                if (!surface)
                    continue;
                const ImVec2 p{viewport->Pos.x + viewport->Size.x - surface->width - 5,
                               viewport->Pos.y + (slot == team_x ? 5 : team_height + 10)};
                draw->AddImage((ImTextureID)(intptr_t)texture_, p, {p.x + surface->width, p.y + surface->height},
                               {float(slot) / atlas_width, 0},
                               {float(slot + surface->width) / atlas_width, float(surface->height) / atlas_height});
            }
        }
        if (h.crosshair_visible)
        {
            const auto rect = original_crosshair_rect({0, 0, int(viewport->Size.x), int(viewport->Size.y)},
                                                      crosshair_->width, crosshair_->height);
            draw->AddImage(
                (ImTextureID)(intptr_t)texture_, {viewport->Pos.x + rect.left, viewport->Pos.y + rect.top},
                {viewport->Pos.x + rect.right, viewport->Pos.y + rect.bottom}, {float(crosshair_x) / atlas_width, 0},
                {float(crosshair_x + crosshair_->width) / atlas_width, float(crosshair_->height) / atlas_height});
        }
        return true;
    }
} // namespace OpenBlood2
