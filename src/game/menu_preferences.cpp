#include "game/menu_preferences.h"
#include "engine/atomic_file.h"
#include <SDL3/SDL.h>
#include <algorithm>
#include <array>
#include <charconv>
#include <fstream>
namespace OpenBlood2
{
    namespace
    {
        // Version 1 has the first seven keys; version 2 adds the display settings; version 3 adds
        // the renderer selection.
        constexpr std::array<const char*, 12> names{
            "version",  "sound",          "music",    "sound_volume", "music_volume", "sensitivity",
            "invert_y", "detail",         "pixel_lighting", "renderer", "quality",     "normal_maps"};
        constexpr std::array<std::size_t, 3> version_keys{7, 9, 12};
        bool valid(const MenuSettings& s)
        {
            return s.sound_volume >= 0 && s.sound_volume <= 50 && s.music_volume >= 0 && s.music_volume <= 50 &&
                   s.sensitivity >= 100 && s.sensitivity <= 1500 && s.detail >= 0 && s.detail <= 2 &&
                   (s.renderer == 0 || s.renderer == 1) && (s.quality == 0 || s.quality == 1);
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
        std::array<int, names.size()> values{};
        std::array<bool, names.size()> seen{};
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
        const bool known = values[0] >= 1 && values[0] <= int(version_keys.size());
        const std::size_t required = known ? version_keys[values[0] - 1] : 0;
        auto flag = [&](std::size_t key) { return key >= required || (values[key] >= 0 && values[key] <= 1); };
        if (input.bad() || !known || std::find(seen.begin(), seen.begin() + required, false) != seen.begin() + required ||
            std::find(seen.begin() + required, seen.end(), true) != seen.end() || !flag(1) || !flag(2) || !flag(6) ||
            !flag(8) || !flag(11))
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
        if (required >= version_keys[1])
        {
            result.detail = values[7];
            result.pixel_lighting = values[8];
        }
        if (required >= version_keys[2])
        {
            result.renderer = values[9];
            result.quality = values[10];
            result.normal_maps = values[11];
        }
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
        const std::array<int, names.size()> values{3,
                                                   settings.sound,
                                                   settings.music,
                                                   settings.sound_volume,
                                                   settings.music_volume,
                                                   settings.sensitivity,
                                                   settings.invert_y,
                                                   settings.detail,
                                                   settings.pixel_lighting,
                                                   settings.renderer,
                                                   settings.quality,
                                                   settings.normal_maps};
        std::string text;
        for (unsigned i = 0; i < values.size(); ++i)
            text += std::string(names[i]) + '=' + std::to_string(values[i]) + '\n';
        return write_file_atomic(
            path, std::span(reinterpret_cast<const std::uint8_t*>(text.data()), text.size()), "preferences", error);
    }
} // namespace OpenBlood2
