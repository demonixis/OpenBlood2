#include "game/menu.h"
#include "engine/localization.h"
#include "game/localized_art.h"
#include "game/localized_font.h"
#include "game/original/MenuIds.h"
#include <SDL3/SDL_stdinc.h>
#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <optional>

namespace OpenBlood2
{
    namespace
    {
        std::string refresh_label(int millihertz)
        {
            auto result = std::to_string(millihertz / 1000);
            if (millihertz % 1000)
            {
                auto fraction = std::to_string(1000 + millihertz % 1000).substr(1);
                while (fraction.back() == '0')
                    fraction.pop_back();
                result += "." + fraction;
            }
            return result + " HZ";
        }
        // CoolFont's implementation was distributed only as a library. Glyph
        // widths are authored; a quarter-height space is the explicit adapter policy.
        int width(const HudFont& font, std::string_view text)
        {
            int result = 0;
            for (unsigned char c : text)
                result += c == ' ' ? std::max(1u, font.height / 4) : c >= '!' && c <= '~' ? font.widths[c - '!'] : 0;
            return result;
        }
        void text(HudSurface& target, const HudFont& font, std::string_view value, int x, int y)
        {
            for (unsigned char c : value)
            {
                if (c == ' ')
                    x += std::max(1u, font.height / 4);
                else if (c >= '!' && c <= '~')
                {
                    const char letter = char(c);
                    font.draw(target, std::string_view(&letter, 1), x, y, 0);
                    x += font.widths[c - '!'];
                }
            }
        }

        struct SettingLabel
        {
            std::string english, localized;
        };
        struct Choice
        {
            std::string_view english, key{};
        };
        std::string localized_save_name(const MenuSaveRow& slot, const Localization& locale, const Resources& resources)
        {
            if (locale.language() == Language::English || slot.world_title.empty())
                return {};
            std::string_view title = slot.name, suffix;
            const auto bracket = title.rfind(" [");
            if (bracket != title.npos && title.size() - bracket == 18 && title.ends_with(']'))
            {
                suffix = title.substr(bracket);
                title = title.substr(0, bracket);
            }
            // Translate the original slot layout while retaining any custom display name.
            if (title != slot.world_title)
                return {};
            auto translated = localization_resource_overridden(resources, slot.world_path)
                                  ? std::string_view{} : locale.world(slot.world_path);
            std::string date(suffix);
            if (!date.empty())
            {
                constexpr std::array<std::string_view, 12> months{"Jan", "Feb", "Mar", "Apr", "May", "Jun",
                                                                  "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
                const auto month = std::ranges::find(months, suffix.substr(2, 3));
                if (month != months.end())
                {
                    const auto key = "menu.save_month." + std::to_string(month - months.begin() + 1);
                    date.replace(2, 3, locale.text(key, suffix.substr(2, 3)));
                }
            }
            if (translated.empty() && date == suffix)
                return {};
            return std::string(translated.empty() ? title : translated) + date;
        }

        SettingLabel setting_label(const Menu& menu, int setting, const Localization* localization,
                                   const Resources& resources)
        {
            const auto& display = menu.settings;
            constexpr Choice on{"ON", "menu.on"}, off{"OFF", "menu.off"};
            constexpr Choice low{"LOW", "menu.low"}, high{"HIGH", "menu.high"};
            const auto toggle = [&](bool value) { return value ? on : off; };
            const auto row = [&](std::string_view key, std::string_view english, Choice value,
                                 std::string_view suffix = {})
            {
                SettingLabel label;
                label.english = std::string(english) + " [" + std::string(value.english) + std::string(suffix) + "]";
                const auto title = localization ? localization->text(key, {}) : std::string_view{};
                if (!title.empty())
                {
                    const auto option = value.key.empty() ? value.english : localization->text(value.key, value.english);
                    label.localized = std::string(title) + " [" + std::string(option) + std::string(suffix) + "]";
                }
                return label;
            };
            switch (setting)
            {
                case 7:
                    return row("menu.lighting", "LIGHTING",
                               display.renderer ? (display.quality ? high : Choice{"STANDARD", "menu.standard"})
                               : display.pixel_lighting ? Choice{"PER PIXEL", "menu.per_pixel"}
                                                        : Choice{"PER VERTEX", "menu.per_vertex"});
                case 8:
                    return row("menu.renderer", "RENDERER", display.renderer ? Choice{"ENHANCED", "menu.enhanced"}
                                                                             : Choice{"ORIGINAL", "menu.original"});
                case 9:
                    return row("menu.normal_maps", "NORMAL MAPS",
                               display.normal_maps ? Choice{"GENERATED", "menu.generated"} : off);
                case 10:
                    if (display.vr_turn)
                        return row("menu.turn", "TURN", {"SNAP", "menu.snap"}, " " + std::to_string(display.vr_turn));
                    return row("menu.turn", "TURN", {"SMOOTH", "menu.smooth"});
                case 11:
                    return row("menu.aim_laser", "AIM LASER", toggle(display.vr_laser));
                case 12:
                    return row("menu.crosshair", "CROSSHAIR", toggle(display.vr_crosshair));
                case 13:
                    return row("menu.move", "MOVE", display.vr_move_hand ? Choice{"RIGHT HAND", "menu.right_hand"}
                                                                        : Choice{"HEAD", "menu.head"});
                case 14:
                    return row("menu.cheats", "CHEATS", toggle(display.cheats));
                case 15:
                    return row("menu.god_mode", "GOD MODE", toggle(display.god_mode));
                case 16:
                {
                    if (menu.level < 0 || menu.level >= int(menu.levels.size()))
                        return row("menu.level", "LEVEL", {"NONE", "menu.none"});
                    auto world = menu.levels[std::size_t(menu.level)];
                    const auto path = "worlds/" + world + ".dat";
                    const auto localized = localization && !localization_resource_overridden(resources, path)
                                               ? localization->world(path) : std::string_view{};
                    for (auto& c : world)
                        c = char(std::toupper(static_cast<unsigned char>(c)));
                    auto label = row("menu.level", "LEVEL", {world});
                    if (!localized.empty())
                        label.localized = std::string(localization->text("menu.level", "LEVEL")) + " [" +
                                          std::string(localized) + "]";
                    return label;
                }
                case 17:
                {
                    constexpr std::array modes{off, Choice{"FXAA"}, Choice{"MSAA 2X"}, Choice{"MSAA 4X"}};
                    return row("menu.antialiasing", "ANTI-ALIASING", modes[std::clamp(display.antialiasing, 0, 3)]);
                }
                case 18:
                    return row("menu.bloom", "BLOOM", toggle(display.bloom));
                case 19:
                {
                    constexpr std::array modes{off, Choice{"NEUTRAL", "menu.neutral"}, Choice{"FILMIC", "menu.filmic"}};
                    return row("menu.tonemap", "TONEMAP", modes[std::clamp(display.tonemap, 0, 2)]);
                }
                case 20:
                {
                    constexpr std::array modes{off, low, high};
                    return row("menu.ambient_occlusion", "AMBIENT OCCLUSION",
                               modes[std::clamp(display.ambient_occlusion, 0, 2)]);
                }
                case 21:
                {
                    constexpr std::array modes{off, low, high};
                    return row("menu.reflections", "REFLECTIONS", modes[std::clamp(display.reflections, 0, 2)]);
                }
                case 22:
                    return row("menu.fullscreen", "FULLSCREEN", toggle(display.fullscreen));
                case 23:
                    return row("menu.fullscreen_mode", "FULLSCREEN MODE",
                               display.fullscreen_mode ? Choice{"EXCLUSIVE", "menu.exclusive"}
                                                       : Choice{"BORDERLESS", "menu.borderless"});
                case 24:
                    if (display.fullscreen && !display.fullscreen_mode)
                        return row("menu.resolution", "RESOLUTION", {"DESKTOP", "menu.desktop"});
                    if (display.resolution_width)
                        return row("menu.resolution", "RESOLUTION",
                                   {std::to_string(display.resolution_width) + " X " +
                                    std::to_string(display.resolution_height)});
                    return row("menu.resolution", "RESOLUTION", {"DEFAULT", "menu.default"});
                case 25:
                    return row("menu.vsync", "VSYNC", toggle(display.vsync));
                case 26:
                    if (display.fps_limit)
                        return row("menu.frame_limit", "FRAME LIMIT", {std::to_string(display.fps_limit) + " FPS"});
                    return row("menu.frame_limit", "FRAME LIMIT", {"UNLIMITED", "menu.unlimited"});
                case 27:
                    if (menu.video.refresh_rates.size() < 2)
                        return row("menu.refresh_rate", "REFRESH RATE", {"UNAVAILABLE", "menu.unavailable"});
                    if (display.vr_refresh_rate)
                        return row("menu.refresh_rate", "REFRESH RATE", {refresh_label(display.vr_refresh_rate)});
                    return row("menu.refresh_rate", "REFRESH RATE", {"AUTO", "menu.auto"});
                case 28:
                    return row("menu.render_scale", "RENDER SCALE", {std::to_string(display.vr_render_scale) + "%"});
                case 30:
                    return row("menu.field_of_view", "FIELD OF VIEW", {std::to_string(display.field_of_view)});
                case menu_language_setting:
                {
                    const auto fallback = display.language == Language::English ? std::string_view("English")
                                                                                : language_tag(display.language);
                    auto label = row("menu.language", "LANGUAGE", {fallback});
                    if (localization)
                        label.localized = std::string(localization->text("menu.language", "LANGUAGE")) + " [" +
                                          std::string(language_name(display.language)) + "]";
                    return label;
                }
                default:
                    return {};
            }
        }
    } // namespace
    MenuViewport MenuViewport::fit(float width, float height)
    {
        if (!std::isfinite(width) || !std::isfinite(height) || width <= 0 || height <= 0)
            return {0, 0, 0};
        const float scale = std::min(width / 640, height / 480);
        return {(width - 640 * scale) / 2, (height - 480 * scale) / 2, scale};
    }
    bool MenuViewport::point(float& px, float& py) const
    {
        if (scale <= 0 || !std::isfinite(px) || !std::isfinite(py))
            return false;
        px = (px - x) / scale;
        py = (py - y) / scale;
        return px >= 0 && px < 640 && py >= 0 && py < 480;
    }
    bool MenuCanvas::initialize(std::string& error)
    {
        if (surface)
            return true;
        const auto& assets = menu_resources(menu_edition(resources_));
        if (!load_menu_strings(resources_, strings, error))
            return false;
        if (!resources_.demo())
            if (const auto* source = resources_.find("cres.dll"))
                stock_strings_ = std::ranges::none_of(resources_.mounts(), [&](const Mount& mount)
                { return mount.role == MountRole::Mod && mount.path == source->archive; });
        if (!canvas.load_font(assets.small_bitmap, assets.small_font, small_) ||
            !canvas.load_font(assets.large_bitmaps[0], assets.large_font, selected_, 1) ||
            !canvas.load_font(assets.large_bitmaps[1], assets.large_font, normal_, 1) ||
            !canvas.load_font(assets.large_bitmaps[2], assets.large_font, disabled_, 1))
        {
            error = canvas.error;
            return false;
        }
        background_ = canvas.CreateSurfaceFromBitmap(assets.background);
        slider_ = canvas.CreateSurfaceFromBitmap(assets.slider);
        thumb_ = canvas.CreateSurfaceFromBitmap(assets.thumb);
        if (!resources_.demo())
        {
            up_arrow_ = canvas.CreateSurfaceFromBitmap(assets.up_arrow);
            down_arrow_ = canvas.CreateSurfaceFromBitmap(assets.down_arrow);
        }
        for (const auto* key : {"mainmenu.pcx", "singleplayer.pcx", "options.pcx", "loadgame.pcx", "savegame.pcx"})
            if (const auto* path = assets.title(key))
                titles_[key] = canvas.CreateSurfaceFromBitmap(path);
        for (auto promotion : {MenuPromotion::Bloodbath, MenuPromotion::Info, MenuPromotion::Modes})
            if (const auto* path = assets.promotion(promotion))
            {
                const auto* image = canvas.CreateSurfaceFromBitmap(path);
                if (image && (image->width != 640 || image->height != 480))
                {
                    error = std::string("Invalid 640x480 menu promotion: ") + path;
                    return false;
                }
                promotions_[promotion] = image;
            }
        if (!canvas.error.empty())
        {
            error = canvas.error;
            return false;
        }
        surface = canvas.CreateSurface(640, 480);
        return surface != nullptr;
    }
    std::string_view MenuCanvas::translated(std::string_view key) const
    {
        return localization && localized_font && !key.empty() ? localization->text(key, {}) : std::string_view{};
    }
    std::string_view MenuCanvas::translated(unsigned id) const
    {
        return localization && localized_font && stock_strings_ ? localization->text(id, {}) : std::string_view{};
    }
    void MenuCanvas::draw_small(std::string_view english, std::string_view translation, int x, int y, int right)
    {
        if (!translation.empty() && localized_font && localized_font->supports(translation))
            LocalizedBitmapLine(small_, *localized_font, translation).draw(*surface, {x, y, right, y + int(small_.height)});
        else
            text(*surface, small_, english, x, y);
    }
    // CMenuControls rows: CLTGUIColumnTextCtrl columns (action 100 px right-justified, an 11 px
    // buffer, "=" 20 px, then the bound controls) in the 280-pixel CLTGUIListCtrl at the options
    // position, with the CMenuBase::RenderArrows up/down arrows centered on x = 298.
    bool MenuCanvas::compose_controls(Menu& menu, std::string& error)
    {
        constexpr int left = 180, top = 140, height = 280, arrow_center = 298;
        const auto unassigned = strings.strings.find(IDS_MENU_KEY_UNASSIGNED);
        if (unassigned == strings.strings.end() || !up_arrow_ || !down_arrow_)
        {
            error = "Missing original controls menu resources";
            return false;
        }
        const int row = int(small_.height), count = int(menu.items.size());
        const int visible = std::max(1, height / std::max(1, row));
        menu.selected = std::clamp(menu.selected, 0, std::max(0, count - 1));
        menu.first = std::clamp(menu.first, std::max(0, menu.selected - visible + 1), menu.selected);
        int y = top;
        for (int i = 0; i < count; ++i)
        {
            auto& item = menu.items[i];
            item.rect = {};
            if (item.string_id)
            {
                const auto it = strings.strings.find(item.string_id);
                if (it == strings.strings.end())
                {
                    error = "Missing original menu string " + std::to_string(item.string_id);
                    return false;
                }
                item.literal = it->second;
            }
            if (i < menu.first || i >= menu.first + visible)
                continue;
            const bool control = item.command == int(MENU_CMD_CHANGE_CONTROL);
            const auto translation = item.string_id ? translated(item.string_id) : translated(item.localization_key);
            const bool draw_translation = !translation.empty() && localized_font->supports(translation);
            const int label = draw_translation ? LocalizedBitmapLine(small_, *localized_font, translation).advance()
                                               : width(small_, item.literal);
            const int x = control ? std::max(draw_translation ? 40 : 0, left + 100 - label) : left;
            item.rect = {draw_translation ? std::min(left, x) : left, y, 625, y + row};
            draw_small(item.literal, translation, x, y, control ? left + 100 : 625);
            if (control && i == menu.binding)
                text(*surface, small_, "=", left + 111, y);
            if (control && i < int(menu.controls.size()))
            {
                std::string value = menu.controls[i].binding;
                std::string binding_translation;
                if (!menu.controls[i].enabled)
                {
                    value = "NOT IMPLEMENTED";
                    binding_translation = translated("control.not_implemented");
                }
                else if (value.empty())
                {
                    value = unassigned->second;
                    binding_translation = translated(IDS_MENU_KEY_UNASSIGNED);
                }
                else if (menu.controls[i].localized_binding != value)
                    binding_translation = menu.controls[i].localized_binding;
                // The bound text stops at the canvas edge instead of overflowing it.
                while (!value.empty() && left + 131 + width(small_, value) > 635)
                    value.pop_back();
                draw_small(value, binding_translation, left + 131, y, 635);
            }
            if (i == menu.selected && menu.enabled(item))
                text(*surface, small_, ">", item.rect.left - 16, y);
            if (!menu.enabled(item))
                for (int py = item.rect.top; py < item.rect.bottom; ++py)
                    for (int px = item.rect.left; px < item.rect.right; ++px)
                        for (unsigned c = 0; c < 3; ++c)
                            surface->rgba[(py * 640 + px) * 4 + c] /= 2;
            y += row;
        }
        if (menu.first > 0)
            canvas.DrawSurfaceToSurfaceTransparent(surface, up_arrow_, nullptr,
                                                   arrow_center - int(up_arrow_->width) / 2,
                                                   top - int(up_arrow_->height) - 8, hud_magenta);
        if (menu.first + visible < count)
            canvas.DrawSurfaceToSurfaceTransparent(surface, down_arrow_, nullptr,
                                                   arrow_center - int(down_arrow_->width) / 2, top + height - 5,
                                                   hud_magenta);
        if (menu.audio_failed)
            draw_small("Menu audio unavailable - see log.", translated("menu.audio_unavailable"), 180, 460);
        return true;
    }
    namespace
    {
        enum class PosterTextStyle
        {
            Glow,
            Center,
            Underline,
            Left,
            Paragraph,
        };
        struct PosterText
        {
            std::string_view key;
            HudRect original, translated;
            int height;
            PosterTextStyle style;
        };

        std::span<const PosterText> poster_text(MenuPromotion promotion)
        {
            using Style = PosterTextStyle;
            static constexpr PosterText bloodbath[] = {
                {"promotions.bloodbath_title", {119, 42, 520, 92}, {56, 42, 584, 92}, 48, Style::Glow},
                {"promotions.bloodbath_tagline", {65, 94, 576, 115}, {58, 94, 582, 115}, 20, Style::Center},
                {"promotions.bloodbath_soon", {206, 119, 417, 151}, {115, 118, 525, 150}, 25, Style::Underline},
                {"promotions.blood_brother_title", {67, 159, 311, 183}, {68, 159, 313, 183}, 24, Style::Center},
                {"promotions.blood_brother_body", {68, 184, 311, 234}, {72, 184, 312, 238}, 16, Style::Paragraph},
                {"promotions.blood_feud_title", {351, 159, 540, 183}, {330, 159, 582, 183}, 24, Style::Center},
                {"promotions.blood_feud_body", {328, 184, 584, 239}, {331, 184, 582, 239}, 16, Style::Paragraph},
            };
            static constexpr PosterText info[] = {
                {"promotions.info_title", {177, 89, 466, 138}, {56, 88, 584, 137}, 48, Style::Glow},
                {"promotions.info_characters", {300, 145, 624, 177}, {303, 145, 620, 179}, 14, Style::Paragraph},
                {"promotions.info_weapons", {300, 182, 624, 243}, {303, 182, 620, 245}, 14, Style::Paragraph},
                {"promotions.info_levels", {300, 248, 624, 280}, {303, 248, 620, 282}, 14, Style::Paragraph},
                {"promotions.info_maps", {300, 284, 624, 318}, {303, 284, 620, 320}, 14, Style::Paragraph},
                {"promotions.info_network", {300, 321, 624, 342}, {303, 321, 620, 345}, 14, Style::Paragraph},
                {"promotions.info_editor", {300, 345, 624, 377}, {303, 345, 620, 381}, 14, Style::Paragraph},
                {"promotions.info_engine", {300, 382, 624, 425}, {303, 382, 620, 426}, 14, Style::Paragraph},
            };
            static constexpr PosterText modes[] = {
                {"promotions.modes_title", {79, 42, 582, 100}, {56, 42, 584, 100}, 50, Style::Glow},
                {"promotions.modes_caleb", {194, 126, 621, 168}, {195, 126, 620, 174}, 16, Style::Paragraph},
                {"promotions.modes_chosen_title", {91, 377, 431, 399}, {92, 377, 431, 399}, 22, Style::Left},
                {"promotions.modes_chosen", {91, 399, 431, 432}, {93, 399, 431, 433}, 16, Style::Paragraph},
            };
            switch (promotion)
            {
                case MenuPromotion::Bloodbath:
                    return bloodbath;
                case MenuPromotion::Info:
                    return info;
                case MenuPromotion::Modes:
                    return modes;
                default:
                    return {};
            }
        }

        std::uint8_t* poster_pixel(HudSurface& surface, int x, int y)
        {
            return surface.rgba.data() + (std::size_t(y) * surface.width + unsigned(x)) * 4;
        }
        const std::uint8_t* poster_pixel(const HudSurface& surface, int x, int y)
        {
            return surface.rgba.data() + (std::size_t(y) * surface.width + unsigned(x)) * 4;
        }

        bool poster_ink(const std::uint8_t* rgb)
        {
            const int peak = std::max({rgb[0], rgb[1], rgb[2]});
            const int base = std::min({rgb[0], rgb[1], rgb[2]});
            return peak >= 24 && peak - base <= 22;
        }

        void erase_poster_ink(HudSurface& target, const HudSurface& original, const HudSurface& background,
                              const HudRect& rect, bool glow)
        {
            // Copper headings have a large, low-contrast halo. Restore the rock across the whole
            // title region to remove it, without bringing over the menu background's bright seam.
            // Gray captions only need individual ink texels repaired from their own backdrop.
            for (int y = rect.top; y < rect.bottom; ++y)
                for (int x = rect.left; x < rect.right; ++x)
                {
                    auto* pixel = poster_pixel(target, x, y);
                    if (glow && x < int(background.width) && y < int(background.height))
                    {
                        auto* rock = poster_pixel(background, x, y);
                        if (std::max({rock[0], rock[1], rock[2]}) >= 40 && x >= 16)
                            rock = poster_pixel(background, x - 16, y);
                        const auto edge = unsigned(std::min({x - rect.left + 1, rect.right - x,
                                                             y - rect.top + 1, rect.bottom - y, 4}));
                        for (unsigned c = 0; c < 3; ++c)
                            pixel[c] = std::uint8_t((pixel[c] * (4 - edge) + rock[c] * edge) / 4);
                        continue;
                    }
                    if (!poster_ink(poster_pixel(original, x, y)))
                        continue;

                    std::array<unsigned, 3> color{};
                    unsigned count = 0;
                    for (int radius = 1; radius <= 10 && !count; ++radius)
                        for (const auto& [sx, sy] : {std::pair{x - radius, y}, std::pair{x + radius, y},
                                                     std::pair{x, y - radius}, std::pair{x, y + radius}})
                        {
                            if (sx < rect.left || sx >= rect.right || sy < rect.top || sy >= rect.bottom)
                                continue;
                            const auto* sample = poster_pixel(original, sx, sy);
                            if (poster_ink(sample))
                                continue;
                            for (unsigned c = 0; c < 3; ++c)
                                color[c] += sample[c];
                            ++count;
                        }
                    for (unsigned c = 0; c < 3; ++c)
                        pixel[c] = count ? std::uint8_t(color[c] / count) : 0;
                }
        }

        void draw_poster_glow(HudSurface& target, const HudSurface& original, const LocalizedFont& font,
                              std::string_view label, const HudRect& source, const HudRect& rect,
                              const std::array<unsigned, 3>& glow = {184, 87, 20})
        {
            const unsigned width = unsigned(rect.right - rect.left), height = unsigned(rect.bottom - rect.top);
            HudSurface mask{width, height, std::vector<std::uint8_t>(std::size_t(width) * height * 4)};
            font.draw(mask, label, {0, 0, int(width), int(height)}, 0xffffff);
            constexpr int radius = 3;
            for (int y = 0; y < int(height); ++y)
                for (int x = 0; x < int(width); ++x)
                {
                    unsigned total = 0;
                    for (int sy = std::max(0, y - radius); sy <= std::min(int(height) - 1, y + radius); ++sy)
                        for (int sx = std::max(0, x - radius); sx <= std::min(int(width) - 1, x + radius); ++sx)
                            total += *poster_pixel(mask, sx, sy);
                    const unsigned coverage = total / ((radius * 2 + 1) * (radius * 2 + 1) * 2);
                    auto* pixel = poster_pixel(target, rect.left + x, rect.top + y);
                    for (unsigned c = 0; c < 3; ++c)
                        pixel[c] = std::uint8_t((pixel[c] * (255 - coverage) + glow[c] * coverage + 127) / 255);
                }

            const unsigned sample_width = unsigned(source.right - source.left);
            const unsigned sample_height = unsigned(source.bottom - source.top);
            HudSurface sample{sample_width, sample_height,
                              std::vector<std::uint8_t>(std::size_t(sample_width) * sample_height * 4)};
            hud_blit(sample, original, &source, 0, 0, false, 0);
            font.draw_menu(target, label, rect, sample, true);

            std::array<unsigned, 3> highlight{};
            unsigned highlights = 0;
            for (std::size_t i = 0; i < sample.rgba.size(); i += 4)
                if (sample.rgba[i] >= 208 && sample.rgba[i + 1] >= 155 && sample.rgba[i + 2] >= 95)
                {
                    for (unsigned c = 0; c < 3; ++c)
                        highlight[c] += sample.rgba[i + c];
                    ++highlights;
                }
            if (!highlights)
                return;
            for (auto& channel : highlight)
                channel /= highlights;
            for (int y = 0; y < int(height); ++y)
                for (int x = 0; x < int(width); ++x)
                {
                    const unsigned coverage = *poster_pixel(mask, x, y) / 3;
                    auto* pixel = poster_pixel(target, rect.left + x, rect.top + y);
                    for (unsigned c = 0; c < 3; ++c)
                        pixel[c] = std::uint8_t((pixel[c] * (255 - coverage) + highlight[c] * coverage + 127) / 255);
                }
        }

        void localize_poster(HudSurface& target, const HudSurface& original, const HudSurface& background,
                            MenuPromotion promotion, const Localization& catalog, const LocalizedFont& font)
        {
            const auto entries = poster_text(promotion);
            if (std::ranges::any_of(entries, [&](const PosterText& entry)
                                   {
                                       const auto label = catalog.text(entry.key);
                                       return label.empty() || !font.supports(label);
                                   }))
                return; // Incomplete catalogs keep the authored English poster intact.

            // Restore every text region before painting so one replacement cannot erase its neighbor.
            for (const auto& entry : entries)
                erase_poster_ink(target, original, background, entry.original, entry.style == PosterTextStyle::Glow);

            constexpr std::uint32_t gray = 0x868686;
            for (const auto& entry : entries)
            {
                const auto label = catalog.text(entry.key);
                auto rect = entry.translated;
                if (entry.style == PosterTextStyle::Paragraph)
                {
                    font.draw_wrapped(target, label, rect, entry.height, gray);
                    continue;
                }
                const int fitted = std::min(rect.right - rect.left, font.text_width(label, entry.height) + 2);
                if (entry.style != PosterTextStyle::Left)
                    rect.left += (rect.right - rect.left - fitted) / 2;
                rect.right = rect.left + fitted;
                rect.top += (rect.bottom - rect.top - entry.height) / 2;
                rect.bottom = rect.top + entry.height;
                if (entry.style == PosterTextStyle::Glow)
                    draw_poster_glow(target, original, font, label, entry.original, rect);
                else
                    font.draw(target, label, rect, gray);
                if (entry.style == PosterTextStyle::Underline)
                    for (int x = rect.left; x < rect.right; ++x)
                        std::fill_n(poster_pixel(target, x, rect.bottom), 3, std::uint8_t(gray & 255));
            }
        }

        enum class ArtInk
        {
            Splash,
            Copper,
            Red,
        };

        void replace_art_label(HudSurface& art, const HudRect& area, HudRect text, std::string_view label,
                               const LocalizedFont& font, ArtInk ink, const HudSurface* backdrop = nullptr)
        {
            const HudSurface original = art;
            const int width = area.right - area.left, height = area.bottom - area.top;
            std::vector<std::uint8_t> mask(std::size_t(width) * height);
            const auto inside = [&](int x, int y)
            {
                return x >= area.left && x < area.right && y >= area.top && y < area.bottom;
            };
            const auto at = [&](int x, int y) { return std::size_t(y - area.top) * width + (x - area.left); };
            const auto covered = [&](int x, int y) { return inside(x, y) && mask[at(x, y)]; };
            // Include the darker glow around the letter strokes. Sampling it as background
            // would leave recognizable English letters underneath the translation.
            const int radius = ink == ArtInk::Splash ? 4 : 3;
            for (int y = area.top; y < area.bottom; ++y)
                for (int x = area.left; x < area.right; ++x)
                {
                    const auto* pixel = poster_pixel(original, x, y);
                    const int r = pixel[0], g = pixel[1], b = pixel[2];
                    const bool colored = ink == ArtInk::Splash ? r >= 170
                                       : ink == ArtInk::Red    ? r >= 18 && r > 3 * (g + b)
                                                               : r >= 18 && r - g >= 7 && g - b >= 5;
                    if (colored)
                        for (int dy = -radius; dy <= radius; ++dy)
                            for (int dx = -radius; dx <= radius; ++dx)
                                if (dx * dx + dy * dy <= radius * radius && inside(x + dx, y + dy))
                                    mask[at(x + dx, y + dy)] = 1;
                }
            constexpr std::pair<int, int> directions[]{{1, 0}, {-1, 0}, {0, 1}, {0, -1},
                                                       {1, 1}, {-1, -1}, {1, -1}, {-1, 1}};
            for (int y = area.top; y < area.bottom; ++y)
                for (int x = area.left; x < area.right; ++x)
                {
                    if (!covered(x, y))
                        continue;
                    auto* pixel = poster_pixel(art, x, y);
                    if (backdrop && backdrop->width == art.width && backdrop->height == art.height)
                    {
                        std::copy_n(poster_pixel(*backdrop, x, y), 3, pixel);
                        continue;
                    }
                    std::array<float, 3> color{};
                    float total_weight = 0;
                    for (const auto& [dx, dy] : directions)
                    {
                        if (ink == ArtInk::Copper && y >= 350 && y <= 355 && dy)
                            continue; // Follow the horizontal divider through the lettering.
                        int distance = 1;
                        while (covered(x + dx * distance, y + dy * distance))
                            ++distance;
                        const auto* sample = poster_pixel(original, x + dx * distance, y + dy * distance);
                        const float weight = 1.f / (distance * (dx && dy ? 1.414f : 1.f));
                        for (unsigned c = 0; c < 3; ++c)
                            color[c] += sample[c] * weight;
                        total_weight += weight;
                    }
                    for (unsigned c = 0; c < 3; ++c)
                        pixel[c] = std::uint8_t(color[c] / total_weight + .5f);
                }
            const int fitted = std::min(text.right - text.left, font.text_width(label, text.bottom - text.top) + 2);
            text.left += (text.right - text.left - fitted) / 2;
            text.right = text.left + fitted;
            if (ink == ArtInk::Red)
            {
                draw_poster_glow(art, original, font, label, area, text, {130, 0, 0});
                return;
            }
            draw_poster_glow(art, original, font, label, area, text);
        }
    } // namespace

    const HudSurface* demo_end_screen(HudCanvas& canvas, const Resources& resources, const Localization& catalog,
                                     const LocalizedFont& font)
    {
        constexpr auto path = "screens/info.pcx";
        const auto* original = canvas.CreateSurfaceFromBitmap(path);
        if (!original || original->width != 640 || original->height != 480 || catalog.language() == Language::English ||
            !font || localization_resource_overridden(resources, path))
            return original;
        const auto* background = canvas.CreateSurfaceFromBitmap(menu_resources(MenuEdition::Demo).background);
        if (!background)
        {
            canvas.error.clear();
            return original;
        }
        auto* result = canvas.CreateSurface(640, 480);
        if (!result)
        {
            canvas.error.clear();
            return original;
        }
        *result = *original;
        localize_poster(*result, *original, *background, MenuPromotion::Info, catalog, font);
        return result;
    }

    void localize_demo_title(HudSurface& title, const Resources& resources, const Localization& catalog,
                             const LocalizedFont& font)
    {
        const auto label = catalog.text("menu.demo_version");
        if (title.width != 640 || title.height != 480 || label.empty() || !font.supports(label) ||
            localization_resource_overridden(resources, "screens/title.pcx"))
            return;
        replace_art_label(title, {74, 302, 550, 362}, {72, 309, 552, 353}, label, font, ArtInk::Splash);
    }

    void localize_loading_art(HudSurface& screen, std::string_view path, const Localization& catalog,
                              const LocalizedFont& font, const HudSurface* chapter_backdrop)
    {
        if (screen.width != 640 || screen.height != 480)
            return;
        const auto replace = [&](std::string_view key, const HudRect& area, const HudRect& text, ArtInk ink,
                                 const HudSurface* backdrop = nullptr)
        {
            const auto label = catalog.text(key);
            if (!label.empty() && font.supports(label))
                replace_art_label(screen, area, text, label, font, ink, backdrop);
        };
        if (path == "screens/loading1.pcx" || path == "screens/loading2.pcx")
            replace("loading.demo_level", {138, 306, 482, 350}, {138, 308, 480, 351}, ArtInk::Copper);
        else if (path == "interface/loadscreens/b2bumper01.pcx" || path == "interface/loadscreens/b2bumper02.pcx" ||
                 path == "interface/loadscreens/b2bumper03.pcx" || path == "interface/loadscreens/b2bumper04.pcx")
            replace("loading.chapter", {267, 303, 483, 366}, {267, 306, 479, 360}, ArtInk::Copper, chapter_backdrop);
        else if (path == "screens_ao/bumper.pcx")
            replace("loading.loading", {174, 399, 466, 455}, {171, 400, 469, 451}, ArtInk::Red);
    }

    bool localize_loading_plate(HudSurface& screen, const HudSurface& plate, const Localization& catalog,
                                const LocalizedFont& font)
    {
        const auto label = catalog.text("loading.loading");
        if (screen.width != 640 || screen.height != 480 || plate.width != 209 || plate.height != 42 || label.empty() ||
            !font.supports(label))
            return false;
        const int fitted = std::min(209, font.text_width(label, 38) + 2);
        const int left = 215 + (209 - fitted) / 2;
        draw_poster_glow(screen, plate, font, label, {0, 0, 209, 42}, {left, 322, left + fitted, 360});
        return true;
    }

    bool MenuCanvas::compose(Menu& menu, double, std::string& error)
    {
        if (menu.edition() != menu_edition(resources_))
        {
            error = "Menu/controller resource edition mismatch";
            return false;
        }
        if (!initialize(error))
            return false;
        if (composed_ == menu.revision)
            return true;
        canvas.FillRect(surface, nullptr, 0);
        if (menu.promotion != MenuPromotion::None)
        {
            const auto it = promotions_.find(menu.promotion);
            if (it == promotions_.end() || !it->second)
            {
                error = "Missing menu promotion resource";
                return false;
            }
            canvas.DrawSurfaceToSurface(surface, it->second, nullptr, 0, 0);
            if (localization && localized_font && localization->language() != Language::English &&
                !localization_resource_overridden(resources_, menu_resources(menu.edition()).promotion(menu.promotion)))
                localize_poster(*surface, *it->second, *background_, menu.promotion, *localization, *localized_font);
            ++revision;
            composed_ = menu.revision;
            return true;
        }
        const auto& assets = menu_resources(menu_edition(resources_));
        canvas.DrawSurfaceToSurface(surface, background_, nullptr, 0, 0);
        const std::string_view title_role = menu.title();
        const bool bloodbath = title_role == "bloodbath.pcx";
        if (bloodbath && !titles_.contains("bloodbath.pcx"))
        {
            // Stock retail artwork is optional for this page, and is never a solo startup requirement.
            const auto* path = assets.title(title_role);
            titles_["bloodbath.pcx"] = path ? canvas.CreateSurfaceFromBitmap(path) : nullptr;
            canvas.error.clear();
        }
        const auto title = titles_.find(menu.title());
        if (!bloodbath && (title == titles_.end() || !title->second))
        {
            error = std::string("Unavailable menu title: ") + menu.title();
            return false;
        }
        const unsigned title_id = title_role == "singleplayer.pcx" ? IDS_MENU_MAIN_SINGLEPLAYER
                                  : title_role == "options.pcx"    ? IDS_MENU_MAIN_OPTIONS
                                  : title_role == "savegame.pcx"   ? IDS_MENU_SINGLEPLAYER_SAVEGAME
                                  : title_role == "loadgame.pcx"   ? IDS_MENU_SINGLEPLAYER_LOADGAME
                                  : bloodbath                      ? IDS_MENU_TITLE_BLOODBATH
                                                                   : 0;
        const auto original_title = strings.strings.find(title_id);
        const std::string_view fallback_title = bloodbath ? "BLOODBATH" : "MAIN MENU";
        const std::string_view english_title =
            original_title == strings.strings.end() ? fallback_title : original_title->second;
        auto translated_title = title_id ? translated(title_id) : translated("menu.main_title");
        if (bloodbath && translated_title.empty())
            translated_title = translated(IDS_MENU_MAIN_BLOODBATH);
        const bool use_translation = !translated_title.empty() && translated_title != english_title &&
                                     localized_font->supports(translated_title);
        if (!title->second)
        {
            if (use_translation)
                LocalizedBitmapLine(normal_, *localized_font, translated_title)
                    .draw(*surface, {assets.title_x, assets.title_y, 625, assets.title_y + int(normal_.height)});
            else
                text(*surface, normal_, english_title, assets.title_x, assets.title_y);
        }
        else if (!use_translation)
            canvas.DrawSurfaceToSurfaceTransparent(surface, title->second, nullptr, assets.title_x, assets.title_y,
                                                   hud_magenta);
        else
            localized_font->draw_menu(*surface, translated_title,
                                      {assets.title_x, assets.title_y, 625,
                                       std::min(assets.list_y, assets.title_y + int(title->second->height))},
                                      *title->second, true);
        if (menu.page == MenuPage::Controls)
        {
            if (!compose_controls(menu, error))
                return false;
            ++revision;
            composed_ = menu.revision;
            return true;
        }
        int y = assets.list_y;
        if (menu.page == MenuPage::Quit)
        {
            const auto it = strings.strings.find(IDS_MENU_MESSAGE_QUIT);
            if (it == strings.strings.end())
            {
                error = "Missing original quit text";
                return false;
            }
            draw_small(it->second, translated(IDS_MENU_MESSAGE_QUIT), 180, y);
            y += small_.height + 12;
        }
        // Replacement rows can exceed the bounded list (the demo's 41-pixel font fits six): the
        // large-font rows then share the list height evenly. The demo's glyph cells carry a
        // margin, so its nine-row XR Options page (30 pixels per row) stays legible.
        const int rows = int(menu.items.size());
        const bool multiplayer = menu.in_multiplayer();
        const int large = int(normal_.height);
        const int step = !menu.small() && rows > 1 && y + rows * large > 425 ? (425 - y - large) / (rows - 1) : large;
        if (step < large * 2 / 3)
        {
            error = "Original menu list exceeds bounded canvas";
            return false;
        }
        const int visible = menu.small() ? std::max(1, (425 - y) / (int(small_.height) + 5)) : rows;
        if (menu.small())
            menu.first = std::clamp(menu.first, std::max(0, menu.selected - visible + 1), menu.selected);
        else
            menu.first = 0;
        for (std::size_t i = 0; i < menu.items.size(); ++i)
        {
            auto& item = menu.items[i];
            item.rect = {};
            item.slider_rect = {};
            if (int(i) < menu.first || int(i) >= menu.first + visible)
                continue;
            std::string label = item.literal;
            std::string translation;
            std::string localized_slot;
            if (localization && localized_font)
            {
                const MenuSaveRow* slot = nullptr;
                if (menu.page == MenuPage::Load)
                    slot = i == 0 ? &menu.saves.restart : i == 1 ? &menu.saves.quick :
                           i - 2 < menu.saves.slots.size() ? &menu.saves.slots[i - 2] : nullptr;
                else if (menu.page == MenuPage::Save && i < menu.saves.slots.size())
                    slot = &menu.saves.slots[i];
                if (slot)
                    localized_slot = localized_save_name(*slot, *localization, resources_);
            }
            if (item.string_id)
            {
                const auto it = strings.strings.find(item.string_id);
                if (it == strings.strings.end() && (!multiplayer || label.empty()))
                {
                    error = "Missing original menu string " + std::to_string(item.string_id);
                    return false;
                }
                if (it != strings.strings.end())
                    label = it->second;
                translation = translated(item.string_id);
                if (menu.page == MenuPage::Load)
                {
                    // The retail strings use one FormatMessage numbered insertion.
                    constexpr std::string_view placeholder = "%1!s!";
                    const auto marker = label.find(placeholder);
                    if (marker == label.npos)
                    {
                        error = "Unsupported load-menu string insertion";
                        return false;
                    }
                    const auto translated_insertion = localized_slot.empty() ? translated(item.insertion_key)
                                                                            : std::string_view(localized_slot);
                    const auto translated_marker = translation.find(placeholder);
                    if (translated_marker == translation.npos ||
                        translation.find(placeholder, translated_marker + placeholder.size()) != translation.npos)
                        translation.clear();
                    if (!translated_insertion.empty() && translation.empty())
                        translation = label;
                    if (!translation.empty())
                        translation.replace(translation.find(placeholder), placeholder.size(),
                                            translated_insertion.empty() ? std::string_view(item.insertion)
                                                                         : translated_insertion);
                    label.replace(marker, placeholder.size(), item.insertion);
                }
                if (!multiplayer)
                    item.literal = label;
            }
            if (translation.empty() && (!item.string_id || multiplayer) && !item.localization_key.empty())
                translation = translated(item.localization_key);
            else if (!item.string_id && !localized_slot.empty())
                translation = std::move(localized_slot);
            if (!item.localized_literal.empty())
                translation = item.localized_literal;
            if (item.setting >= 0)
            {
                auto setting = setting_label(menu, item.setting, localized_font ? localization : nullptr, resources_);
                if (!setting.english.empty())
                {
                    label = item.literal = std::move(setting.english);
                    translation = std::move(setting.localized);
                }
            }
            const bool enabled = menu.enabled(item);
            const auto& font = menu.small()              ? small_
                               : !enabled                ? disabled_
                               : int(i) == menu.selected ? selected_
                                                         : normal_;
            const int row_height = std::min(int(font.height), step);
            std::optional<LocalizedBitmapLine> translated_label;
            if (localized_font && !translation.empty() &&
                (item.setting != menu_language_setting || translation != label) &&
                localized_font->supports(translation))
                translated_label.emplace(font, *localized_font, translation);
            else if (multiplayer && localized_font &&
                     (width(font, label) > 445 || std::ranges::any_of(label, [](unsigned char c) { return c > 127; })))
                translated_label.emplace(font, *localized_font, label);
            const int label_width = translated_label ? translated_label->advance() : width(font, label);
            item.rect = {180, y, std::min(625, 180 + label_width), y + row_height};
            if (item.setting >= 0)
                item.rect.right = 500;
            if (y + int(font.height) > 425)
            {
                error = "Original menu list exceeds bounded canvas";
                return false;
            }
            if (translated_label)
            {
                const bool slider = item.setting == 1 || item.setting == 3 || item.setting == 4;
                const bool toggle = item.setting == 0 || item.setting == 2 || item.setting == 5 || item.setting == 29;
                const int right = slider ? 280 : toggle ? 440 : item.setting == menu_language_setting ? 500 : 625;
                translated_label->draw(*surface, {180, y, right, y + int(font.height)});
            }
            else
                text(*surface, font, label, 180, y);
            if (menu.small() && int(i) == menu.selected && enabled)
                text(*surface, small_, ">", 164, y);
            if ((item.setting >= 0 && item.setting <= 5) ||
                item.setting == 29) // Detail/lighting rows carry their value in the label.
            {
                const auto& settings = menu.settings;
                const bool toggle = item.setting == 0 || item.setting == 2 || item.setting == 5 || item.setting == 29;
                if (toggle)
                {
                    const bool on = item.setting == 0    ? settings.sound
                                    : item.setting == 2  ? settings.music
                                    : item.setting == 29 ? settings.mouse_look
                                                         : settings.invert_y;
                    draw_small(on ? "On" : "Off", translated(on ? "menu.on_short" : "menu.off_short"), 440, y);
                }
                else
                {
                    const int value = item.setting == 1   ? settings.sound_volume
                                      : item.setting == 3 ? settings.music_volume
                                                          : settings.sensitivity;
                    const int minimum = item.setting == 4 ? 100 : 0, maximum = item.setting == 4 ? 1500 : 50;
                    canvas.DrawSurfaceToSurfaceTransparent(surface, slider_, nullptr, 280, y, hud_magenta);
                    const int travel = std::max(0, int(slider_->width) - int(thumb_->width));
                    item.slider_rect = {280 + int(thumb_->width) / 2, y, 280 + int(thumb_->width) / 2 + travel,
                                        y + int(font.height)};
                    const int x = 280 + travel * (value - minimum) / (maximum - minimum);
                    canvas.DrawSurfaceToSurfaceTransparent(surface, thumb_, nullptr, x, y, hud_magenta);
                    text(*surface, small_, std::to_string(value), 460, y);
                }
            }
            if (menu.small() && (!enabled || (multiplayer && item.command < 0)))
            {
                for (int py = item.rect.top; py < item.rect.bottom; ++py)
                    for (int px = item.rect.left; px < item.rect.right; ++px)
                        for (unsigned c = 0; c < 3; ++c)
                            surface->rgba[(py * 640 + px) * 4 + c] /= 2;
            }
            y += menu.small() ? font.height + 5 : step;
        }
        if (menu.first > 0 && up_arrow_)
            canvas.DrawSurfaceToSurfaceTransparent(surface, up_arrow_, nullptr, 298 - int(up_arrow_->width) / 2,
                                                   assets.list_y - int(up_arrow_->height) - 8, hud_magenta);
        if (menu.first + visible < rows && down_arrow_)
            canvas.DrawSurfaceToSurfaceTransparent(surface, down_arrow_, nullptr, 298 - int(down_arrow_->width) / 2,
                                                   426, hud_magenta);
        if (menu.page == MenuPage::Vr && menu.video.current_refresh_rate)
        {
            const auto value = refresh_label(menu.video.current_refresh_rate);
            const auto english = "CURRENT REFRESH: " + value;
            const auto translation = localization ? localization->format("menu.current_refresh", "CURRENT REFRESH: {0}", {value})
                                                  : english;
            draw_small(english, translation == english ? std::string_view{} : translation, 180, 440);
        }
        if ((menu.page == MenuPage::Save || menu.page == MenuPage::Load) && !menu.saves.unavailable.empty())
            draw_small(menu.saves.unavailable, translated("menu.saves_unavailable"), 180, 440);
        if (menu.page == MenuPage::Cheats)
        {
            if (!menu.cheat_status.empty())
            {
                std::string translation(translated(menu.cheat_status_key));
                if (!translation.empty() && menu.cheat_status_key == "menu.cheat_keys")
                    translation = localization->format(menu.cheat_status_key, "{0} keys added.",
                                                       {std::to_string(menu.cheat_status_count)});
                draw_small(menu.cheat_status, translation == menu.cheat_status ? std::string_view{} : translation,
                           180, 440);
            }
            else if (menu.settings.cheats && !(menu.in_world && menu.alive))
                draw_small("Grey cheats need a game in progress.", translated("menu.cheat_requires_game"), 180, 440);
        }
        if (menu.demo() &&
            (menu.page == MenuPage::Options || menu.page == MenuPage::Audio || menu.page == MenuPage::Mouse))
            draw_small("Grey options are unavailable in OpenBlood2.", translated("menu.unavailable_options"), 180, 440);
        if (multiplayer)
        {
            constexpr int left = 180, right = 625;
            const auto footer_width = [&](std::string_view line)
            {
                return localized_font ? LocalizedBitmapLine(small_, *localized_font, line).advance()
                                      : width(small_, line);
            };
            const auto footer_fit = [&](std::string_view line)
            {
                if (footer_width(line) <= right - left)
                    return line.size();
                std::size_t fitted = 0, remaining = line.size();
                auto* cursor = line.data();
                while (remaining)
                {
                    SDL_StepUTF8(&cursor, &remaining);
                    const auto prefix = line.substr(0, line.size() - remaining);
                    if (footer_width(prefix) > right - left)
                        break;
                    fitted = prefix.size();
                }
                return fitted;
            };
            const auto footer = [&](std::string_view line, int top)
            {
                if (line.empty())
                    return;
                if (localized_font)
                    LocalizedBitmapLine(small_, *localized_font, line)
                        .draw(*surface, {left, top, right, top + int(small_.height)});
                else
                    text(*surface, small_, line, left, top);
            };
            const std::string_view status = menu.multiplayer_status;
            const auto newline = status.find('\n');
            const auto first_line = status.substr(0, newline);
            auto second_line = newline != status.npos ? status.substr(newline + 1) : std::string_view{};
            auto end = footer_fit(first_line);
            if (newline == status.npos && !menu.multiplayer_edit && end < first_line.size())
            {
                const auto space = first_line.rfind(' ', end);
                if (space != first_line.npos && space > end / 2)
                    end = space;
                second_line = first_line.substr(end);
                if (second_line.starts_with(' '))
                    second_line.remove_prefix(1);
            }
            footer(first_line.substr(0, end), 440);
            if (menu.multiplayer_edit)
                footer(std::string_view(*menu.multiplayer_edit).substr(0, footer_fit(*menu.multiplayer_edit)), 460);
            else if (!second_line.empty())
                footer(second_line.substr(0, footer_fit(second_line)), 460);
            else if (menu.audio_failed)
                draw_small("Menu audio unavailable - see log.", translated("menu.audio_unavailable"), 180, 460);
        }
        else if (menu.audio_failed)
            draw_small("Menu audio unavailable - see log.", translated("menu.audio_unavailable"), 180, 460);
        ++revision;
        composed_ = menu.revision;
        return true;
    }
} // namespace OpenBlood2
