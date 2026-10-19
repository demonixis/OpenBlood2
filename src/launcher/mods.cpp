#include "launcher/mods.h"
#include <fstream>
#include <map>
#include <set>

namespace OpenBlood2::Launcher
{
    namespace fs = std::filesystem;
    namespace
    {
        bool validate(const std::vector<Mod>& mods, std::string& error)
        {
            if (mods.size() > 24)
            {
                error = "At most 24 mod archives can be registered";
                return false;
            }
            std::set<std::string> seen;
            for (const auto& mod : mods)
            {
                std::error_code ec;
                const auto path = fs::weakly_canonical(mod.archive, ec);
                if (ec || !mod.archive.is_absolute() || !mod.installation.directory.is_absolute() ||
                    !valid_text(path_text(mod.archive), 4096) ||
                    !valid_text(path_text(mod.installation.directory), 4096) ||
                    (mod.installation.edition != Edition::Demo && mod.installation.edition != Edition::Retail) ||
                    !seen.insert(std::to_string(int(mod.installation.edition)) + "\n" +
                                 path_text(mod.installation.directory) + "\n" + path_text(path))
                         .second)
                {
                    error = "Invalid or duplicate mod archive";
                    return false;
                }
            }
            return true;
        }
    } // namespace
    bool load_mods(const fs::path& file, std::vector<Mod>& mods, std::string& error)
    {
        error.clear();
        std::error_code ec;
        const bool exists = fs::exists(file, ec);
        if (!exists && !ec)
        {
            mods.clear();
            return true;
        }
        const auto size = fs::file_size(file, ec);
        if (ec || size > 65536)
        {
            error = "Cannot read bounded mods.cfg";
            return false;
        }
        std::ifstream input(file, std::ios::binary);
        std::string text(size, '\0');
        if (!input.read(text.data(), std::streamsize(size)) || input.peek() != EOF)
        {
            error = "Cannot read mods.cfg";
            return false;
        }
        std::map<std::string, std::string> fields;
        std::string_view remaining(text);
        while (!remaining.empty())
        {
            const auto end = remaining.find('\n');
            auto line = remaining.substr(0, end);
            remaining.remove_prefix(end == remaining.npos ? remaining.size() : end + 1);
            if (!line.empty() && line.back() == '\r')
                line.remove_suffix(1);
            if (line.empty() || line.starts_with('#'))
                continue;
            const auto equals = line.find('=');
            if (equals == line.npos || !valid_text(line, 4200) ||
                !fields.emplace(std::string(line.substr(0, equals)), std::string(line.substr(equals + 1))).second)
            {
                error = "Malformed mods.cfg";
                return false;
            }
        }
        if (fields["version"] != "1" || (fields.size() - 1) % 4 || fields.size() > 97)
        {
            error = "Unsupported mods.cfg format";
            return false;
        }
        std::vector<Mod> next;
        const auto count = (fields.size() - 1) / 4;
        for (std::size_t i = 0; i < count; ++i)
        {
            const auto prefix = "mod." + std::to_string(i) + ".";
            const auto edition = fields[prefix + "edition"], enabled = fields[prefix + "enabled"];
            if ((edition != "demo" && edition != "retail") || (enabled != "0" && enabled != "1") ||
                !fields.contains(prefix + "path") || !fields.contains(prefix + "root"))
            {
                error = "Incomplete mod record";
                return false;
            }
            next.push_back(
                {{edition == "demo" ? Edition::Demo : Edition::Retail, path_from_utf8(fields[prefix + "root"])},
                 path_from_utf8(fields[prefix + "path"]),
                 enabled == "1"});
        }
        if (fields.size() != count * 4 + 1 || !validate(next, error))
            return false;
        mods = std::move(next);
        return true;
    }
    bool save_mods(const fs::path& file, const std::vector<Mod>& mods, std::string& error)
    {
        std::vector<Mod> previous;
        if (!load_mods(file, previous, error) || !validate(mods, error))
            return false;
        std::string text = "version=1\n";
        for (std::size_t i = 0; i < mods.size(); ++i)
        {
            const auto prefix = "mod." + std::to_string(i) + ".";
            const auto& mod = mods[i];
            text += prefix + "edition=" + (mod.installation.edition == Edition::Demo ? "demo\n" : "retail\n");
            text += prefix + "root=" + path_text(mod.installation.directory) + "\n";
            text += prefix + "path=" + path_text(mod.archive) + "\n";
            text += prefix + "enabled=" + (mod.enabled ? "1\n" : "0\n");
        }
        if (text.size() > 65536)
        {
            error = "mods.cfg exceeds 64 KiB";
            return false;
        }
        return atomic_write(file, text, error);
    }
    std::vector<fs::path> enabled_mods(const std::vector<Mod>& mods, const Installation& item)
    {
        std::vector<fs::path> paths;
        for (const auto& mod : mods)
            if (mod.enabled && mod.installation == item)
                paths.push_back(mod.archive);
        return paths;
    }
} // namespace OpenBlood2::Launcher
