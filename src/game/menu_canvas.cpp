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
                result += c == ' ' ? std::max(1u, font.height / 4) :
                          c >= '!' && c <= '~' ? font.widths[c - '!'] : 0;
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
    }
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
        std::vector<std::uint8_t> bytes;
        const auto* dll = resources_.find("cres.dll");
        if (!dll || dll->size > 32 * 1024 * 1024 || !resources_.read("cres.dll", bytes, error) ||
            !read_string_resources(bytes, strings, error))
        {
            if (error.empty()) error = "Missing/oversized menu strings: cres.dll";
            return false;
        }
        if (!canvas.load_font("interface/fonts/MenuFont1.pcx", "interface/fonts/MenuFont1.fnt", small_) ||
            !canvas.load_font("interface/mainmenus/mm_font_01.pcx", "interface/mainmenus/mm_font.fnt", selected_, 1) ||
            !canvas.load_font("interface/mainmenus/mm_font_05.pcx", "interface/mainmenus/mm_font.fnt", normal_, 1) ||
            !canvas.load_font("interface/mainmenus/mm_font_dis.pcx", "interface/mainmenus/mm_font.fnt", disabled_, 1))
        {
            error = canvas.error;
            return false;
        }
        background_ = canvas.CreateSurfaceFromBitmap("interface/mainmenus/background.pcx");
        slider_ = canvas.CreateSurfaceFromBitmap("interface/mainmenus/SliderBar.pcx");
        thumb_ = canvas.CreateSurfaceFromBitmap("interface/mainmenus/SliderTab.pcx");
        for (const auto* name : {"mainmenu.pcx", "singleplayer.pcx", "options.pcx", "loadgame.pcx", "savegame.pcx"})
            titles_[name] = canvas.CreateSurfaceFromBitmap((std::string("interface/mainmenus/") + name).c_str());
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
        if (!initialize(error)) return false;
        if (composed_ == menu.revision) return true;
        canvas.FillRect(surface, nullptr, 0);
        canvas.DrawSurfaceToSurface(surface, background_, nullptr, 0, 0);
        canvas.DrawSurfaceToSurfaceTransparent(surface, titles_.at(menu.title()), nullptr, 82, 52, hud_magenta);
        int y = 140;
        if (menu.page == MenuPage::Quit)
        {
            const auto it = strings.strings.find(IDS_MENU_MESSAGE_QUIT);
            if (it == strings.strings.end()) { error="Missing original quit text"; return false; }
            text(*surface, small_, it->second, 180, y);
            y += small_.height + 12;
        }
        for (std::size_t i=0; i<menu.items.size(); ++i)
        {
            auto& item = menu.items[i];
            std::string label = item.literal;
            if (item.string_id)
            {
                auto it = strings.strings.find(item.string_id);
                if (it == strings.strings.end())
                { error="Missing original menu string " + std::to_string(item.string_id); return false; }
                label = it->second;
            }
            const bool enabled = item.enabled && !(item.setting == 1 && !menu.settings.sound) &&
                                 !(item.setting == 3 && !menu.settings.music);
            const auto& font = menu.small() ? small_ : !enabled ? disabled_ :
                               int(i)==menu.selected ? selected_ : normal_;
            item.rect = {180, y, std::min(625, 180 + width(font, label)), y + int(font.height)};
            if (item.setting >= 0) item.rect.right=500;
            if (y + int(font.height) > 425) { error="Original menu list exceeds bounded canvas"; return false; }
            text(*surface, font, label, 180, y);
            if (menu.small() && int(i)==menu.selected && enabled)
                text(*surface, small_, ">", 164, y);
            if (item.setting >= 0)
            {
                const auto& settings = menu.settings;
                const bool toggle = item.setting==0 || item.setting==2 || item.setting==5;
                if (toggle)
                {
                    const bool on = item.setting==0 ? settings.sound : item.setting==2 ? settings.music : settings.invert_y;
                    text(*surface, small_, on ? "On" : "Off", 440, y);
                }
                else
                {
                    const int value = item.setting==1 ? settings.sound_volume : item.setting==3 ? settings.music_volume : settings.sensitivity;
                    const int minimum=item.setting==4 ? 100 : 0, maximum=item.setting==4 ? 1500 : 50;
                    canvas.DrawSurfaceToSurfaceTransparent(surface, slider_, nullptr, 280, y, hud_magenta);
                    const int travel=std::max(0, int(slider_->width)-int(thumb_->width));
                    const int x=280 + travel * (value-minimum)/(maximum-minimum);
                    canvas.DrawSurfaceToSurfaceTransparent(surface, thumb_, nullptr, x, y, hud_magenta);
                    text(*surface, small_, std::to_string(value), 460, y);
                }
            }
            if (menu.small() && !enabled)
            {
                for (int py=item.rect.top; py<item.rect.bottom; ++py)
                    for (int px=item.rect.left; px<item.rect.right; ++px)
                        for (unsigned c=0; c<3; ++c) surface->rgba[(py*640+px)*4+c]/=2;
            }
            y += font.height + (menu.small() ? 5 : 0);
        }
        if (menu.page==MenuPage::Save || menu.page==MenuPage::Load)
            text(*surface, small_, "Saving and loading are not available yet.", 180, 440);
        ++revision;
        composed_ = menu.revision;
        return true;
    }
}
