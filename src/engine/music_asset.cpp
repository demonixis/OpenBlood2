#include "engine/music_asset.h"
#include "engine/formats/reader.h"
#include "engine/resources.h"

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
} // namespace OpenBlood2
