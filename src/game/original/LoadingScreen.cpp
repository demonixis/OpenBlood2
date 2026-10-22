// Selective original ClientShellDLL/BloodClientShell.cpp: GetWorldTitle and DrawLoadingScreen,
// with ClientShellDLL/LoadScreenData.cpp (g_LoadScreenData, g_LoadScreenMessages). The engine's
// screen surface is the 640x480 reference canvas; CoolFont is the HudFont adapter.
#include "game/original/LoadingScreen.h"
#include "engine/world.h"
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

    bool LoadingScreen::begin(std::string_view world, bool action, std::string& error)
    {
        active_ = false;
        error.clear();
        const bool demo = resources_.demo();
        // GetWorldInfoString reads the header alone; a missing world keeps the fallback title
        // (the load itself then reports it).
        std::string path, info;
        if (resource_path(world, path))
        {
            if (!path.starts_with("worlds/"))
                path = "worlds/" + path;
            if (!path.ends_with(".dat"))
                path += ".dat";
            std::vector<std::uint8_t> header;
            std::string ignored;
            if (resources_.find(path) && resources_.read(path, header, ignored, 16 + 65536))
                read_world_info(header, info, ignored);
        }
        info_ = world_load_info(info, world, demo);
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
        const auto* source = canvas_->CreateSurfaceFromBitmap(
            bitmap_path(info_.load_screen.empty() ? "Interface\\LoadScreens\\LoadScreen.pcx" : info_.load_screen)
                .c_str());
        if (!source)
        {
            error = canvas_->error;
            return false;
        }
        HudSurface temp = *source;
        if (info_.load_screen.empty())
        {
            const auto* text = canvas_->CreateSurfaceFromBitmap("interface/loadscreens/loadingtext/loading_01.pcx");
            if (!text)
            {
                error = canvas_->error;
                return false;
            }
            hud_blit(temp, *text, nullptr, 215, 321, false, 0);
        }
        background_ = {width, height, std::vector<std::uint8_t>(std::size_t(width) * height * 4)};
        hud_scale(background_, temp);
        // The title, bumper title and text drawn on m_hLoadingScreen. _DEMO and ID 0 draw the
        // centred title only.
        const auto h = int(title_font_.height);
        if (demo || !info_.load_screen_id)
            title_font_.draw(background_, info_.title, int(width / 2), h * 2, 1);
        else
        {
            std::string title, text;
            if (!lookup(bumper_title_ids + info_.load_screen_id - 1, title, error))
                return false;
            title_font_.draw(background_, info_.title, title_x, h * 2, 1);
            title_font_.draw(background_, title, title_x, h * 3, 1);
            if (!action) // && the screen is at least 512x384, always true for the reference screen.
            {
                if (!lookup(bumper_text_ids + info_.load_screen_id - 1, text, error))
                    return false;
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
            title_font_.draw_solid(screen_, "Press a key to continue...", x, y, 1,
                                   (channel(r) << 16) | (channel(g) << 8) | channel(b));
            return true;
        }
        // GetRandom(0, MAX_LOADING_MESSAGES - 1) on every draw.
        random_state = random_state * 1664525u + 1013904223u;
        message_id = loading_message_ids + (random_state >> 16) % loading_messages;
        std::string message;
        if (!lookup(message_id, message, error))
            return false;
        title_font_.draw(screen_, message, x, y, 1);
        return true;
    }
} // namespace OpenBlood2
