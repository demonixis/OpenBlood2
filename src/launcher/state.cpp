#include "launcher/state.h"
#include <SDL3/SDL.h>
#include <array>
#include <charconv>
#include <fstream>
#include <map>
#include <set>
#include <sstream>

namespace OpenBlood2::Launcher
{
    namespace fs = std::filesystem;
    namespace
    {
        bool integer(std::string_view text, int& value)
        {
            auto r = std::from_chars(text.data(), text.data() + text.size(), value);
            return !text.empty() && r.ec == std::errc{} && r.ptr == text.data() + text.size();
        }
        bool validate(const State& state, std::string& error)
        {
            if (state.installations.size() > 32 || state.selected < -1 ||
                (state.installations.empty()
                     ? state.selected != -1
                     : state.selected < 0 || std::size_t(state.selected) >= state.installations.size()) ||
                (!state.game.empty() && (!state.game.is_absolute() || !valid_text(path_text(state.game), 4096))))
            {
                error = "Invalid launcher selection or executable path";
                return false;
            }
            std::set<std::pair<Edition, fs::path>> seen;
            for (const auto& item : state.installations)
            {
                std::error_code ec;
                const auto path = fs::weakly_canonical(item.directory, ec);
                if (!item.directory.is_absolute() || !valid_text(path_text(item.directory), 4096) || ec ||
                    (item.edition != Edition::Retail && item.edition != Edition::Demo) ||
                    !seen.emplace(item.edition, path).second)
                {
                    error = "Invalid or duplicate installation path";
                    return false;
                }
            }
            return true;
        }
    } // namespace
    bool valid_text(std::string_view text, std::size_t limit)
    {
        if (text.size() > limit)
            return false;
        for (std::size_t i = 0; i < text.size();)
        {
            const auto c = static_cast<unsigned char>(text[i++]);
            if (c < 32 || c == 127)
                return false;
            if (c < 128)
                continue;
            unsigned count = 0, value = 0, minimum = 0;
            if (c >= 0xc2 && c <= 0xdf)
            {
                count = 1;
                value = c & 31;
                minimum = 0x80;
            }
            else if (c >= 0xe0 && c <= 0xef)
            {
                count = 2;
                value = c & 15;
                minimum = 0x800;
            }
            else if (c >= 0xf0 && c <= 0xf4)
            {
                count = 3;
                value = c & 7;
                minimum = 0x10000;
            }
            else
                return false;
            if (count > text.size() - i)
                return false;
            for (unsigned n = 0; n < count; ++n)
            {
                const auto part = static_cast<unsigned char>(text[i++]);
                if ((part & 0xc0) != 0x80)
                    return false;
                value = (value << 6) | (part & 63);
            }
            if (value < minimum || value > 0x10ffff || (value >= 0xd800 && value <= 0xdfff))
                return false;
        }
        return true;
    }
    bool load_state(const fs::path& file, State& output, std::string& error)
    {
        error.clear();
        std::error_code ec;
        const bool exists = fs::exists(file, ec);
        if (ec)
        {
            error = "Cannot inspect launcher settings: " + ec.message();
            return false;
        }
        if (!exists)
        {
            output = {};
            return true;
        }
        const auto size = fs::file_size(file, ec);
        if (ec || size > 65536)
        {
            error = "Launcher settings unreadable or exceed 64 KiB";
            return false;
        }
        std::ifstream source(file, std::ios::binary);
        if (!source)
        {
            error = "Cannot open launcher settings";
            return false;
        }
        std::string contents(65537, '\0');
        source.read(contents.data(), static_cast<std::streamsize>(contents.size()));
        const auto count = static_cast<std::size_t>(source.gcount());
        if (source.bad() || count > 65536)
        {
            error = "Cannot read bounded launcher settings";
            return false;
        }
        contents.resize(count);
        std::istringstream input(contents);
        std::map<std::string, std::string> fields;
        std::string line;
        while (std::getline(input, line))
        {
            if (!line.empty() && line.back() == '\r')
                line.pop_back();
            if (line.empty() || line.front() == '#')
                continue;
            const auto eq = line.find('=');
            if (eq == line.npos || !valid_text(line, 8192) ||
                !fields.emplace(line.substr(0, eq), line.substr(eq + 1)).second)
            {
                error = "Malformed or duplicate launcher setting";
                return false;
            }
        }
        State next;
        if (input.bad() || !fields.contains("version") || fields["version"] != "1" || !fields.contains("game") ||
            !fields.contains("selected") || !integer(fields["selected"], next.selected))
        {
            error = "Incomplete or unsupported launcher settings";
            return false;
        }
        next.game = path_from_utf8(fields["game"]);
        fields.erase("version");
        fields.erase("game");
        fields.erase("selected");
        for (unsigned n = 0; n < 32; ++n)
        {
            const auto prefix = "installation." + std::to_string(n);
            const auto edition = fields.find(prefix + ".edition"), path = fields.find(prefix + ".path");
            if (edition == fields.end() && path == fields.end())
                break;
            if (edition == fields.end() || path == fields.end() ||
                (edition->second != "retail" && edition->second != "demo"))
            {
                error = "Incomplete installation or unsupported edition";
                return false;
            }
            next.installations.push_back(
                {edition->second == "demo" ? Edition::Demo : Edition::Retail, path_from_utf8(path->second)});
            fields.erase(edition);
            fields.erase(path);
        }
        if (!fields.empty())
        {
            error = "Unknown or noncontiguous launcher setting";
            return false;
        }
        if (!validate(next, error))
            return false;
        output = std::move(next);
        return true;
    }
    bool atomic_write(const fs::path& file, std::string_view text, std::string& error)
    {
        error.clear();
        std::error_code ec;
        fs::create_directories(file.parent_path(), ec);
        if (ec)
        {
            error = "Cannot create launcher directory: " + ec.message();
            return false;
        }
        // Exclusive creation prevents another writer's temporary from being truncated.
        const auto temp =
            file.parent_path() / (path_text(file.filename()) + ".tmp-" + std::to_string(SDL_GetTicksNS()));
        auto* io = SDL_IOFromFile(path_text(temp).c_str(), "wbx");
        if (!io)
        {
            error = SDL_GetError();
            return false;
        }
        const bool written = SDL_WriteIO(io, text.data(), text.size()) == text.size();
        const bool closed = SDL_CloseIO(io);
        const bool renamed = written && closed && SDL_RenamePath(path_text(temp).c_str(), path_text(file).c_str());
        if (!renamed)
        {
            error = "Cannot publish launcher file: " + std::string(SDL_GetError());
            fs::remove(temp, ec);
        }
        return renamed;
    }
    bool save_state(const fs::path& file, const State& state, std::string& error)
    {
        error.clear();
        State previous;
        if (!validate(state, error) || !load_state(file, previous, error))
            return false;
        std::string text =
            "version=1\ngame=" + path_text(state.game) + "\nselected=" + std::to_string(state.selected) + '\n';
        for (std::size_t i = 0; i < state.installations.size(); ++i)
        {
            const auto prefix = "installation." + std::to_string(i);
            const auto& item = state.installations[i];
            text += prefix + ".edition=" + (item.edition == Edition::Demo ? "demo" : "retail") + '\n';
            text += prefix + ".path=" + path_text(item.directory) + '\n';
        }
        if (text.size() > 65536)
        {
            error = "Launcher settings exceed 64 KiB";
            return false;
        }
        return atomic_write(file, text, error);
    }
} // namespace OpenBlood2::Launcher
