#include "engine/atomic_file.h"
#include "engine/config.h"
#include <SDL3/SDL.h>
#include <fstream>

namespace OpenBlood2
{
    bool write_file_atomic(const std::filesystem::path& path, std::span<const std::uint8_t> bytes,
                           std::string_view what, std::string& error, const AtomicWriteOptions& options)
    {
        error.clear();
        std::error_code ec;
        std::filesystem::create_directories(path.parent_path(), ec);
        if (ec)
        {
            error = "Cannot create " + std::string(what) + " directory: " + ec.message();
            return false;
        }
        // A unique sibling prevents partial readers and collisions between running instances.
        const auto prefix = path_text(path.filename()) + ".tmp-";
        const auto temp = path.parent_path() / (prefix + std::to_string(SDL_GetTicksNS()));
        std::ofstream out(temp, std::ios::binary | std::ios::trunc);
        if (!bytes.empty())
            out.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
        out.flush();
        const bool written = bool(out);
        out.close();
        if (!written || out.fail() || !SDL_RenamePath(path_text(temp).c_str(), path_text(path).c_str()))
        {
            error = "Cannot atomically write " + std::string(what) + ": " + std::string(SDL_GetError());
            std::filesystem::remove(temp, ec);
            return false;
        }
        if (options.remove_stale_temporaries)
        {
            // Best effort: a stale file that cannot be removed is ignored again by the next write.
            auto it = std::filesystem::directory_iterator(path.parent_path(), ec);
            for (auto end = std::filesystem::directory_iterator{}; !ec && it != end; it.increment(ec))
            {
                const auto name = path_text(it->path().filename());
                std::error_code ignored;
                if (name.size() > prefix.size() && name.starts_with(prefix) && it->is_regular_file(ignored))
                    std::filesystem::remove(it->path(), ignored);
            }
        }
        return true;
    }
    bool read_file_bounded(const std::filesystem::path& path, std::uint64_t limit, std::vector<std::uint8_t>& bytes,
                           std::string& error)
    {
        bytes.clear();
        error.clear();
        std::error_code ec;
        if (!std::filesystem::is_regular_file(path, ec) || ec)
        {
            error = "Cannot open " + path_text(path.filename());
            return false;
        }
        const auto size = std::filesystem::file_size(path, ec);
        if (ec || size > limit)
        {
            error = ec ? "Cannot inspect " + path_text(path.filename())
                       : path_text(path.filename()) + " exceeds the " + std::to_string(limit) + "-byte limit";
            return false;
        }
        std::ifstream input(path, std::ios::binary);
        std::vector<std::uint8_t> data(static_cast<std::size_t>(size));
        if (!input || (size && !input.read(reinterpret_cast<char*>(data.data()), static_cast<std::streamsize>(size))) ||
            input.peek() != std::char_traits<char>::eof())
        {
            error = "Cannot read " + path_text(path.filename()) + " (changed while reading)";
            return false;
        }
        bytes = std::move(data);
        return true;
    }
} // namespace OpenBlood2
