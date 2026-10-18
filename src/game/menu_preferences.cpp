#include "game/menu_preferences.h"
#include <SDL3/SDL.h>
#include <algorithm>
#include <array>
#include <charconv>
#include <fstream>
namespace OpenBlood2
{
    namespace
    {
        constexpr std::array<const char*, 7> names{"version",      "sound",       "music",   "sound_volume",
                                                   "music_volume", "sensitivity", "invert_y"};
        bool valid(const MenuSettings& s)
        {
            return s.sound_volume >= 0 && s.sound_volume <= 50 && s.music_volume >= 0 && s.music_volume <= 50 &&
                   s.sensitivity >= 100 && s.sensitivity <= 1500;
        }
    } // namespace
    bool menu_preferences_path(std::filesystem::path& output, std::string& error)
    {
        auto* path = SDL_GetPrefPath("OpenBlood2", "Blood2");
        if (!path)
        {
            error = SDL_GetError();
            return false;
        }
        output = path_from_utf8(path) / "preferences.cfg";
        SDL_free(path);
        return true;
    }
    bool load_menu_preferences(const std::filesystem::path& path, MenuSettings& output, std::string& error)
    {
        error.clear();
        std::error_code ec;
        const bool exists = std::filesystem::exists(path, ec);
        if (ec)
        {
            error = "Cannot inspect preferences: " + ec.message();
            return false;
        }
        if (!exists)
            return true;
        const auto size = std::filesystem::file_size(path, ec);
        if (ec || size > 4096)
        {
            error = "Cannot read preferences (4 KiB limit)";
            return false;
        }
        std::ifstream input(path);
        if (!input)
        {
            error = "Cannot open preferences";
            return false;
        }
        std::array<int, 7> values{};
        std::array<bool, 7> seen{};
        std::string line;
        while (std::getline(input, line))
        {
            if (!line.empty() && line.back() == '\r')
                line.pop_back();
            if (line.empty() || line.front() == '#')
                continue;
            const auto equal = line.find('=');
            unsigned key = 0;
            for (; key < names.size(); ++key)
                if (line.substr(0, equal) == names[key])
                    break;
            if (key == names.size() || equal == line.npos || seen[key])
            {
                error = "Unknown or duplicate preferences key";
                return false;
            }
            auto first = line.data() + equal + 1, last = line.data() + line.size();
            const auto parsed = std::from_chars(first, last, values[key]);
            if (parsed.ec != std::errc{} || parsed.ptr != last)
            {
                error = "Invalid preferences value";
                return false;
            }
            seen[key] = true;
        }
        if (input.bad() || std::find(seen.begin(), seen.end(), false) != seen.end() || values[0] != 1 ||
            values[1] < 0 || values[1] > 1 || values[2] < 0 || values[2] > 1 || values[6] < 0 || values[6] > 1)
        {
            error = "Incomplete or unsupported preferences";
            return false;
        }
        MenuSettings result;
        result.sound = values[1];
        result.music = values[2];
        result.sound_volume = values[3];
        result.music_volume = values[4];
        result.sensitivity = values[5];
        result.invert_y = values[6];
        if (!valid(result))
        {
            error = "Preferences out of range";
            return false;
        }
        output = result;
        return true;
    }
    bool save_menu_preferences(const std::filesystem::path& path, const MenuSettings& settings, std::string& error)
    {
        error.clear();
        if (!valid(settings))
        {
            error = "Preferences out of range";
            return false;
        }
        std::error_code ec;
        std::filesystem::create_directories(path.parent_path(), ec);
        if (ec)
        {
            error = "Cannot create preferences directory: " + ec.message();
            return false;
        }
        // A unique sibling prevents partial readers and collisions between running instances.
        const auto temp =
            path.parent_path() / (path_text(path.filename()) + ".tmp-" + std::to_string(SDL_GetTicksNS()));
        const std::array<int, 7> values{1,
                                        settings.sound,
                                        settings.music,
                                        settings.sound_volume,
                                        settings.music_volume,
                                        settings.sensitivity,
                                        settings.invert_y};
        std::ofstream out(temp, std::ios::binary | std::ios::trunc);
        for (unsigned i = 0; i < values.size(); ++i)
            out << names[i] << '=' << values[i] << '\n';
        out.flush();
        const bool written = bool(out);
        out.close();
        if (!written || out.fail() || !SDL_RenamePath(path_text(temp).c_str(), path_text(path).c_str()))
        {
            error = "Cannot atomically write preferences: " + std::string(SDL_GetError());
            std::filesystem::remove(temp, ec);
            return false;
        }
        return true;
    }
} // namespace OpenBlood2
