#include "engine/string_resources.h"
#include "engine/formats/reader.h"
#include <algorithm>
#include <set>
#include <vector>
namespace OpenBlood2
{
    namespace
    {
        struct Section
        {
            std::uint32_t rva, size, offset;
        };
        struct Entry
        {
            std::uint32_t id, offset;
        };
        struct Block
        {
            std::uint32_t id, offset, size;
            std::uint16_t language;
        };
        bool range(std::size_t offset, std::size_t length, std::size_t size)
        {
            return offset <= size && length <= size - offset;
        }
        bool directory(std::span<const std::uint8_t> bytes, std::uint32_t offset, std::vector<Entry>& entries)
        {
            if (!range(offset, 16, bytes.size()))
                return false;
            Reader r(bytes.subspan(offset));
            r.skip(12);
            const auto named = r.u16(), ids = r.u16();
            if (std::size_t(named) + ids > 4096 || !r.skip(std::size_t(named) * 8))
                return false;
            std::set<std::uint32_t> unique;
            for (unsigned i = 0; i < ids; ++i)
            {
                const auto id = r.u32(), child = r.u32();
                if (!r.good() || id > 65535 || !unique.insert(id).second)
                    return false;
                entries.push_back({id, child});
            }
            return r.good();
        }
        bool utf8(Reader& r, unsigned count, std::string& out)
        {
            if (count > 4096)
                return false;
            for (unsigned i = 0; i < count; ++i)
            {
                std::uint32_t c = r.u16();
                if (!c || (c >= 0xdc00 && c <= 0xdfff))
                    return false;
                if (c >= 0xd800 && c <= 0xdbff)
                {
                    if (++i == count)
                        return false;
                    const auto low = r.u16();
                    if (low < 0xdc00 || low > 0xdfff)
                        return false;
                    c = 0x10000 + ((c - 0xd800) << 10) + low - 0xdc00;
                }
                if (c < 0x80)
                    out += static_cast<char>(c);
                else if (c < 0x800)
                {
                    out += static_cast<char>(0xc0 | (c >> 6));
                    out += static_cast<char>(0x80 | (c & 63));
                }
                else
                {
                    if (c >= 0x10000)
                    {
                        out += static_cast<char>(0xf0 | (c >> 18));
                        out += static_cast<char>(0x80 | ((c >> 12) & 63));
                    }
                    else
                        out += static_cast<char>(0xe0 | (c >> 12));
                    out += static_cast<char>(0x80 | ((c >> 6) & 63));
                    out += static_cast<char>(0x80 | (c & 63));
                }
            }
            return r.good();
        }
        // Validated PE image: resource directory bytes plus RVA-to-file mapping.
        struct Image
        {
            std::vector<Section> sections;
            std::span<const std::uint8_t> resources;
            bool map(std::uint32_t rva, std::uint32_t size, std::uint32_t& offset) const
            {
                unsigned matches = 0;
                for (const auto& section : sections)
                    if (rva >= section.rva && range(rva - section.rva, size, section.size))
                    {
                        offset = section.offset + (rva - section.rva);
                        ++matches;
                    }
                return matches == 1;
            }
        };
        bool image(std::span<const std::uint8_t> bytes, Image& out)
        {
            if (bytes.size() < 64 || bytes.size() > 32 * 1024 * 1024 || bytes[0] != 'M' || bytes[1] != 'Z')
                return false;
            Reader dos(bytes.subspan(60));
            const auto pe = dos.u32();
            if (pe < 64 || !range(pe, 24, bytes.size()))
                return false;
            Reader header(bytes.subspan(pe));
            if (header.u32() != 0x4550)
                return false;
            header.u16(); // Machine is irrelevant: code is never executed.
            const auto count = header.u16();
            header.skip(12);
            const auto optional = header.u16();
            header.u16();
            if (!count || count > 96 || optional < 120 || !range(pe + 24ull, optional + count * 40ull, bytes.size()))
                return false;
            Reader opt(header.take(optional));
            const auto magic = opt.u16();
            if (magic != 0x10b && magic != 0x20b)
                return false;
            opt.skip(magic == 0x10b ? 90 : 106);
            if (opt.u32() < 3 || !opt.skip(16))
                return false;
            const auto resource_rva = opt.u32(), resource_size = opt.u32();
            if (!opt.good() || resource_size < 16 || resource_size > 16 * 1024 * 1024)
                return false;
            for (unsigned i = 0; i < count; ++i)
            {
                header.skip(12);
                const auto rva = header.u32(), size = header.u32(), offset = header.u32();
                header.skip(16);
                if (!header.good() || !range(offset, size, bytes.size()))
                    return false;
                out.sections.push_back({rva, size, offset});
            }
            std::uint32_t resource_offset = 0;
            if (!out.map(resource_rva, resource_size, resource_offset))
                return false;
            out.resources = bytes.subspan(resource_offset, resource_size);
            return true;
        }
        // Named entries of one directory level (IMAGE_RESOURCE_DIR_STRING_U), ASCII-uppercased
        // like the case-insensitive FindResource name comparison. Non-ASCII names are rejected.
        bool named_directory(std::span<const std::uint8_t> resources, std::uint32_t offset,
                             std::vector<std::pair<std::string, std::uint32_t>>& entries)
        {
            if (!range(offset, 16, resources.size()))
                return false;
            Reader r(resources.subspan(offset));
            r.skip(12);
            const auto named = r.u16(), ids = r.u16();
            if (std::size_t(named) + ids > 4096)
                return false;
            std::set<std::string> unique;
            for (unsigned i = 0; i < named; ++i)
            {
                const auto name = r.u32(), child = r.u32();
                if (!r.good() || !(name & 0x80000000u))
                    return false;
                const auto at = name & 0x7fffffffu;
                if (!range(at, 2, resources.size()))
                    return false;
                Reader text(resources.subspan(at));
                const auto length = text.u16();
                if (!length || length > 256)
                    return false;
                std::string value;
                for (unsigned c = 0; c < length; ++c)
                {
                    const auto unit = text.u16();
                    if (!unit || unit > 0x7f)
                        return false;
                    value += static_cast<char>(unit >= 'a' && unit <= 'z' ? unit - 32 : unit);
                }
                if (!text.good() || !unique.insert(value).second)
                    return false;
                entries.emplace_back(std::move(value), child);
            }
            return r.good();
        }
        std::string upper(std::string_view text)
        {
            std::string value(text);
            for (auto& c : value)
                if (c >= 'a' && c <= 'z')
                    c = static_cast<char>(c - 32);
            return value;
        }
        bool named_resource(std::span<const std::uint8_t> bytes, std::string_view type, std::string_view name,
                            std::vector<std::uint8_t>& out)
        {
            Image pe;
            if (type.empty() || name.empty() || !image(bytes, pe))
                return false;
            const auto resources = pe.resources;
            std::vector<std::pair<std::string, std::uint32_t>> types, names;
            if (!named_directory(resources, 0, types))
                return false;
            const auto wanted_type = upper(type), wanted_name = upper(name);
            const auto type_entry =
                std::find_if(types.begin(), types.end(), [&](const auto& entry) { return entry.first == wanted_type; });
            if (type_entry == types.end() || !(type_entry->second & 0x80000000u))
                return false;
            const auto names_offset = type_entry->second & 0x7fffffffu;
            if (!names_offset || !named_directory(resources, names_offset, names))
                return false;
            const auto name_entry =
                std::find_if(names.begin(), names.end(), [&](const auto& entry) { return entry.first == wanted_name; });
            if (name_entry == names.end() || !(name_entry->second & 0x80000000u))
                return false;
            const auto langs_offset = name_entry->second & 0x7fffffffu;
            std::vector<Entry> langs;
            if (langs_offset == names_offset || !langs_offset || !directory(resources, langs_offset, langs) ||
                langs.size() != 1 || (langs[0].offset & 0x80000000u) || !range(langs[0].offset, 16, resources.size()))
                return false; // One resource language only; FindResource's locale choice is not reproduced.
            Reader data(resources.subspan(langs[0].offset));
            const auto rva = data.u32(), size = data.u32();
            std::uint32_t offset = 0;
            if (!data.good() || size > 1024 * 1024 || !pe.map(rva, size, offset))
                return false;
            const auto payload = bytes.subspan(offset, size);
            out.assign(payload.begin(), payload.end());
            return true;
        }
        bool decode(std::span<const std::uint8_t> bytes, StringResources& out, std::uint16_t language)
        {
            Image pe;
            if (!image(bytes, pe))
                return false;
            const auto resources = pe.resources;
            auto map = [&](std::uint32_t rva, std::uint32_t size, std::uint32_t& offset)
            { return pe.map(rva, size, offset); };
            std::vector<Entry> types;
            if (!directory(resources, 0, types))
                return false;
            std::vector<Block> blocks;
            std::set<std::uint16_t> languages;
            std::set<std::uint32_t> visited{0};
            for (const auto& type : types)
            {
                if (type.id != 6)
                    continue; // RT_STRING only.
                if (!(type.offset & 0x80000000u))
                    return false;
                const auto block_offset = type.offset & 0x7fffffffu;
                if (!visited.insert(block_offset).second)
                    return false;
                std::vector<Entry> names;
                if (!directory(resources, block_offset, names))
                    return false;
                for (const auto& name : names)
                {
                    if (!name.id || name.id > 4096 || !(name.offset & 0x80000000u))
                        return false;
                    const auto lang_offset = name.offset & 0x7fffffffu;
                    if (!visited.insert(lang_offset).second)
                        return false;
                    std::vector<Entry> langs;
                    if (!directory(resources, lang_offset, langs))
                        return false;
                    for (const auto& lang : langs)
                    {
                        if ((lang.offset & 0x80000000u) || !range(lang.offset, 16, resources.size()) ||
                            blocks.size() >= 4096)
                            return false;
                        Reader data(resources.subspan(lang.offset));
                        const auto rva = data.u32(), size = data.u32();
                        std::uint32_t offset = 0;
                        if (size > 16 * (2 + 4096 * 2) || !map(rva, size, offset))
                            return false;
                        blocks.push_back({name.id, offset, size, static_cast<std::uint16_t>(lang.id)});
                        languages.insert(static_cast<std::uint16_t>(lang.id));
                    }
                }
            }
            if (languages.empty() || (!language && languages.size() != 1))
                return false;
            out.language = language ? language : *languages.begin();
            if (!languages.contains(out.language))
                return false;
            std::size_t total = 0;
            for (const auto& block : blocks)
            {
                if (block.language != out.language)
                    continue;
                Reader r(bytes.subspan(block.offset, block.size));
                for (unsigned i = 0; i < 16; ++i)
                {
                    std::string text;
                    const auto length = r.u16();
                    if (!utf8(r, length, text) || !r.good())
                        return false;
                    total += text.size();
                    if (total > 8 * 1024 * 1024)
                        return false;
                    if (length)
                        out.strings.emplace((block.id - 1) * 16 + i, std::move(text));
                }
                if (r.remaining())
                    return false;
            }
            return !out.strings.empty();
        }
    } // namespace
    bool read_string_resources(std::span<const std::uint8_t> bytes, StringResources& out, std::string& error,
                               std::uint16_t language)
    {
        out = {};
        error.clear();
        StringResources next;
        if (!decode(bytes, next, language))
        {
            error = "PE strings: malformed/unsupported image, language ambiguity or reader budget exceeded";
            return false;
        }
        out = std::move(next);
        return true;
    }
    bool read_named_resource(std::span<const std::uint8_t> bytes, std::string_view type, std::string_view name,
                             std::vector<std::uint8_t>& out, std::string& error)
    {
        out.clear();
        error.clear();
        std::vector<std::uint8_t> next;
        if (!named_resource(bytes, type, name, next))
        {
            error = "PE resource " + std::string(type) + "/" + std::string(name) +
                    ": missing, malformed, multi-language or over the 1 MiB reader budget";
            return false;
        }
        out = std::move(next);
        return true;
    }
} // namespace OpenBlood2
