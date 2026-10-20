#include "game/menu.h"
#include "game/original/MenuIds.h"
#include <algorithm>
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
                    label.replace(marker, 5, "unavailable");
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
            const bool enabled = menu.enabled(item);
            const auto& font = menu.small()              ? small_
                               : !enabled                ? disabled_
                               : int(i) == menu.selected ? selected_
                                                         : normal_;
            item.rect = {180, y, std::min(625, 180 + width(font, label)), y + int(font.height)};
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
            y += font.height + (menu.small() ? 5 : 0);
        }
        if (menu.page == MenuPage::Save || menu.page == MenuPage::Load)
            text(*surface, small_, "Saving and loading are not available yet.", 180, 440);
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
