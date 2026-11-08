#include "app/hud.h"
#include "game/localized_font.h"
#include "game/menu.h"
#include "game/original/ClientCrosshair.h"
#include "game/original/ConversationTrigger.h"
#include "game/original/WeaponDefs.h"
#include <algorithm>
#include <cmath>
#include <imgui.h>
#include <tuple>
namespace OpenBlood2
{
    namespace
    {
        std::string authored_objective_key(std::string_view title, std::string_view text)
        {
            Sha256 digest;
            constexpr std::uint8_t separator = 0;
            digest.update({reinterpret_cast<const std::uint8_t*>(title.data()), title.size()});
            digest.update({&separator, 1});
            digest.update({reinterpret_cast<const std::uint8_t*>(text.data()), text.size()});
            return "objectives.authored." + digest_text(digest.finish()).substr(0, 16);
        }

        // cres.dll strings are Windows-1252 bytes, typed lines are already UTF-8.
        std::string display_text(const std::string& text)
        {
            for (std::size_t i = 0; i < text.size();)
            {
                const auto c = static_cast<unsigned char>(text[i]);
                const std::size_t length = c < 0x80         ? 1
                                           : (c >> 5) == 6  ? 2
                                           : (c >> 4) == 14 ? 3
                                           : (c >> 3) == 30 ? 4
                                                            : 0;
                bool valid = length && i + length <= text.size();
                for (std::size_t k = 1; valid && k < length; ++k)
                    valid = (static_cast<unsigned char>(text[i + k]) & 0xc0) == 0x80;
                if (!valid)
                {
                    std::string latin;
                    for (const unsigned char b : text)
                    {
                        if (b < 0x80)
                            latin += char(b);
                        else
                        {
                            latin += char(0xc0 | (b >> 6));
                            latin += char(0x80 | (b & 0x3f));
                        }
                    }
                    return latin;
                }
                i += length;
            }
            return text;
        }
    } // namespace
    int draw_messages(CMessageMgr& messages, float time, const std::string* editor, const std::string& notice,
                      ImFont* localized_font, const Localization* localization, const Resources* resources,
                      const LocalizedFont* glyphs)
    {
        int y = 0;
        const auto lines = messages.Draw(time, &y);
        const auto* viewport = ImGui::GetMainViewport();
        auto* font = ImGui::GetFont();
        // Drawn before the status bar, as PostUpdate drew m_MessageMgr before m_NewStatusBar.
        auto* list = ImGui::GetBackgroundDrawList();
        const auto line = [&](const std::string& source, int x, int top, ImFont* face)
        {
            const auto text = display_text(source);
            list->AddText(face, float(CMessageMgr::kFontHeight), {viewport->Pos.x + x, viewport->Pos.y + top},
                          IM_COL32(255, 255, 255, 255), text.c_str(), text.c_str() + text.size());
        };
        const bool translated = localization && localized_font && localization->language() != Language::English;
        const bool stock = translated && (!resources || !localization_resource_overridden(*resources, "cres.dll"));
        for (const auto& l : lines)
        {
            std::string translated_line;
            if (translated && l.source.builtin == ConsoleTextSource::Builtin::BombOverhold)
                translated_line = localization->text("hud.bomb_overhold");
            else if (stock && l.source.resource)
            {
                constexpr std::string_view pickups[]{"hud.picked_up", "hud.pickup_unavailable", "hud.pickup_no_slot"};
                if (l.source.resource >= 260 && l.source.resource <= 262)
                {
                    if (!l.source.argument.empty())
                    {
                        auto name = l.source.argument_resource
                                        ? localization->text(l.source.argument_resource, l.source.argument)
                                        : localization->pickup_name(l.source.argument);
                        if (name.empty())
                            name = l.source.argument;
                        translated_line = localization->format(pickups[l.source.resource - 260], {}, {name});
                    }
                }
                else
                    translated_line = localization->text(l.source.resource);
            }
            const bool use_translation = !translated_line.empty() && translated_line != l.text &&
                                         (!glyphs || glyphs->supports(translated_line));
            line(use_translation ? translated_line : l.text, l.x, l.y, use_translation ? localized_font : font);
        }
        int count = int(lines.size());
        if (!notice.empty())
        {
            line(notice, 1, y, localized_font ? localized_font : font);
            y += CMessageMgr::kFontHeight;
            ++count;
        }
        if (editor)
        {
            // CInputLine::Draw: "SAY:" + text, then a cursor when modf(GetTime()) > .5.
            double whole = 0;
            auto prefix = translated ? localization->text("hud.say", "SAY:") : "SAY:";
            if (glyphs && !glyphs->supports(prefix))
                prefix = "SAY:";
            auto text = std::string(prefix) + (prefix != "SAY:" ? " " : "") + *editor;
            if (std::modf(double(time), &whole) > .5)
                text += '_';
            line(text, 1, y, prefix != "SAY:" ? localized_font : font);
            ++count;
        }
        return count;
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
    void draw_touch_controls(const TouchControls& touch, const Localization* localization, ImFont* localized_face,
                             const LocalizedFont* glyphs)
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
            const auto translation = localization && localized_face ? localization->text(c.localization_key)
                                                                    : std::string_view{};
            const bool localized = !translation.empty() && glyphs && glyphs->supports(translation);
            const auto label = localized ? translation : std::string_view(c.label);
            auto* face = localized ? localized_face : font;
            const auto* end = label.data() + label.size();
            float size = std::max(8.f, radius * .45f);
            auto text = face->CalcTextSizeA(size, FLT_MAX, 0, label.data(), end);
            if (localized && text.x > radius * 1.7f)
            {
                size *= radius * 1.7f / text.x;
                text = face->CalcTextSizeA(size, FLT_MAX, 0, label.data(), end);
            }
            list->AddText(face, size, {center.x - text.x / 2, center.y - text.y / 2}, IM_COL32(255, 255, 255, 170),
                          label.data(), end);
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
    bool draw_credits(const CCredits& credits, const Resources& resources, const Localization* localization,
                      ImFont* localized_font, const LocalizedFont* glyphs)
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
        const bool translate = localized_font && glyphs && localization && localization->language() != Language::English &&
                               !localization_resource_overridden(resources, "cres.dll");
        for (int i = 0; i < count; ++i)
        {
            const auto* source = drawn.credit->GetString(i);
            if (!source)
                continue;
            const auto replacement = translate ? localization->text(credit_line_key(*source)) : std::string_view{};
            const bool translated = !replacement.empty() && glyphs->supports(replacement);
            auto* face = translated ? localized_font : font;
            std::string fallback;
            auto text = replacement;
            if (!translated)
            {
                // Original resource bytes use a Windows single-byte encoding.
                for (const unsigned char c : *source)
                {
                    if (c < 0x80)
                        fallback += char(c);
                    else
                    {
                        fallback += char(0xc0 | (c >> 6));
                        fallback += char(0x80 | (c & 0x3f));
                    }
                }
                text = fallback;
            }
            // Original DrawString: rcDraw.left = x - xString / 2, top = y - yString / 2.
            const auto* end = text.data() + text.size();
            const int width = static_cast<int>(
                std::ceil(face->CalcTextSizeA(float(height), FLT_MAX, 0, text.data(), end).x));
            constexpr int available = 624;
            const float size = translated && width > available ? float(height) * available / width : float(height);
            const int drawn_width = translated ? std::min(width, available) : width;
            const int center = credit_line_center(240, count, i, height, credits.font_spacing);
            const int left = 320 - drawn_width / 2;
            const float top = translated ? center - size / 2 : float(center - height / 2);
            list->AddText(face, size * fit.scale,
                          {viewport->Pos.x + fit.x + left * fit.scale, viewport->Pos.y + fit.y + top * fit.scale},
                          IM_COL32(red, 0, 0, 255), text.data(), end);
        }
        return true;
    }
    namespace
    {
        std::uint8_t* map_hud_texture(SDL_GPUDevice* device, SDL_GPUTexture*& texture, SDL_GPUTransferBuffer*& transfer,
                                      unsigned width, unsigned height, std::string& error)
        {
            if (!texture)
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
                texture = SDL_CreateGPUTexture(device, &info);
            }
            if (!transfer)
            {
                SDL_GPUTransferBufferCreateInfo info{};
                info.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
                info.size = width * height * 4;
                transfer = SDL_CreateGPUTransferBuffer(device, &info);
            }
            auto* data = texture && transfer ? SDL_MapGPUTransferBuffer(device, transfer, true) : nullptr;
            if (!data)
                error = SDL_GetError();
            return static_cast<std::uint8_t*>(data);
        }

        bool submit_hud_texture(SDL_GPUDevice* device, SDL_GPUTexture* texture, SDL_GPUTransferBuffer* transfer,
                                unsigned width, unsigned height, std::string& error)
        {
            SDL_UnmapGPUTransferBuffer(device, transfer);
            auto* commands = SDL_AcquireGPUCommandBuffer(device);
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
            source.transfer_buffer = transfer;
            source.pixels_per_row = width;
            source.rows_per_layer = height;
            SDL_GPUTextureRegion destination{};
            destination.texture = texture;
            destination.w = width;
            destination.h = height;
            destination.d = 1;
            SDL_UploadToGPUTexture(pass, &source, &destination, true);
            SDL_EndGPUCopyPass(pass);
            if (SDL_SubmitGPUCommandBuffer(commands))
                return true;
            error = SDL_GetError();
            return false;
        }
    } // namespace

    HudRenderer::~HudRenderer()
    {
        if (device_)
            SDL_WaitForGPUIdle(device_);
        for (auto* texture : {texture_, communication_texture_})
            if (texture)
                SDL_ReleaseGPUTexture(device_, texture);
        for (auto* transfer : {transfer_, communication_transfer_})
            if (transfer)
                SDL_ReleaseGPUTransferBuffer(device_, transfer);
    }
    bool HudRenderer::upload(std::string& error)
    {
        constexpr unsigned width = atlas_width, height = atlas_height;
        auto* data = map_hud_texture(device_, texture_, transfer_, width, height, error);
        if (!data)
            return false;
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
        // The original heading is painted into the stock 250x150 PCX. Retouch only its strip
        // with the adjacent unlettered texture, leaving the page, skull and border in place.
        const auto* objectives = status_->objectives_page();
        HudSurface localized_page;
        if (objectives && objectives->width == 250 && objectives->height == 150 && localization_ && localized_font_ &&
            resources_ && !localization_resource_overridden(*resources_, "interface/objectives/objectives.pcx"))
        {
            const auto heading = localization_->text("objectives.heading");
            if (!heading.empty() && localized_font_->supports(heading))
            {
                localized_page = *objectives;
                constexpr HudRect caption{46, 9, 171, 29};
                constexpr HudRect clean_strip{174, caption.top, 235, caption.bottom};
                for (int x = caption.left; x < caption.right; x += clean_strip.right - clean_strip.left)
                {
                    auto source = clean_strip;
                    source.right = std::min(source.right, source.left + caption.right - x);
                    hud_blit(localized_page, *objectives, &source, x, caption.top, true, hud_magenta);
                }
                const int label_width = std::min(caption.right - caption.left,
                    localized_font_->text_width(heading, caption.bottom - caption.top));
                const int left = (caption.left + caption.right - label_width) / 2;
                localized_font_->draw(localized_page, heading, {left, caption.top, left + label_width, caption.bottom},
                                      0xd7a569);
                objectives = &localized_page;
            }
        }
        // The objectives page, its icon and the power bar: m_hTransColor (magenta) is transparent.
        for (auto [surface, x0, limit] :
             {std::tuple{objectives, 0u, objectives_icon_x},
              std::tuple{status_->objectives_icon(), objectives_icon_x, power_x - objectives_icon_x},
              std::tuple{status_->power_bar(), power_x, width - power_x}})
        {
            if (!surface)
                continue;
            if (surface->width > limit || surface->height > height - panels_y ||
                surface->rgba.size() != std::size_t(surface->width) * surface->height * 4)
            {
                SDL_UnmapGPUTransferBuffer(device_, transfer_);
                error = "Original objectives or power bar surface exceeds atlas bounds";
                return false;
            }
            for (unsigned y = 0; y < surface->height; ++y)
                for (unsigned x = 0; x < surface->width; ++x)
                {
                    const auto from = (y * surface->width + x) * 4, to = ((panels_y + y) * width + x0 + x) * 4;
                    std::copy_n(surface->rgba.data() + from, 4, data + to);
                    if (data[to] == 255 && data[to + 1] == 0 && data[to + 2] == 255)
                        data[to + 3] = 0;
                }
        }
        if (!submit_hud_texture(device_, texture_, transfer_, width, height, error))
            return false;
        uploaded_revision_ = status_->revision;
        return true;
    }
    void HudRenderer::read_hud_strings(const Resources& resources)
    {
        if (hud_strings_read_)
            return;
        hud_strings_read_ = true;
        std::vector<std::uint8_t> bytes;
        const auto* dll = resources.find("cres.dll");
        if (dll && dll->size <= 32 * 1024 * 1024 && resources.read("cres.dll", bytes, hud_strings_error_) &&
            read_string_resources(bytes, hud_strings_, hud_strings_error_))
            return;
        if (hud_strings_error_.empty())
            hud_strings_error_ = "cres.dll item names unavailable";
    }

    bool HudRenderer::draw_communication(const CommLinkHudState& h, const Resources& resources, std::string& error)
    {
        if (h.portrait.empty())
            return true;
        constexpr unsigned max_width = 4096, max_height = 128;
        constexpr int margin = 10, max_lines = 4;
        const auto* viewport = ImGui::GetMainViewport();
        const unsigned width = unsigned(std::clamp(viewport->Size.x, 0.f, float(max_width)));
        if (width <= 2 * margin)
            return true;
        const auto language = localization_ && localized_font_ ? localization_->language() : Language::English;
        if (!communication_canvas_ || communication_resources_ != &resources)
        {
            communication_resources_ = &resources;
            communication_canvas_ = std::make_unique<HudCanvas>(resources);
            communication_font_ = {};
            communication_canvas_->load_font("interface/commlink/comm_font_1.pcx", "interface/commlink/comm_font_1.fnt",
                                             communication_font_);
            communication_font_.space_width = std::max(1u, communication_font_.height / 4);
            communication_width_ = 0;
        }
        if (communication_width_ != width || communication_language_ != language || drawn_communication_ != h)
        {
            // CCommLink::Draw paints the magenta-keyed face and the four-line text strip at y=10.
            communication_canvas_->error.clear();
            const auto* face = communication_canvas_->CreateSurfaceFromBitmap(h.portrait.c_str());
            const int face_width = face ? int(face->width) : 0;
            const unsigned height = std::min(max_height, std::max({1u, face ? face->height : 0u,
                                                                 max_lines * communication_font_.height}));
            HudSurface strip{width, height, std::vector<std::uint8_t>(std::size_t(width) * height * 4)};
            if (face)
            {
                const int x = int(face_width * std::clamp(h.portrait_ratio, 0.f, 1.f)) - face_width;
                hud_blit(strip, *face, nullptr, x, 0, true, hud_magenta);
            }
            const int left = face_width + margin, right = int(width) - margin;
            if (communication_font_.surface && left < right && h.text_ratio > 0)
            {
                std::string_view text = h.text;
                const auto* original = conversation_text(h.file);
                if (language != Language::English && original && text == original &&
                    !localization_resource_overridden(resources, h.file))
                {
                    const auto value = localization_->dialogue(h.file);
                    if (!value.empty() && localized_font_->supports(value))
                        text = value;
                }
                const auto continuation = [](unsigned char c) { return (c & 0xc0) == 0x80; };
                const auto count = std::ranges::count_if(text, [&](unsigned char c) { return !continuation(c); });
                const auto shown = std::size_t(double(count) * std::clamp(h.text_ratio, 0.f, 1.f));
                auto boundary = h.text_from_start ? shown : std::size_t(count) - shown;
                std::size_t at = 0;
                while (at < text.size() && (boundary || continuation(text[at])))
                    if (!continuation(text[at++]))
                        --boundary;
                const LocalizedFont::TextReveal reveal = h.text_from_start ? LocalizedFont::TextReveal{0, at}
                                                                          : LocalizedFont::TextReveal{at, text.size()};
                const int rows = std::min(int(height), max_lines * int(communication_font_.height));
                if (localized_font_ && *localized_font_)
                {
                    localized_font_->draw_wrapped(strip, text, {left, 0, right, rows}, int(communication_font_.height),
                                                 0xffffff, &communication_font_, reveal);
                    // Keep the PCX pixels and color the Unicode coverage with the original comm_font_1 ink.
                    for (unsigned y = 0; y < unsigned(rows); ++y)
                        for (int x = left; x < right; ++x)
                        {
                            auto* pixel = strip.rgba.data() + (std::size_t(y) * width + x) * 4;
                            if (!pixel[3] && pixel[0])
                            {
                                pixel[3] = pixel[0];
                                pixel[0] = 198;
                                pixel[1] = 165;
                                pixel[2] = 132;
                            }
                        }
                }
                else
                    communication_font_.draw_format(strip, text.substr(reveal.first, reveal.last - reveal.first), left,
                                                    0, right - left);
            }
            auto* bytes = map_hud_texture(device_, communication_texture_, communication_transfer_, max_width,
                                          max_height, error);
            if (!bytes)
                return false;
            std::ranges::copy(strip.rgba, bytes);
            if (!submit_hud_texture(device_, communication_texture_, communication_transfer_, width, height, error))
                return false;
            drawn_communication_ = h;
            communication_width_ = width;
            communication_height_ = height;
            communication_language_ = language;
        }
        const ImVec2 p{viewport->Pos.x, viewport->Pos.y + margin};
        ImGui::GetBackgroundDrawList()->AddImage((ImTextureID)(intptr_t)communication_texture_, p,
            {p.x + communication_width_, p.y + communication_height_}, {0, 0},
            {float(communication_width_) / max_width, float(communication_height_) / max_height});
        return true;
    }

    bool HudRenderer::draw(const HudState& h, const Resources& resources, std::string& error)
    {
        error.clear();
        if (!h.visible && !h.crosshair_visible)
            return draw_communication(h.communication, resources, error);
        const auto* viewport = ImGui::GetMainViewport();
        if (viewport->Size.x < 135 || viewport->Size.y < 100)
            return true;
        const auto language = localization_ && localized_font_ ? localization_->language() : Language::English;
        const bool translated = language != Language::English;
        const bool locale_changed = language != language_;
        const bool stock = translated && !localization_resource_overridden(resources, "cres.dll");
        language_ = language;
        const bool recreate = !status_ || resources_ != &resources;
        if (recreate)
        {
            status_ = std::make_unique<CNewStatusBar>(resources);
            resources_ = &resources;
            uploaded_revision_ = UINT64_MAX;
            item_messages_ = UINT64_MAX;
            item_shows_ = h.item_shows;
            // A new status bar applies the shell's current objectives and power bar request.
            objective_updates_ = power_requests_ = 0;
            hud_strings_ = {};
            hud_strings_error_.clear();
            hud_strings_read_ = false;
            world_revision_ = h.world_revision;
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
        if (h.world_revision != world_revision_)
        {
            // OnEnterWorld's Reset precedes the new world's messages.
            status_->Reset();
            world_revision_ = h.world_revision;
        }
        const bool new_items = h.item_messages != item_messages_;
        if (new_items || locale_changed)
        {
            // One original UpdateInv per SMSG_ITEMCHANGED; names resolve cres.dll IDS_ITEM_*.
            auto items = h.items;
            for (auto* item : {&items.current, &items.previous, &items.next})
                if (*item && (*item)->name_id)
                {
                    read_hud_strings(resources);
                    if (!hud_strings_error_.empty())
                        status_->item_error = hud_strings_error_;
                    const auto name = hud_strings_.strings.find((*item)->name_id);
                    (*item)->name = name != hud_strings_.strings.end() ? name->second : std::string{};
                }
            CNewStatusBar::TextPainter paint_name;
            if (stock && items.current && items.current->name_id)
            {
                const auto value = localization_->text(items.current->name_id);
                if (!value.empty() && value != items.current->name && localized_font_->supports(value))
                    paint_name = [font = localized_font_, value](HudSurface& surface, int)
                    {
                        // Rasterize a grayscale mask and carry its coverage into the keyed atlas as alpha.
                        std::fill(surface.rgba.begin(), surface.rgba.end(), 0);
                        font->draw(surface, value, {0, 0, int(surface.width), int(surface.height)}, 0xffffff);
                        for (std::size_t i = 0; i < surface.rgba.size(); i += 4)
                        {
                            surface.rgba[i + 3] = surface.rgba[i];
                            std::fill_n(surface.rgba.begin() + i, 3, 255);
                        }
                    };
            }
            status_->UpdateInv(items, h.item_shows != item_shows_, h.time, paint_name, new_items);
            item_messages_ = h.item_messages;
            item_shows_ = h.item_shows;
        }
        const bool new_objective = h.objective_updates != objective_updates_;
        if (new_objective || locale_changed || (recreate && translated))
        {
            CNewStatusBar::TextPainter paint_title, paint_body;
            if (translated)
            {
                std::string_view title, body;
                auto resource = h.objectives_resource;
                if (!h.objectives_valid)
                    title = localization_->text("objectives.empty");
                else if (!resource)
                {
                    const auto key = authored_objective_key(h.objectives_title, h.objectives_text);
                    title = localization_->text(key + ".title");
                    body = localization_->text(key + ".text");
                    if (stock && title.empty() && body.empty() && !resources.demo())
                    {
                        // Client save data contains only the resolved English pair. Recover the
                        // original resource number without changing the save payload or shell state.
                        read_hud_strings(resources);
                        const auto& strings = hud_strings_.strings;
                        for (auto it = strings.lower_bound(5000); it != strings.end() && it->first < 6000; ++it)
                            if (it->second == h.objectives_title)
                            {
                                const auto text = strings.find(it->first + 1000);
                                if (text != strings.end() && text->second == h.objectives_text)
                                {
                                    resource = it->first - 4999;
                                    break;
                                }
                            }
                    }
                }
                if (stock && resource)
                {
                    title = localization_->text(4999 + resource);
                    body = localization_->text(5999 + resource);
                }
                if (!title.empty() && title != h.objectives_title && localized_font_->supports(title))
                    paint_title = [font = localized_font_, title](HudSurface& surface, int font_height)
                    {
                        const int height = std::min(font_height, OBJTAB_TEXT_TOP - OBJTAB_TITLE_TOP);
                        const int width = std::min(OBJTAB_TEXT_WIDTH, font->text_width(title, height));
                        const int left = OBJTAB_TITLE_CENTER - width / 2;
                        font->draw(surface, title, {left, OBJTAB_TITLE_TOP, left + width, OBJTAB_TITLE_TOP + height},
                                   0xffffff);
                    };
                if (!body.empty() && body != h.objectives_text && localized_font_->supports(body))
                    paint_body = [font = localized_font_, body](HudSurface& surface, int font_height)
                    {
                        font->draw_wrapped(surface, body,
                                           {OBJTAB_TEXT_LEFT, OBJTAB_TEXT_TOP, OBJTAB_TEXT_LEFT + OBJTAB_TEXT_WIDTH,
                                            int(surface.height) - 7},
                                           font_height, 0xffffff);
                    };
            }
            status_->UpdateObjectives(h.objectives_valid, h.objectives_title, h.objectives_text, paint_title, paint_body,
                                      new_objective);
            objective_updates_ = h.objective_updates;
        }
        if (h.power_requests != power_requests_)
        {
            // CBloodClientShell::SetPowerBarLevel: bar 0 is its TurnOffPowerBar branch.
            if (h.power_bar)
                status_->SetPowerBarLevel(h.power_bar, h.power_percent, h.power_time);
            else
                status_->TurnOffPowerBar();
            power_requests_ = h.power_requests;
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
            // Then DrawObjectives (ScaleSurfaceToSurfaceTransparent into m_rObjectives),
            // DrawObjectivesIcon and DrawPowerBar.
            const auto panel = [&](const HudSurface* s, unsigned atlas_x, ImVec2 min, ImVec2 max)
            {
                draw->AddImage((ImTextureID)(intptr_t)texture_, {viewport->Pos.x + min.x, viewport->Pos.y + min.y},
                               {viewport->Pos.x + max.x, viewport->Pos.y + max.y},
                               {float(atlas_x) / atlas_width, float(panels_y) / atlas_height},
                               {float(atlas_x + s->width) / atlas_width, float(panels_y + s->height) / atlas_height});
            };
            const auto objectives = status_->DrawObjectives(h.time);
            if (objectives.page)
                panel(status_->objectives_page(), 0, {float(objectives.rect.left), float(objectives.rect.top)},
                      {float(objectives.rect.right), float(objectives.rect.bottom)});
            if (objectives.icon)
            {
                const auto* s = status_->objectives_icon();
                panel(s, objectives_icon_x, {float(objectives.icon_x), float(objectives.icon_y)},
                      {float(objectives.icon_x + int(s->width)), float(objectives.icon_y + int(s->height))});
            }
            const auto power = status_->DrawPowerBar(h.time);
            if (power.visible)
            {
                const auto* s = status_->power_bar();
                panel(s, power_x, {float(power.x), float(power.y)},
                      {float(power.x + int(s->width)), float(power.y + int(s->height))});
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
        if (!draw_communication(h.communication, resources, error))
            return false;
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
