// Selective original ClientShellDLL/BloodClientShell.cpp: GetWorldTitle and DrawLoadingScreen,
// with ClientShellDLL/LoadScreenData.cpp (g_LoadScreenData, g_LoadScreenMessages). The engine's
// screen surface is the 640x480 reference canvas; CoolFont is the HudFont adapter.
#include "game/original/LoadingScreen.h"
#include "engine/localization.h"
#include "engine/world.h"
#include "game/localized_art.h"
#include "game/localized_font.h"
#include "game/session.h"
#include <algorithm>
#include <cctype>
#include <cstdlib>

namespace OpenBlood2
{
    namespace
    {
        // g_LoadScreenData: every authored level uses the same 640x480 layout; IDs 1-34 map to
        // IDS_BUMPER_TITLE_n (2000 + n - 1) and IDS_BUMPER_TEXT_n (3000 + n - 1).
        constexpr int title_x = 350, text_x = 200, text_wrap_width = 300;
        constexpr unsigned bumper_title_ids = 2000, bumper_text_ids = 3000;
        // g_LoadScreenMessages: IDS_LOADING_MESSAGE_1..20.
        constexpr unsigned loading_message_ids = 1000, loading_messages = 20;
        bool iequal(std::string_view a, std::string_view b)
        {
            return a.size() == b.size() &&
                   std::equal(a.begin(), a.end(), b.begin(),
                              [](unsigned char x, unsigned char y) { return std::tolower(x) == std::tolower(y); });
        }
        // The engine resolves a leading separator from the resource root.
        std::string bitmap_path(std::string_view path)
        {
            while (!path.empty() && (path.front() == '\\' || path.front() == '/'))
                path.remove_prefix(1);
            return std::string(path);
        }
    } // namespace

    WorldLoadInfo world_load_info(std::string_view info, std::string_view world, bool demo)
    {
        WorldLoadInfo result;
        // sprintf(szFilename, "Worlds\\%s.dat", pWorldFile), uppercased for the demo test.
        std::string filename = "Worlds\\" + std::string(world) + ".dat";
        std::transform(filename.begin(), filename.end(), filename.begin(),
                       [](unsigned char c) { return char(std::toupper(c)); });
        for (const auto& tokens : world_info_commands(info))
            for (std::size_t i = 0; i + 1 < tokens.size(); ++i)
            {
                if (iequal(tokens[i], "worldname"))
                    result.title = tokens[i + 1];
                else if (iequal(tokens[i], "loadscreen"))
                {
                    if (demo) // _DEMO: the two demo screens, whatever the DAT names.
                        result.load_screen = filename.find("DEMO_02") != std::string::npos ? "\\screens\\loading2.pcx"
                                                                                           : "\\screens\\loading1.pcx";
                    else
                        result.load_screen = "Interface\\LoadScreens\\" + tokens[i + 1];
                }
                else if (iequal(tokens[i], "loadscreenID"))
                    result.load_screen_id = unsigned(std::atoi(tokens[i + 1].c_str()));
            }
        if (result.title.empty())
            result.title = world;
        return result;
    }

    bool LoadingScreen::lookup(unsigned id, std::string& out, std::string& error) const
    {
        if (strings_)
            if (const auto found = strings_->strings.find(id); found != strings_->strings.end())
            {
                out = found->second;
                return true;
            }
        error = "cres.dll lacks loading-screen string " + std::to_string(id);
        return false;
    }

    void LoadingScreen::draw_label(HudSurface& surface, std::string_view english, std::string_view translation,
                                   int x, int y, std::optional<std::uint32_t> color) const
    {
        if (!translation.empty() && localized_font && localized_font->supports(translation))
        {
            const auto h = int(title_font_.height);
            const auto available = 2 * std::min(x, int(width) - x) - 16;
            const LocalizedBitmapLine label(title_font_, *localized_font, translation);
            const auto label_width = std::min(color ? localized_font->text_width(translation, h) : label.advance(),
                                              available);
            const auto left = x - label_width / 2;
            const HudRect row{left, y, left + label_width, y + h};
            if (color)
                localized_font->draw(surface, translation, row, *color);
            else
                label.draw(surface, row);
        }
        else if (color)
            title_font_.draw_solid(surface, english, x, y, 1, *color);
        else
            title_font_.draw(surface, english, x, y, 1);
    }

    bool LoadingScreen::begin(std::string_view world, bool action, std::string& error)
    {
        active_ = false;
        error.clear();
        const bool demo = resources_.demo();
        // GetWorldInfoString reads the header alone; a missing world keeps the fallback title
        // (the load itself then reports it).
        std::string path, info;
        if (world_resource_path(world, resources_.nightmares(), path))
        {
            std::vector<std::uint8_t> header;
            std::string ignored;
            if (resources_.find(path) && resources_.read(path, header, ignored, 16 + 65536))
                read_world_info(header, info, ignored);
        }
        info_ = world_load_info(info, world, demo);
        translated_resources_ = localization && !localization_resource_overridden(resources_, "cres.dll");
        // _ADDON StartNewWorld: a nightmare level always shows the add-on bumper, whatever the DAT
        // names (its load-screen ID still applies).
        if (resources_.nightmares() && addon_world_name(world))
            info_.load_screen = "Screens_ao\\Bumper.pcx";
        if (info_.load_screen_id > max_load_screen_id)
        {
            error = "Load screen ID " + std::to_string(info_.load_screen_id) + " exceeds the original table";
            return false;
        }
        if (!canvas_)
        {
            canvas_ = std::make_unique<HudCanvas>(resources_);
            if (!canvas_->load_font("interface/statusbar/fonts/stat_font_1.pcx",
                                    "interface/statusbar/fonts/stat_font_1.fnt", title_font_) ||
                !canvas_->load_font("interface/objectives/obj_font_1.pcx", "interface/objectives/obj_font_1.fnt",
                                    text_font_))
            {
                error = canvas_->error;
                canvas_.reset();
                return false;
            }
            // The menus' quarter-height space adapter (CoolFont is unavailable).
            title_font_.space_width = title_font_.height / 4;
            text_font_.space_width = text_font_.height / 4;
        }
        if (!demo && info_.load_screen_id && !strings_)
        {
            StringResources strings;
            std::vector<std::uint8_t> bytes;
            const auto* dll = resources_.find("cres.dll");
            if (!dll || dll->size > 32 * 1024 * 1024)
            {
                error = "Missing/oversized loading-screen strings: cres.dll";
                return false;
            }
            if (!resources_.read("cres.dll", bytes, error) || !read_string_resources(bytes, strings, error))
                return false;
            strings_ = std::move(strings);
        }
        // m_hLoadingScreen: the authored screen (or LoadScreen.pcx with Loading_01.pcx at 215, 321)
        // stretched to the screen.
        canvas_->error.clear();
        const auto screen_path =
            bitmap_path(info_.load_screen.empty() ? "Interface\\LoadScreens\\LoadScreen.pcx" : info_.load_screen);
        const auto* source = canvas_->CreateSurfaceFromBitmap(screen_path.c_str());
        if (!source)
        {
            error = canvas_->error;
            return false;
        }
        HudSurface temp = *source;
        const bool translate_art = localization && localized_font && localization->language() != Language::English &&
                                   !localization_resource_overridden(resources_, screen_path);
        if (info_.load_screen.empty())
        {
            constexpr auto plate = "interface/loadscreens/loadingtext/loading_01.pcx";
            const auto* text = canvas_->CreateSurfaceFromBitmap(plate);
            if (!text)
            {
                error = canvas_->error;
                return false;
            }
            if (!translate_art || localization_resource_overridden(resources_, plate) ||
                !localize_loading_plate(temp, *text, *localization, *localized_font))
                hud_blit(temp, *text, nullptr, 215, 321, false, 0);
        }
        background_ = {width, height, std::vector<std::uint8_t>(std::size_t(width) * height * 4)};
        hud_scale(background_, temp);
        std::string art_path;
        if (translate_art && resource_path(screen_path, art_path))
        {
            constexpr auto chapter_background = "interface/mainmenus/background.pcx";
            const auto* backdrop = art_path.starts_with("interface/loadscreens/b2bumper") &&
                                           !localization_resource_overridden(resources_, chapter_background)
                                       ? canvas_->CreateSurfaceFromBitmap(chapter_background)
                                       : nullptr;
            if (!backdrop)
                canvas_->error.clear();
            localize_loading_art(background_, art_path, *localization, *localized_font, backdrop);
        }
        // The title, bumper title and text drawn on m_hLoadingScreen. _DEMO and ID 0 draw the
        // centred title only.
        const auto h = int(title_font_.height);
        std::string_view translated_world;
        if (localization && !localization_resource_overridden(resources_, path))
            translated_world = localization->world(path);
        if (demo || !info_.load_screen_id)
            draw_label(background_, info_.title, translated_world, int(width / 2), h * 2);
        else
        {
            std::string title, text;
            const auto title_id = bumper_title_ids + info_.load_screen_id - 1;
            if (!lookup(title_id, title, error))
                return false;
            draw_label(background_, info_.title, translated_world, title_x, h * 2);
            draw_label(background_, title, translated_resources_ ? localization->text(title_id) : std::string_view{},
                       title_x, h * 3);
            if (!action) // && the screen is at least 512x384, always true for the reference screen.
            {
                const auto text_id = bumper_text_ids + info_.load_screen_id - 1;
                if (!lookup(text_id, text, error))
                    return false;
                const auto translation = translated_resources_ ? localization->text(text_id) : std::string_view{};
                if (!translation.empty() && localized_font && localized_font->supports(translation))
                    localized_font->draw_wrapped(background_, translation,
                        {text_x, h * 5, text_x + text_wrap_width, int(height) - h}, int(text_font_.height), 0xffffff,
                        &text_font_);
                else
                    text_font_.draw_format(background_, text, text_x, h * 5, text_wrap_width);
            }
        }
        active_ = true;
        played_waiting_sound = false;
        return draw(false, 0, error);
    }

    bool LoadingScreen::draw(bool waiting, double time, std::string& error)
    {
        error.clear();
        if (!active_)
        {
            error = "No loading screen";
            return false;
        }
        screen_ = background_;
        ++revision_;
        // _DEMO and ID 0 have no bottom line. (This source revision's _DEMO branch never blits its
        // surface; the demo's own loading1/loading2 screens are shown as retail ID 0 shows its own.)
        if (resources_.demo() || !info_.load_screen_id)
            return true;
        const auto x = int(width / 2), y = int(height - title_font_.height);
        if (waiting)
        {
            if (time - fade_time_ >= 1.0)
            {
                fade_time_ = time;
                fade_up_ = !fade_up_;
            }
            const auto calc = float(time - fade_time_);
            const float r = fade_up_ ? 0.776f + 0.224f * calc : 1.000f - 0.224f * calc;
            const float g = fade_up_ ? 0.647f - 0.647f * calc : 0.647f * calc;
            const float b = fade_up_ ? 0.518f - 0.518f * calc : 0.518f * calc;
            const auto channel = [](float v) { return std::uint32_t(std::clamp(v, 0.f, 1.f) * 255.f); };
            const auto prompt = localization ? localization->text("loading.continue") : std::string_view{};
            draw_label(screen_, "Press a key to continue...", prompt, x, y,
                       (channel(r) << 16) | (channel(g) << 8) | channel(b));
            return true;
        }
        // GetRandom(0, MAX_LOADING_MESSAGES - 1) on every draw.
        random_state = random_state * 1664525u + 1013904223u;
        message_id = loading_message_ids + (random_state >> 16) % loading_messages;
        std::string message;
        if (!lookup(message_id, message, error))
            return false;
        draw_label(screen_, message, translated_resources_ ? localization->text(message_id) : std::string_view{}, x, y);
        return true;
    }
} // namespace OpenBlood2
