#include "game/menu.h"
#include "game/original/MenuIds.h"
#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>

namespace OpenBlood2
{
    namespace
    {
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
        const auto& assets = menu_resources(resources_.demo() ? MenuEdition::Demo : MenuEdition::Retail);
        if (!load_menu_strings(resources_, strings, error))
            return false;
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
            const int label = width(small_, item.literal);
            const int x = control ? std::max(0, left + 100 - label) : left;
            item.rect = {left, y, 625, y + row};
            text(*surface, small_, item.literal, x, y);
            if (control && i == menu.binding)
                text(*surface, small_, "=", left + 111, y);
            if (control && i < int(menu.controls.size()))
            {
                std::string value = menu.controls[i].binding;
                if (value.empty())
                    value = unassigned->second;
                // The bound text stops at the canvas edge instead of overflowing it.
                while (!value.empty() && left + 131 + width(small_, value) > 635)
                    value.pop_back();
                text(*surface, small_, value, left + 131, y);
            }
            if (i == menu.selected && menu.enabled(item))
                text(*surface, small_, ">", left - 16, y);
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
            text(*surface, small_, "Menu audio unavailable - see log.", 180, 460);
        return true;
    }
    bool MenuCanvas::compose(Menu& menu, double, std::string& error)
    {
        if (menu.demo() != resources_.demo())
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
            ++revision;
            composed_ = menu.revision;
            return true;
        }
        const auto& assets = menu_resources(resources_.demo() ? MenuEdition::Demo : MenuEdition::Retail);
        canvas.DrawSurfaceToSurface(surface, background_, nullptr, 0, 0);
        const auto title = titles_.find(menu.title());
        if (title == titles_.end() || !title->second)
        {
            error = std::string("Unavailable menu title: ") + menu.title();
            return false;
        }
        canvas.DrawSurfaceToSurfaceTransparent(surface, title->second, nullptr, assets.title_x, assets.title_y,
                                               hud_magenta);
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
            text(*surface, small_, it->second, 180, y);
            y += small_.height + 12;
        }
        // Replacement rows can exceed the bounded list (the demo's 41-pixel font fits six): the
        // large-font rows then share the list height evenly. The demo's glyph cells carry a
        // margin, so its nine-row XR Options page (30 pixels per row) stays legible.
        const int rows = int(menu.items.size());
        const int large = int(normal_.height);
        const int step = !menu.small() && rows > 1 && y + rows * large > 425 ? (425 - y - large) / (rows - 1) : large;
        if (step < large * 2 / 3)
        {
            error = "Original menu list exceeds bounded canvas";
            return false;
        }
        for (std::size_t i = 0; i < menu.items.size(); ++i)
        {
            auto& item = menu.items[i];
            std::string label = item.literal;
            if (item.string_id)
            {
                auto it = strings.strings.find(item.string_id);
                if (it == strings.strings.end())
                {
                    error = "Missing original menu string " + std::to_string(item.string_id);
                    return false;
                }
                label = it->second;
                if (menu.page == MenuPage::Load)
                {
                    // Original cres.dll uses FormatMessage's numbered string insertion.
                    const auto marker = label.find("%1!s!");
                    if (marker == label.npos)
                    {
                        error = "Unsupported load-menu string insertion";
                        return false;
                    }
                    label.replace(marker, 5, item.insertion);
                }
                item.literal = label;
            }
            // Replacement options, styled after the original DETAIL LEVEL row.
            const auto& display = menu.settings;
            if (item.setting == 7)
                label = item.literal = display.renderer ? (display.quality ? "LIGHTING [HIGH]" : "LIGHTING [STANDARD]")
                                       : display.pixel_lighting ? "LIGHTING [PER PIXEL]"
                                                                : "LIGHTING [PER VERTEX]";
            else if (item.setting == 8)
                label = item.literal = display.renderer ? "RENDERER [ENHANCED]" : "RENDERER [ORIGINAL]";
            else if (item.setting == 9)
                label = item.literal = display.normal_maps ? "NORMAL MAPS [GENERATED]" : "NORMAL MAPS [OFF]";
            else if (item.setting == 17)
            {
                static constexpr std::array<const char*, 4> modes{"ANTI-ALIASING [OFF]", "ANTI-ALIASING [FXAA]",
                                                                  "ANTI-ALIASING [MSAA 2X]", "ANTI-ALIASING [MSAA 4X]"};
                label = item.literal = modes[std::size_t(std::clamp(display.antialiasing, 0, 3))];
            }
            else if (item.setting == 18)
                label = item.literal = display.bloom ? "BLOOM [ON]" : "BLOOM [OFF]";
            else if (item.setting == 19)
            {
                static constexpr std::array<const char*, 3> modes{"TONEMAP [OFF]", "TONEMAP [NEUTRAL]",
                                                                  "TONEMAP [FILMIC]"};
                label = item.literal = modes[std::size_t(std::clamp(display.tonemap, 0, 2))];
            }
            else if (item.setting == 20)
            {
                static constexpr std::array<const char*, 3> modes{
                    "AMBIENT OCCLUSION [OFF]", "AMBIENT OCCLUSION [LOW]", "AMBIENT OCCLUSION [HIGH]"};
                label = item.literal = modes[std::size_t(std::clamp(display.ambient_occlusion, 0, 2))];
            }
            else if (item.setting == 21)
            {
                static constexpr std::array<const char*, 3> modes{"REFLECTIONS [OFF]", "REFLECTIONS [LOW]",
                                                                  "REFLECTIONS [HIGH]"};
                label = item.literal = modes[std::size_t(std::clamp(display.reflections, 0, 2))];
            }
            else if (item.setting == 10)
                label = item.literal = display.vr_turn ? "TURN [SNAP " + std::to_string(display.vr_turn) + "]"
                                                       : std::string("TURN [SMOOTH]");
            else if (item.setting == 11)
                label = item.literal = display.vr_laser ? "AIM LASER [ON]" : "AIM LASER [OFF]";
            else if (item.setting == 12)
                label = item.literal = display.vr_crosshair ? "CROSSHAIR [ON]" : "CROSSHAIR [OFF]";
            else if (item.setting == 13)
                label = item.literal = display.vr_move_hand ? "MOVE [RIGHT HAND]" : "MOVE [HEAD]";
            else if (item.setting == 14)
                label = item.literal = display.cheats ? "CHEATS [ON]" : "CHEATS [OFF]";
            else if (item.setting == 15)
                label = item.literal = display.god_mode ? "GOD MODE [ON]" : "GOD MODE [OFF]";
            else if (item.setting == 16)
            {
                std::string world = menu.level >= 0 && menu.level < int(menu.levels.size())
                                        ? menu.levels[std::size_t(menu.level)]
                                        : std::string("NONE");
                for (auto& c : world)
                    c = char(std::toupper(static_cast<unsigned char>(c)));
                label = item.literal = "LEVEL [" + world + "]";
            }
            const bool enabled = menu.enabled(item);
            const auto& font = menu.small()              ? small_
                               : !enabled                ? disabled_
                               : int(i) == menu.selected ? selected_
                                                         : normal_;
            item.rect = {180, y, std::min(625, 180 + width(font, label)), y + std::min(int(font.height), step)};
            if (item.setting >= 0)
                item.rect.right = 500;
            if (y + int(font.height) > 425)
            {
                error = "Original menu list exceeds bounded canvas";
                return false;
            }
            text(*surface, font, label, 180, y);
            if (menu.small() && int(i) == menu.selected && enabled)
                text(*surface, small_, ">", 164, y);
            if (item.setting >= 0 && item.setting <= 5) // Detail/lighting rows carry their value in the label.
            {
                const auto& settings = menu.settings;
                const bool toggle = item.setting == 0 || item.setting == 2 || item.setting == 5;
                if (toggle)
                {
                    const bool on = item.setting == 0   ? settings.sound
                                    : item.setting == 2 ? settings.music
                                                        : settings.invert_y;
                    text(*surface, small_, on ? "On" : "Off", 440, y);
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
            if (menu.small() && !enabled)
            {
                for (int py = item.rect.top; py < item.rect.bottom; ++py)
                    for (int px = item.rect.left; px < item.rect.right; ++px)
                        for (unsigned c = 0; c < 3; ++c)
                            surface->rgba[(py * 640 + px) * 4 + c] /= 2;
            }
            y += menu.small() ? font.height + 5 : step;
        }
        if ((menu.page == MenuPage::Save || menu.page == MenuPage::Load) && !menu.saves.unavailable.empty())
            text(*surface, small_, menu.saves.unavailable, 180, 440);
        if (menu.page == MenuPage::Cheats)
            text(*surface, small_,
                 !menu.cheat_status.empty() ? menu.cheat_status
                 : menu.settings.cheats && !(menu.in_world && menu.alive)
                     ? std::string("Grey cheats need a game in progress.")
                     : std::string{},
                 180, 440);
        if (menu.demo() &&
            (menu.page == MenuPage::Options || menu.page == MenuPage::Audio || menu.page == MenuPage::Mouse))
            text(*surface, small_, "Grey options are unavailable in OpenBlood2.", 180, 440);
        if (menu.audio_failed)
            text(*surface, small_, "Menu audio unavailable - see log.", 180, 460);
        ++revision;
        composed_ = menu.revision;
        return true;
    }
} // namespace OpenBlood2
