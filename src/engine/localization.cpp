#include "engine/localization.h"
#include "engine/resources.h"
#include <SDL3/SDL_filesystem.h>
#include <SDL3/SDL_iostream.h>
#include <algorithm>
#include <array>
#include <charconv>
#include <memory>
#include <utility>

namespace OpenBlood2
{
    namespace
    {
        constexpr std::array<std::string_view, supported_languages.size()> tags{"en", "fr", "zh-Hans", "de", "it",
                                                                                "es", "pt-BR", "ko"};
        constexpr std::array<std::string_view, supported_languages.size()> names{
            "English", "Français", "简体中文", "Deutsch", "Italiano", "Español", "Português (Brasil)", "한국어"};
        constexpr std::size_t max_catalog_size = 256 * 1024, max_entries = 2048;
        constexpr std::array<std::string_view, 10> sections{"menu", "resources", "hud", "setup", "control",
                                                           "network", "loading", "objectives", "promotions",
                                                           "dialogue"};
        constexpr std::array<std::string_view, static_cast<std::size_t>(TextKey::Count)> pilot_keys{
            "", "menu.language", "menu.setup", "menu.cheats", "menu.unavailable_options", "hud.first_person",
            "hud.third_person"};

        std::string_view trim(std::string_view value)
        {
            const auto first = value.find_first_not_of(" \t");
            if (first == value.npos)
                return {};
            return value.substr(first, value.find_last_not_of(" \t") - first + 1);
        }

        bool valid_line(std::string_view value)
        {
            const char* cursor = value.data();
            for (auto remaining = value.size(); remaining != 0;)
            {
                const auto codepoint = SDL_StepUTF8(&cursor, &remaining);
                if ((codepoint < 32 && codepoint != '\t') || (codepoint >= 127 && codepoint <= 159) ||
                    codepoint == SDL_INVALID_UNICODE_CODEPOINT || codepoint == 0x2028 || codepoint == 0x2029)
                    return false;
            }
            return true;
        }

        bool valid_key(std::string_view key)
        {
            return !key.empty() && key.size() <= 128 && key.front() != '.' && key.back() != '.' &&
                   key.find("..") == key.npos && std::ranges::all_of(key, [](char c)
                   { return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' || c == '.'; });
        }

        bool substitute(std::string_view pattern, std::span<const std::string_view> arguments, std::string& result)
        {
            result.clear();
            std::array<bool, 10> used{};
            if (arguments.size() > used.size())
                return false;
            while (!pattern.empty())
            {
                const auto brace = pattern.find_first_of("{}");
                result += pattern.substr(0, brace);
                if (brace == pattern.npos)
                    break;
                pattern.remove_prefix(brace);
                if (pattern.size() < 3 || pattern[0] != '{' || pattern[2] != '}' ||
                    pattern[1] < '0' || pattern[1] > '9')
                    return false;
                const auto index = std::size_t(pattern[1] - '0');
                if (index >= arguments.size())
                    return false;
                used[index] = true;
                result += arguments[index];
                pattern.remove_prefix(3);
            }
            return std::ranges::all_of(std::span(used).first(arguments.size()), [](bool value) { return value; });
        }
    } // namespace

    std::string_view language_tag(Language language)
    {
        const auto index = static_cast<std::size_t>(language);
        return index < tags.size() ? tags[index] : std::string_view{};
    }

    std::string_view language_name(Language language)
    {
        const auto index = static_cast<std::size_t>(language);
        return index < names.size() ? names[index] : names.front();
    }

    std::optional<Language> parse_language(std::string_view tag)
    {
        const auto entry = std::ranges::find(tags, tag);
        if (entry == tags.end())
            return std::nullopt;
        return supported_languages[entry - tags.begin()];
    }

    bool read_application_asset(std::string_view relative, std::size_t max_size, std::vector<std::uint8_t>& out,
                                std::string& error)
    {
        out.clear();
        error.clear();
        const auto* base = SDL_GetBasePath();
        if (!base)
        {
            error = SDL_GetError();
            return false;
        }
        // SDL supplies the trailing separator, including the `assets://` URI in Android APKs.
        std::string path(base);
        path += relative;
        const std::unique_ptr<SDL_IOStream, decltype(&SDL_CloseIO)> file(SDL_IOFromFile(path.c_str(), "rb"),
                                                                       SDL_CloseIO);
        if (!file)
        {
            error = SDL_GetError();
            return false;
        }
        const auto size = SDL_GetIOSize(file.get());
        if (size < 0 || static_cast<std::uint64_t>(size) > max_size)
        {
            error = "Cannot read asset within the size limit: " + path;
            return false;
        }
        out.resize(static_cast<std::size_t>(size));
        if (!out.empty() && SDL_ReadIO(file.get(), out.data(), out.size()) != out.size())
        {
            out.clear();
            error = "Incomplete application asset: " + path;
            return false;
        }
        return true;
    }

    bool localization_resource_overridden(const Resources& resources, std::string_view path)
    {
        const auto* asset = resources.find(path);
        return asset && std::ranges::any_of(resources.mounts(), [&](const Mount& mount)
        { return mount.role == MountRole::Mod && mount.path == asset->archive; });
    }

    bool Localization::load(Language language, std::string& error)
    {
        *this = {};
        error.clear();
        if (language == Language::English)
            return true;
        const auto tag = language_tag(language);
        if (tag.empty())
        {
            error = "Unknown language";
            return false;
        }
        std::vector<std::uint8_t> bytes;
        if (!read_application_asset("lang/" + std::string(tag) + ".ini", max_catalog_size, bytes, error))
            return false;
        return load_catalog(language, {reinterpret_cast<const char*>(bytes.data()), bytes.size()}, error);
    }

    bool Localization::load_catalog(Language language, std::string_view ini, std::string& error)
    {
        *this = {};
        error.clear();
        if (language == Language::English)
            return true;
        if (language_tag(language).empty() || ini.size() > max_catalog_size)
        {
            error = "Invalid language or catalog size";
            return false;
        }
        if (ini.starts_with("\xef\xbb\xbf"))
            ini.remove_prefix(3);
        Localization candidate;
        std::string_view section;
        std::array<bool, sections.size()> sections_seen{};
        std::size_t line_number = 0;
        const auto fail = [&](std::string_view reason)
        {
            error = "Catalog line " + std::to_string(line_number) + ": " + std::string(reason);
            return false;
        };
        while (!ini.empty())
        {
            ++line_number;
            const auto newline = ini.find('\n');
            auto line = ini.substr(0, newline);
            ini.remove_prefix(newline == ini.npos ? ini.size() : newline + 1);
            if (line.ends_with('\r'))
                line.remove_suffix(1);
            line = trim(line);
            if (!valid_line(line))
                return fail("invalid UTF-8 or control character");
            if (line.empty() || line.front() == '#' || line.front() == ';')
                continue;
            if (line.front() == '[')
            {
                if (!line.ends_with(']'))
                    return fail("unknown section");
                const auto header = std::ranges::find(sections, line.substr(1, line.size() - 2));
                if (header == sections.end())
                    return fail("unknown section");
                const auto index = static_cast<std::size_t>(header - sections.begin());
                if (sections_seen[index])
                    return fail("duplicate section");
                sections_seen[index] = true;
                section = *header;
                continue;
            }
            const auto equal = line.find('=');
            if (section.empty() || equal == line.npos)
                return fail("expected key=value");
            const auto key = trim(line.substr(0, equal));
            const auto value = trim(line.substr(equal + 1));
            const std::size_t limit = section == "resources" || section == "objectives" ? 4096 : 1024;
            if (value.empty() || value.size() > limit || value.find('\t') != value.npos)
                return fail("invalid value");
            if ((section == "setup" || section == "network" || section == "menu") && value.find("##") != value.npos)
                return fail("invalid widget label");
            if (candidate.labels_.size() + candidate.resource_labels_.size() >= max_entries)
                return fail("too many keys");
            if (section == "resources")
            {
                unsigned id = 0;
                if (key.empty() || key.front() == '0')
                    return fail("invalid resource ID");
                const auto end = key.data() + key.size();
                const auto parsed = std::from_chars(key.data(), end, id);
                if (parsed.ec != std::errc{} || parsed.ptr != end || id > 65535)
                    return fail("invalid resource ID");
                if (!candidate.resource_labels_.emplace(id, value).second)
                    return fail("duplicate key");
            }
            else
            {
                if (!valid_key(key))
                    return fail("invalid key");
                std::string qualified(section);
                qualified += '.';
                qualified += key;
                if (!candidate.labels_.emplace(std::move(qualified), value).second)
                    return fail("duplicate key");
            }
        }
        candidate.language_ = language;
        *this = std::move(candidate);
        return true;
    }

    std::string_view Localization::text(TextKey key, std::string_view english) const
    {
        const auto index = static_cast<std::size_t>(key);
        return index < pilot_keys.size() ? text(pilot_keys[index], english) : english;
    }

    std::string_view Localization::text(std::string_view key, std::string_view english) const
    {
        const auto found = labels_.find(key);
        return found == labels_.end() ? english : std::string_view(found->second);
    }

    std::string_view Localization::text(unsigned resource_id, std::string_view english) const
    {
        const auto found = resource_labels_.find(resource_id);
        return found == resource_labels_.end() ? english : std::string_view(found->second);
    }

    std::string_view Localization::dialogue(std::string_view wav_path) const
    {
        const auto separator = wav_path.find_last_of("/\\");
        const auto basename = wav_path.substr(separator == wav_path.npos ? 0 : separator + 1);
        if (basename.size() <= 4 || basename.size() > 132 || language_ == Language::English)
            return {};
        std::string filename(basename);
        for (char& c : filename)
            if (c >= 'A' && c <= 'Z')
                c += 'a' - 'A';
        if (!filename.ends_with(".wav"))
            return {};
        filename.resize(filename.size() - 4);
        return valid_key(filename) ? text("dialogue." + filename) : std::string_view{};
    }

    std::string Localization::format(std::string_view key, std::string_view english,
                                     std::initializer_list<std::string_view> arguments) const
    {
        std::string result;
        const auto localized = text(key);
        if ((!localized.empty() && substitute(localized, arguments, result)) || substitute(english, arguments, result))
            return result;
        return std::string(english);
    }
} // namespace OpenBlood2
