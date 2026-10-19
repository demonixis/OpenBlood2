#include "launcher/artwork.h"
#include "engine/config.h"
#include "engine/resources.h"
#include <fstream>
#include <string>

namespace OpenBlood2::Launcher
{
    bool decode_artwork(std::span<const std::uint8_t> b, std::vector<std::uint8_t>& rgba)
    {
        bool valid = true;
        auto range = [](std::size_t p, std::size_t n, std::size_t size) { return p <= size && n <= size - p; };
        auto u16 = [&](std::size_t p) -> std::uint16_t
        {
            if (!range(p, 2, b.size()))
            {
                valid = false;
                return 0;
            }
            return b[p] | (std::uint16_t(b[p + 1]) << 8);
        };
        auto u32 = [&](std::size_t p) -> std::uint32_t
        {
            const auto low = u16(p);
            return low | (std::uint32_t(u16(p + 2)) << 16);
        };
        if (b.size() < 64 || b.size() > 4 * 1024 * 1024 || u16(0) != 0x5a4d)
            return false;
        const auto pe = u32(60);
        if (!range(pe, 24, b.size()) || u32(pe) != 0x4550 || u16(pe + 24) != 0x10b)
            return false;
        const auto count = u16(pe + 6), optional = u16(pe + 20);
        if (!count || count > 96 || optional < 120 || !range(pe + 24ull, optional + count * 40ull, b.size()))
            return false;
        auto map = [&](std::uint32_t rva, std::uint32_t size, std::size_t& offset)
        {
            unsigned matches = 0;
            for (unsigned i = 0; i < count; ++i)
            {
                const auto section = pe + 24ull + optional + i * 40;
                const auto base = u32(section + 12), bytes = u32(section + 16), raw = u32(section + 20);
                if (rva >= base && range(rva - base, size, bytes) && range(raw, bytes, b.size()))
                {
                    offset = std::size_t(raw) + rva - base;
                    ++matches;
                }
            }
            return valid && matches == 1;
        };
        const auto rva = u32(pe + 24 + 112), bytes = u32(pe + 24 + 116);
        std::size_t root = 0;
        if (bytes < 16 || !map(rva, bytes, root))
            return false;
        auto child = [&](std::uint32_t directory, unsigned depth, std::uint32_t& output)
        {
            if (!range(directory, 16, bytes))
                return false;
            const unsigned n = unsigned(u16(root + directory + 12)) + u16(root + directory + 14);
            if (!n || n > 4096 || !range(directory + 16ull, n * 8ull, bytes))
                return false;
            unsigned matches = 0, best = 0;
            for (unsigned i = 0; i < n; ++i)
            {
                const auto at = root + directory + 16 + i * 8;
                const auto name = u32(at), value = u32(at + 4);
                bool match = depth == 0 ? name == 2 : depth == 2;
                if (depth == 1 && (name & 0x80000000u))
                {
                    const auto text = name & 0x7fffffffu;
                    if (!range(text, 2, bytes))
                        return false;
                    const auto length = u16(root + text);
                    if (!range(text + 2ull, length * 2ull, bytes))
                        return false;
                    std::string label;
                    if (length <= 6)
                        for (unsigned j = 0; j < length; ++j)
                        {
                            const auto c = u16(root + text + 2 + 2 * j);
                            if (c > 127)
                                return false;
                            label += char(c);
                        }
                    const unsigned priority = label == "GAME1" ? 2 : label == "BLOOD2" ? 1 : 0;
                    match = priority && priority >= best;
                    if (priority > best)
                    {
                        matches = 0;
                        best = priority;
                    }
                }
                if (match)
                {
                    output = value;
                    ++matches;
                }
            }
            return valid && matches == 1;
        };
        std::uint32_t offset = 0;
        for (unsigned depth = 0; depth < 3; ++depth)
        {
            std::uint32_t next = 0;
            if (!child(offset, depth, next) || bool(next & 0x80000000u) != (depth < 2))
                return false;
            offset = next & 0x7fffffffu;
        }
        if (!range(offset, 16, bytes))
            return false;
        std::size_t dib = 0;
        const auto dib_size = u32(root + offset + 4);
        if (dib_size != 40 + 1024 + 320 * 240 || !map(u32(root + offset), dib_size, dib))
            return false;
        if (u32(dib) != 40 || u32(dib + 4) != 320 || u32(dib + 8) != 240 || u16(dib + 12) != 1 || u16(dib + 14) != 8 ||
            u32(dib + 16) != 0 || (u32(dib + 32) != 0 && u32(dib + 32) != 256) || !valid)
            return false;
        std::vector<std::uint8_t> next(320 * 240 * 4);
        for (unsigned y = 0; y < 240; ++y)
            for (unsigned x = 0; x < 320; ++x)
            {
                const auto palette = dib + 40 + 4 * b[dib + 1064 + (239 - y) * 320 + x];
                const auto out = (y * 320 + x) * 4;
                next[out] = b[palette + 2];
                next[out + 1] = b[palette + 1];
                next[out + 2] = b[palette];
                next[out + 3] = 255;
            }
        rgba = std::move(next);
        return true;
    }
    bool load_artwork(const std::filesystem::path& directory, std::vector<std::uint8_t>& rgba)
    {
        namespace fs = std::filesystem;
        std::error_code ec;
        fs::path found;
        for (auto it = fs::directory_iterator(directory, ec); !ec && it != fs::directory_iterator{}; it.increment(ec))
        {
            std::string name;
            if (resource_path(path_text(it->path().filename()), name) && (name == "blood2.exe" || name == "b2demo.exe"))
            {
                if (!found.empty())
                    return false;
                found = it->path();
            }
        }
        if (ec || found.empty())
            return false;
        const auto size = fs::file_size(found, ec);
        if (ec || size > 4 * 1024 * 1024)
            return false;
        std::ifstream input(found, std::ios::binary);
        std::vector<std::uint8_t> bytes(size);
        if (!input.read(reinterpret_cast<char*>(bytes.data()), std::streamsize(size)))
            return false;
        return decode_artwork(bytes, rgba);
    }
} // namespace OpenBlood2::Launcher
