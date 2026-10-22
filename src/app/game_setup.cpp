#include "app/game_setup.h"
#include "engine/atomic_file.h"
#include "engine/user_directory.h"
#include <SDL3/SDL_filesystem.h>
#include <SDL3/SDL_platform.h>
#if defined(SDL_PLATFORM_ANDROID)
#include <SDL3/SDL_system.h>
#include <jni.h>
#endif
#include <algorithm>
#include <map>
#include <set>

namespace OpenBlood2
{
    namespace fs = std::filesystem;
    namespace
    {
        constexpr std::uint64_t file_limit = 64 * 1024;
        const char* edition_name(Edition edition)
        {
            return edition == Edition::Demo ? "demo" : "retail";
        }
        bool edition_from(std::string_view text, Edition& edition)
        {
            if (text != "demo" && text != "retail")
                return false;
            edition = text == "demo" ? Edition::Demo : Edition::Retail;
            return true;
        }
        bool valid_installation(const Installation& item)
        {
            return item.directory.is_absolute() && valid_setup_text(path_text(item.directory), 4096) &&
                   (item.edition == Edition::Retail || item.edition == Edition::Demo);
        }
        bool validate(const SetupState& state, std::string& error)
        {
            error = "Invalid or duplicate setup entry";
            if (state.added.size() > max_setup_installations || state.mods.size() > max_setup_mods)
            {
                error = "Too many setup entries";
                return false;
            }
            if (state.selected && !valid_installation(*state.selected))
                return false;
            std::set<std::pair<int, std::string>> added;
            for (const auto& item : state.added)
                if (!valid_installation(item) || !added.emplace(int(item.edition), path_text(item.directory)).second)
                    return false;
            std::set<std::string> mods;
            for (const auto& mod : state.mods)
                if (!valid_installation(mod.installation) || !mod.archive.is_absolute() ||
                    !valid_setup_text(path_text(mod.archive), 4096) ||
                    !mods.insert(std::string(edition_name(mod.installation.edition)) + "\n" +
                                 path_text(mod.installation.directory) + "\n" + path_text(mod.archive))
                         .second)
                    return false;
            error.clear();
            return true;
        }
        std::string lower(std::string text)
        {
            std::transform(text.begin(), text.end(), text.begin(),
                           [](unsigned char c) { return char(c >= 'A' && c <= 'Z' ? c + 32 : c); });
            return text;
        }
    } // namespace

    bool valid_setup_text(std::string_view text, std::size_t limit)
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

    bool setup_path(fs::path& output, std::string& error)
    {
        fs::path directory;
        if (!user_directory(directory, error))
            return false;
        output = directory / "setup.cfg";
        return true;
    }

    bool load_setup(const fs::path& file, SetupState& output, std::string& error)
    {
        error.clear();
        std::error_code ec;
        if (!fs::exists(file, ec) && !ec)
        {
            output = {};
            return true;
        }
        std::vector<std::uint8_t> bytes;
        if (!read_file_bounded(file, file_limit, bytes, error))
            return false;
        std::map<std::string, std::string> fields;
        std::string_view remaining(reinterpret_cast<const char*>(bytes.data()), bytes.size());
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
            if (equals == line.npos || !valid_setup_text(line, 4200) ||
                !fields.emplace(std::string(line.substr(0, equals)), std::string(line.substr(equals + 1))).second)
            {
                error = "Malformed setup.cfg";
                return false;
            }
        }
        const auto take = [&](const std::string& key, std::string& value)
        {
            const auto found = fields.find(key);
            if (found == fields.end())
                return false;
            value = std::move(found->second);
            fields.erase(found);
            return true;
        };
        std::string version;
        if (!take("version", version) || version != "1")
        {
            error = "Unsupported setup.cfg version";
            return false;
        }
        SetupState next;
        std::string edition, path, root, enabled;
        const auto installation = [&](const std::string& prefix, Installation& item)
        {
            return take(prefix + "edition", edition) && take(prefix + "path", path) &&
                   edition_from(edition, item.edition) && (item.directory = path_from_utf8(path), true);
        };
        if (fields.contains("selected.edition") || fields.contains("selected.path"))
        {
            Installation selected;
            if (!installation("selected.", selected))
            {
                error = "Incomplete selected installation in setup.cfg";
                return false;
            }
            next.selected = selected;
        }
        for (std::size_t i = 0; fields.contains("added." + std::to_string(i) + ".path"); ++i)
        {
            Installation item;
            if (i >= max_setup_installations || !installation("added." + std::to_string(i) + ".", item))
            {
                error = "Incomplete added installation in setup.cfg";
                return false;
            }
            next.added.push_back(std::move(item));
        }
        for (std::size_t i = 0; fields.contains("mod." + std::to_string(i) + ".path"); ++i)
        {
            const auto prefix = "mod." + std::to_string(i) + ".";
            SetupMod mod;
            if (i >= max_setup_mods || !take(prefix + "edition", edition) || !take(prefix + "root", root) ||
                !take(prefix + "path", path) || !take(prefix + "enabled", enabled) ||
                !edition_from(edition, mod.installation.edition) || (enabled != "0" && enabled != "1"))
            {
                error = "Incomplete mod record in setup.cfg";
                return false;
            }
            mod.installation.directory = path_from_utf8(root);
            mod.archive = path_from_utf8(path);
            mod.enabled = enabled == "1";
            next.mods.push_back(std::move(mod));
        }
        if (!fields.empty())
        {
            error = "Unknown setup.cfg key: " + fields.begin()->first;
            return false;
        }
        if (!validate(next, error))
            return false;
        output = std::move(next);
        return true;
    }

    bool save_setup(const fs::path& file, const SetupState& state, std::string& error)
    {
        if (!validate(state, error))
            return false;
        std::string text = "version=1\n";
        if (state.selected)
        {
            text += std::string("selected.edition=") + edition_name(state.selected->edition) + "\n";
            text += "selected.path=" + path_text(state.selected->directory) + "\n";
        }
        for (std::size_t i = 0; i < state.added.size(); ++i)
        {
            const auto prefix = "added." + std::to_string(i) + ".";
            text += prefix + "edition=" + edition_name(state.added[i].edition) + "\n";
            text += prefix + "path=" + path_text(state.added[i].directory) + "\n";
        }
        for (std::size_t i = 0; i < state.mods.size(); ++i)
        {
            const auto prefix = "mod." + std::to_string(i) + ".";
            const auto& mod = state.mods[i];
            text += prefix + "edition=" + edition_name(mod.installation.edition) + "\n";
            text += prefix + "root=" + path_text(mod.installation.directory) + "\n";
            text += prefix + "path=" + path_text(mod.archive) + "\n";
            text += prefix + "enabled=" + (mod.enabled ? "1\n" : "0\n");
        }
        if (text.size() > file_limit)
        {
            error = "setup.cfg exceeds 64 KiB";
            return false;
        }
        return write_file_atomic(file, std::span(reinterpret_cast<const std::uint8_t*>(text.data()), text.size()),
                                 "setup.cfg", error);
    }

    std::vector<Installation> folder_installations(const fs::path& directory)
    {
        bool retail = false, demo = false;
        std::error_code ec;
        for (auto it = fs::directory_iterator(directory, ec); !ec && it != fs::directory_iterator{}; it.increment(ec))
        {
            const auto name = lower(path_text(it->path().filename()));
            retail |= name == "blood2.rez";
            demo |= name == "b2demo.rez";
        }
        std::vector<Installation> result;
        const auto absolute = fs::absolute(directory, ec).lexically_normal();
        if (retail)
            result.push_back({Edition::Retail, absolute});
        if (demo)
            result.push_back({Edition::Demo, absolute});
        return result;
    }

    fs::path setup_content_folder(const fs::path& user)
    {
        return user / "content";
    }

    std::vector<fs::path> setup_search_folders(const fs::path& user)
    {
        std::vector<fs::path> roots;
        // Downloaded demos: content/<install>/Game.
        std::error_code ec;
        for (auto it = fs::directory_iterator(setup_content_folder(user), ec); !ec && it != fs::directory_iterator{};
             it.increment(ec))
            if (!it->path().filename().string().starts_with(".") && it->is_directory(ec))
                roots.push_back(it->path() / "Game");
        std::vector<fs::path> shared;
#if defined(SDL_PLATFORM_ANDROID)
        // The shared folders sideloaded ports use, readable with all-files access.
        shared.push_back("/sdcard/openblood2");
#elif defined(SDL_PLATFORM_IOS)
        // Documents is the app's folder in the Files app and in Finder's file sharing.
        if (const char* documents = SDL_GetUserFolder(SDL_FOLDER_DOCUMENTS))
            shared.push_back(path_from_utf8(documents));
#else
        // A portable copy placed in the game folder.
        if (const char* base = SDL_GetBasePath())
            roots.push_back(path_from_utf8(base));
#endif
        for (const auto& folder : shared)
        {
            roots.push_back(folder);
            for (auto it = fs::directory_iterator(folder, ec); !ec && it != fs::directory_iterator{}; it.increment(ec))
                if (it->is_directory(ec) && !it->path().filename().string().starts_with("."))
                    roots.push_back(it->path());
        }
        return roots;
    }

    std::vector<SetupEntry> scan_setup(const SetupState& state, const fs::path& user,
                                       const std::vector<Installation>& extra)
    {
        std::vector<SetupEntry> entries;
        const auto add = [&](const Installation& item, bool added)
        {
            for (auto& entry : entries)
                if (entry.installation == item)
                {
                    entry.added |= added;
                    return;
                }
            SetupEntry entry{item, added};
            if (!inspect_data(setup_config(state, item), entry.status, entry.error) && entry.error.empty())
                entry.error = "Unreadable game data";
            entries.push_back(std::move(entry));
        };
        for (const auto& item : state.added)
            add(item, true);
        for (const auto& item : extra)
            add(item, false);
        for (const auto& folder : setup_search_folders(user))
            for (const auto& item : folder_installations(folder))
                add(item, false);
        // A selection that vanished stays listed with its refusal.
        if (state.selected)
            add(*state.selected, false);
        return entries;
    }

    Config setup_config(const SetupState& state, const Installation& item)
    {
        Config config;
        config.data_dir = item.directory;
        config.demo = item.edition == Edition::Demo;
        config.source = "setup.cfg";
        for (const auto& mod : state.mods)
            if (mod.enabled && mod.installation == item)
                config.overlays.push_back(mod.archive);
        return config;
    }

    bool setup_ready(const SetupState& state, Config& config, std::string& error)
    {
        error.clear();
        if (!state.selected)
            return false;
        DataStatus status;
        auto candidate = setup_config(state, *state.selected);
        if (!inspect_data(candidate, status, error))
            return false;
        config = std::move(candidate);
        return true;
    }

#if defined(SDL_PLATFORM_ANDROID)
    namespace
    {
        // A static Blood2Activity method without arguments; FindClass cannot see application
        // classes on native threads, the activity's class can.
        bool activity_call(const char* name, const char* signature, bool& result)
        {
            auto* env = static_cast<JNIEnv*>(SDL_GetAndroidJNIEnv());
            auto activity = static_cast<jobject>(SDL_GetAndroidActivity());
            if (!env || !activity)
                return false;
            auto type = env->GetObjectClass(activity);
            env->DeleteLocalRef(activity);
            auto method = env->GetStaticMethodID(type, name, signature);
            if (method && signature[2] == 'Z')
                result = env->CallStaticBooleanMethod(type, method);
            else if (method)
                env->CallStaticVoidMethod(type, method);
            const bool ok = method && !env->ExceptionCheck();
            env->ExceptionClear();
            env->DeleteLocalRef(type);
            return ok;
        }
    } // namespace
    bool storage_access()
    {
        bool granted = false;
        return activity_call("hasAllFilesAccess", "()Z", granted) && granted;
    }
    void request_storage_access()
    {
        bool ignored = false;
        activity_call("requestAllFilesAccess", "()V", ignored);
    }
#else
    bool storage_access()
    {
        return true;
    }
    void request_storage_access() {}
#endif
} // namespace OpenBlood2
