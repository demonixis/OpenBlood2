#include "engine/resources.h"
#include "engine/formats/reader.h"
#include <algorithm>
#include <array>
#include <fstream>
#include <functional>
#include <set>

namespace OpenBlood2
{
    namespace
    {
        constexpr std::size_t header_size = 168, max_metadata = 32 * 1024 * 1024;
        using Read = std::function<bool(std::uint32_t, std::size_t, std::vector<std::uint8_t>&)>;
        bool index_rez(std::uint64_t length, const Read& read, std::vector<Resource>& output, std::string& error)
        {
            output.clear();
            error.clear();
            std::vector<Resource> entries;
            std::vector<std::uint8_t> header;
            if (length < header_size || !read(0, header_size, header) || header[0] != 13 || header[1] != 10 ||
                header[62] != 13 || header[63] != 10 || header[124] != 13 || header[125] != 10 || header[126] != 26)
            {
                error = "REZ: invalid/truncated v1 header";
                return false;
            }
            Reader h(std::span<const std::uint8_t>(header).subspan(127));
            if (h.u32() != 1)
            {
                error = "REZ: unsupported version (only v1)";
                return false;
            }
            const auto root = h.u32(), root_size = h.u32();
            std::map<std::uint32_t, std::uint32_t> ranges;
            std::set<std::string> names;
            std::size_t metadata = 0, directories = 0;
            auto range = [&](std::uint32_t offset, std::uint32_t size)
            { return (size == 0 || offset >= header_size) && offset <= length && size <= length - offset; };
            auto directory = [&](auto&& self, std::uint32_t offset, std::uint32_t size, const std::string& prefix,
                                 unsigned depth) -> bool
            {
                if (depth > 32 || ++directories > 16384 || !range(offset, size) || size > max_metadata - metadata)
                    return false;
                if (!size)
                    return true; // Empty directories may share offset zero.
                auto next = ranges.lower_bound(offset);
                if ((next != ranges.end() && std::uint64_t(offset) + size > next->first) ||
                    (next != ranges.begin() &&
                     std::uint64_t(std::prev(next)->first) + std::prev(next)->second > offset))
                    return false;
                ranges.emplace(offset, size);
                metadata += size;
                std::vector<std::uint8_t> data;
                if (!read(offset, size, data))
                    return false;
                Reader r(data);
                while (r.good() && r.remaining())
                {
                    const auto kind = r.u32(), pos = r.u32(), bytes = r.u32();
                    r.u32();
                    if (kind > 1 || !range(pos, bytes))
                        return false;
                    if (kind == 1)
                    {
                        const auto raw = r.cstring(255);
                        std::string name;
                        if (!r.good() || !resource_path(raw, name) || name.find('/') != std::string::npos ||
                            prefix.size() + name.size() > 1024 || !names.insert(prefix + name + '/').second ||
                            !self(self, pos, bytes, prefix + name + '/', depth + 1))
                            return false;
                    }
                    else
                    {
                        r.u32();
                        const auto extension = r.u32(), keys = r.u32();
                        auto raw = r.cstring(255);
                        r.cstring(4096);
                        if (keys > 4096 || !r.skip(std::size_t(keys) * 4))
                            return false;
                        std::string ext;
                        for (int shift = 24; shift >= 0; shift -= 8)
                        {
                            const char c = static_cast<char>((extension >> shift) & 255);
                            if (c)
                                ext.push_back(c);
                        }
                        if (!ext.empty())
                            raw += '.' + ext;
                        std::string name;
                        if (!resource_path(raw, name) || name.find('/') != std::string::npos ||
                            prefix.size() + name.size() > 1024 || !names.insert(prefix + name).second ||
                            entries.size() >= 100000)
                            return false;
                        entries.push_back({prefix + name, {}, pos, bytes, 0});
                    }
                }
                return r.good();
            };
            if (!directory(directory, root, root_size, "", 0))
            {
                error = "REZ: malformed directory, range, duplicate path, cycle or budget exceeded";
                return false;
            }
            output = std::move(entries);
            return true;
        }
        bool file_read(std::ifstream& file, std::uint32_t offset, std::size_t size, std::vector<std::uint8_t>& out)
        {
            out.resize(size);
            file.clear();
            file.seekg(offset);
            return file && (size == 0 ||
                            bool(file.read(reinterpret_cast<char*>(out.data()), static_cast<std::streamsize>(size))));
        }
    } // namespace
    bool resource_path(std::string_view input, std::string& output)
    {
        output.clear();
        if (input.empty() || input.size() > 1024)
            return false;
        for (unsigned char c : input)
        {
            if (c < 32 || c == ':' || c == 127)
                return false;
            if (c == '\\')
                c = '/';
            if (c >= 'A' && c <= 'Z')
                c += 'a' - 'A';
            output.push_back(static_cast<char>(c));
        }
        std::size_t start = 0;
        while (start <= output.size())
        {
            auto end = output.find('/', start);
            if (end == std::string::npos)
                end = output.size();
            const auto part = std::string_view(output).substr(start, end - start);
            if (part.empty() || part == "." || part == "..")
            {
                output.clear();
                return false;
            }
            if (end == output.size())
                break;
            start = end + 1;
        }
        return true;
    }
    bool read_rez(std::span<const std::uint8_t> bytes, std::vector<Resource>& entries, std::string& error)
    {
        return index_rez(
            bytes.size(),
            [&](std::uint32_t off, std::size_t n, std::vector<std::uint8_t>& out)
            {
                if (off > bytes.size() || n > bytes.size() - off)
                    return false;
                out.assign(bytes.begin() + off, bytes.begin() + off + n);
                return true;
            },
            entries, error);
    }
    bool Resources::mount(const std::filesystem::path& path, std::string& error)
    {
        if (mounts_.size() >= 32)
        {
            error = "REZ: mount limit";
            return false;
        }
        std::error_code ec;
        auto size = std::filesystem::file_size(path, ec);
        std::ifstream file(path, std::ios::binary);
        std::vector<Resource> entries;
        if (ec || !file)
        {
            error = "REZ: cannot open " + path_text(path);
            return false;
        }
        if (!index_rez(size, [&](auto off, auto n, auto& out) { return file_read(file, off, n, out); }, entries, error))
            return false;
        // Only publish a complete validated index. Later mounts replace exact paths.
        for (auto& e : entries)
        {
            e.archive = path;
            auto found = index_.find(e.path);
            if (found != index_.end())
                e.overrides = found->second.overrides + 1;
            index_.insert_or_assign(e.path, e);
        }
        mounts_.push_back({path, entries.size()});
        return true;
    }
    bool Resources::mount_game(const Config& config, std::string& error)
    {
        Resources next;
        std::error_code ec;
        std::map<std::string, std::filesystem::path> files;
        auto it = std::filesystem::directory_iterator(config.data_dir, ec);
        if (ec)
        {
            error = "Cannot enumerate content directory";
            return false;
        }
        for (auto end = std::filesystem::directory_iterator{}; it != end; it.increment(ec))
        {
            if (ec)
            {
                error = "Cannot enumerate content directory";
                return false;
            }
            std::string key;
            if (resource_path(path_text(it->path().filename()), key))
            {
                if (!files.emplace(key, it->path()).second)
                {
                    error = "Ambiguous content filename: " + key;
                    return false;
                }
            }
        }
        if (ec)
        {
            error = "Cannot enumerate content directory";
            return false;
        }
        static constexpr std::array<const char*, 4> full{"blood2.rez", "sound.rez", "blood2p.rez", "blood2p2.rez"};
        static constexpr std::array<const char*, 1> demo{"b2demo.rez"};
        const auto archives = config.demo ? std::span<const char* const>(demo) : std::span<const char* const>(full);
        for (const char* name : archives)
        {
            const auto found = files.find(name);
            if (found == files.end())
            {
                if (name == archives.front()) // The first archive is required.
                {
                    error = config.demo ? "Missing B2DEMO.REZ" : "Missing BLOOD2.REZ";
                    return false;
                }
            }
            else if (!next.mount(found->second, error))
                return false;
        }
        if (config.overlays.size() > 24)
        {
            error = "Too many mod archives";
            return false;
        }
        std::vector<std::filesystem::path> mounted;
        for (const auto& mount : next.mounts())
        {
            auto path = std::filesystem::canonical(mount.path, ec);
            if (ec)
            {
                error = "Cannot resolve base archive";
                return false;
            }
            mounted.push_back(std::move(path));
        }
        for (const auto& overlay : config.overlays)
        {
            std::string name;
            auto canonical = std::filesystem::canonical(overlay, ec);
            if (ec || !overlay.is_absolute() || !resource_path(path_text(canonical.filename()), name) ||
                !name.ends_with(".rez") || name == "blood2nm.rez")
            {
                error = "Mod must be a data-only REZ archive; Nightmares is unsupported";
                return false;
            }
            for (const auto& previous : mounted)
            {
                const bool same = std::filesystem::equivalent(previous, canonical, ec);
                if (ec || same)
                {
                    error = "Duplicate/base mod archive";
                    return false;
                }
            }
            Resources check;
            if (!check.mount(canonical, error))
                return false;
            if (check.index().empty())
            {
                error = "Empty mod archive";
                return false;
            }
            for (const auto& [key, resource] : check.index())
            {
                (void)resource;
                if (key.ends_with(".dll") || key.ends_with(".exe"))
                {
                    error = "Mods with game code are unsupported: " + key;
                    return false;
                }
            }
            if (!next.mount(canonical, error))
                return false;
            mounted.push_back(std::move(canonical));
        }
        next.content_root_ = config.data_dir;
        next.demo_ = config.demo;
        *this = std::move(next);
        return true;
    }
    const Resource* Resources::find(std::string_view path) const
    {
        std::string key;
        if (!resource_path(path, key))
            return nullptr;
        const auto it = index_.find(key);
        return it == index_.end() ? nullptr : &it->second;
    }
    bool Resources::read(std::string_view path, std::vector<std::uint8_t>& bytes, std::string& error) const
    {
        bytes.clear();
        error.clear();
        const auto* entry = find(path);
        if (!entry)
        {
            error = "Resource not found: " + std::string(path);
            return false;
        }
        if (entry->size > 128 * 1024 * 1024)
        {
            error = "Resource exceeds 128 MiB budget";
            return false;
        }
        std::error_code ec;
        auto size = std::filesystem::file_size(entry->archive, ec);
        if (ec || entry->offset > size || entry->size > size - entry->offset)
        {
            error = "Resource archive changed/truncated";
            return false;
        }
        std::ifstream file(entry->archive, std::ios::binary);
        if (!file || !file_read(file, entry->offset, entry->size, bytes))
        {
            bytes.clear();
            error = "Resource read failed";
            return false;
        }
        return true;
    }
} // namespace OpenBlood2
