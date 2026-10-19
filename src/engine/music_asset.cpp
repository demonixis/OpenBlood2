#include "engine/music_asset.h"
#include "engine/formats/reader.h"
#include "engine/resources.h"
#include <charconv>

namespace OpenBlood2
{
    namespace
    {
        bool reference(std::span<const std::uint8_t> bytes, std::string& result)
        {
            // The observed section sref is a NUL-terminated UTF-16LE style basename.
            // Admit ASCII filenames only until other encodings have evidence.
            if (bytes.size() < 4 || bytes.size() > 160 || bytes.size() % 2)
                return false;
            Reader r(bytes);
            while (r.remaining() > 2)
            {
                const auto c = r.u16();
                if (c < 32 || c > 126)
                    return false;
                result.push_back(static_cast<char>(c));
            }
            std::string normalized;
            return r.u16() == 0 && resource_path(result, normalized) && normalized.find('/') == normalized.npos;
        }
        bool chunks(std::span<const std::uint8_t> bytes, std::uint32_t base, unsigned depth, MusicAssetInfo& info,
                    bool section)
        {
            if (depth > 16)
                return false;
            Reader r(bytes);
            while (r.remaining())
            {
                if (r.remaining() < 8 || info.chunks.size() >= 8192)
                    return false;
                MusicChunk chunk;
                chunk.offset = base + static_cast<std::uint32_t>(r.position());
                chunk.depth = depth;
                chunk.id = r.text(4, 4);
                chunk.size = r.u32();
                const auto payload = r.take(chunk.size);
                if (!r.good() || !r.skip(chunk.size & 1))
                    return false;
                const bool container = chunk.id == "RIFF" || chunk.id == "LIST";
                if (container)
                {
                    if (payload.size() < 4)
                        return false;
                    chunk.form.assign(payload.begin(), payload.begin() + 4);
                }
                info.chunks.push_back(chunk);
                if (container && !chunks(payload.subspan(4), chunk.offset + 12, depth + 1, info, section))
                    return false;
                if (section && depth == 1 && chunk.id == "sref")
                {
                    if (!info.style_reference.empty() || !reference(payload, info.style_reference))
                        return false;
                }
            }
            return true;
        }
    } // namespace
    bool read_music_asset(std::span<const std::uint8_t> bytes, MusicAssetKind kind, MusicAssetInfo& out,
                          std::string& error)
    {
        out = {};
        error.clear();
        MusicAssetInfo next;
        Reader r(bytes);
        const char* expected = kind == MusicAssetKind::Section ? "AASE"
                               : kind == MusicAssetKind::Style ? "AASY"
                                                               : "DLS ";
        if (bytes.size() < 12 || bytes.size() > max_music_asset_bytes || r.text(4, 4) != "RIFF" ||
            r.u32() != bytes.size() - 8 || r.text(4, 4) != expected ||
            !chunks(bytes, 0, 0, next, kind == MusicAssetKind::Section) ||
            (kind == MusicAssetKind::Section && next.style_reference.empty()))
        {
            error = "Music asset: wrong RIFF form, malformed ranges/style reference or reader budget exceeded";
            return false;
        }
        out = std::move(next);
        return true;
    }
    bool read_music_style_index(std::string_view text, std::map<std::string, std::string>& output, std::string& error)
    {
        output.clear();
        error.clear();
        auto fail = [&]
        {
            error = "Invalid or ambiguous iMusic.ini style index";
            return false;
        };
        if (text.size() > 65536 || text.find('\0') != text.npos)
            return fail();
        auto trim = [](std::string_view v)
        {
            const auto first = v.find_first_not_of(" \t\r");
            return first == v.npos ? std::string_view{} : v.substr(first, v.find_last_not_of(" \t\r") - first + 1);
        };
        bool styles = false, seen_styles = false;
        std::map<std::string, std::string> fields;
        while (!text.empty())
        {
            const auto end = text.find('\n');
            const auto line = trim(text.substr(0, end));
            text = end == text.npos ? std::string_view{} : text.substr(end + 1);
            if (line.size() > 512)
                return fail();
            if (line.empty() || line.front() == ';')
                continue;
            if (line.front() == '[')
            {
                styles = line == "[Styles]";
                if (styles && seen_styles)
                    return fail();
                seen_styles |= styles;
                continue;
            }
            if (!styles)
                continue;
            const auto equal = line.find('=');
            if (equal == line.npos || fields.size() >= 2048 ||
                !fields.emplace(trim(line.substr(0, equal)), trim(line.substr(equal + 1))).second)
                return fail();
        }
        const auto count = fields.find("NbrStyles");
        unsigned n = 0;
        if (!seen_styles || count == fields.end())
            return fail();
        const auto value = std::from_chars(count->second.data(), count->second.data() + count->second.size(), n);
        if (value.ec != std::errc{} || value.ptr != count->second.data() + count->second.size() || n > 256)
            return fail();
        std::map<std::string, std::string> next;
        for (unsigned i = 1; i <= n; ++i)
        {
            const auto name = fields.find("Name" + std::to_string(i)), file = fields.find("File" + std::to_string(i));
            std::string key, path;
            auto basename = [](const std::string& value, std::string& normalized)
            {
                return !value.empty() && value.size() < 80 &&
                       value.find_first_not_of(
                           "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789 _-.()") == value.npos &&
                       resource_path(value, normalized) && normalized.find('/') == normalized.npos;
            };
            if (name == fields.end() || file == fields.end() || !basename(name->second, key) ||
                !basename(file->second, path) || !path.ends_with(".sty") || !next.emplace(key, file->second).second)
                return fail();
        }
        output = std::move(next);
        return true;
    }
} // namespace OpenBlood2
