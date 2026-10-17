#include "engine/ima_style.h"
#include "engine/formats/reader.h"
#include <algorithm>

namespace OpenBlood2
{
    namespace
    {
        // The original fixed UTF-16 name stops at its first NUL; bytes after it are padding.
        bool name(Reader& r, std::string& out)
        {
            Reader text(r.take(32));
            for (unsigned i = 0; i < 16; ++i)
            {
                const auto c = text.u16();
                if (!c)
                    return text.good();
                if (c < 32 || c > 126)
                    return false;
                out.push_back(static_cast<char>(c));
            }
            return false;
        }
        std::int16_t signed16(Reader& r)
        {
            return std::bit_cast<std::int16_t>(r.u16());
        }
        struct Decoder
        {
            std::span<const std::uint8_t> bytes;
            ImaStyle& style;
            std::size_t budget;
            const MusicChunk& chunk(std::size_t i) const
            {
                return style.source.chunks[i];
            }
            Reader payload(std::size_t i) const
            {
                const auto& c = chunk(i);
                return Reader(bytes.subspan(c.offset + 8, c.size)); // Validated by read_music_asset.
            }
            std::size_t next(std::size_t i) const
            {
                const auto depth = chunk(i++).depth;
                while (i < style.source.chunks.size() && chunk(i).depth > depth)
                    ++i;
                return i;
            }
            bool notes(Reader& r, ImaGrid& grid)
            {
                if (r.remaining() < 2 || r.u16() != 18 || r.remaining() % 18 ||
                    r.remaining() / 18 > budget - style.notes)
                    return false;
                const auto count = r.remaining() / 18;
                for (std::size_t i = 0; i < count; ++i)
                {
                    ImaNote n;
                    n.type = r.u8();
                    n.channel = r.u8();
                    n.time_offset = signed16(r);
                    n.variations = r.u16();
                    n.pitch_modifier = r.u8();
                    n.byte7 = r.u8();
                    n.midi_note = r.u8();
                    n.velocity = r.u8();
                    n.music_value = r.u16();
                    n.duration = signed16(r);
                    n.time_range = r.u8();
                    n.duration_range = r.u8();
                    n.velocity_range = r.u8();
                    n.play_mode = r.u8();
                    grid.notes.push_back(n);
                }
                style.notes += count;
                return r.good();
            }
            bool grid(std::size_t i, ImaPattern& pattern)
            {
                if (style.grids >= 8192)
                    return false;
                ImaGrid grid;
                grid.offset = chunk(i).offset;
                bool has_index = false;
                for (auto child = i + 1, end = next(i); child < end; child = next(child))
                {
                    const auto& c = chunk(child);
                    auto r = payload(child);
                    if (c.id == "clik")
                    {
                        if (has_index || c.size != 2)
                            return false;
                        has_index = true;
                        grid.index = signed16(r);
                    }
                    else if (c.id == "note")
                    {
                        if (!notes(r, grid))
                            return false;
                    }
                    else if (c.id == "curv")
                        ++grid.curve_chunks; // Range is retained; curve events are not decoded.
                }
                if (!has_index)
                    return false;
                style.curve_chunks += grid.curve_chunks;
                ++style.grids;
                pattern.grids.push_back(std::move(grid));
                return true;
            }
            bool pattern(std::size_t i)
            {
                if (style.patterns.size() >= 256)
                    return false;
                ImaPattern pattern;
                pattern.offset = chunk(i).offset;
                bool header = false;
                for (auto child = i + 1, end = next(i); child < end; child = next(child))
                {
                    const auto& c = chunk(child);
                    auto r = payload(child);
                    if (c.id == "patt")
                    {
                        if (header || c.size != 625)
                            return false;
                        header = true;
                        pattern.word0 = r.u32();
                        pattern.length = std::bit_cast<std::int32_t>(r.u32());
                        pattern.flags = r.u32();
                        for (auto& value : pattern.timing)
                            value = signed16(r);
                        if (!name(r, pattern.name))
                            return false;
                        // The remaining 573 bytes include ranges/variation tables; keep their source range.
                    }
                    else if (c.id == "LIST" && c.form == "AACL" && !grid(child, pattern))
                        return false;
                }
                if (!header)
                    return false;
                style.patterns.push_back(std::move(pattern));
                return true;
            }
            bool run()
            {
                bool header = false;
                for (std::size_t i = 1; i < style.source.chunks.size(); i = next(i))
                {
                    const auto& c = chunk(i);
                    if (c.id == "styl")
                    {
                        auto r = payload(i);
                        if (header || c.size != 94 || !name(r, style.name))
                            return false;
                        header = true;
                    }
                    else if (c.id == "RIFF" && c.form == "AAPT" && !pattern(i))
                        return false;
                }
                return header;
            }
        };
    } // namespace
    bool read_ima_style(std::span<const std::uint8_t> bytes, ImaStyle& out, std::string& error, std::size_t note_budget)
    {
        out = {};
        ImaStyle parsed;
        if (!read_music_asset(bytes, MusicAssetKind::Style, parsed.source, error))
            return false;
        if (!Decoder{bytes, parsed, std::min<std::size_t>(65536, note_budget)}.run())
        {
            error = "IMA style: invalid/unsupported header, name, grid, note stride/range or record budget";
            return false;
        }
        out = std::move(parsed);
        return true;
    }
} // namespace OpenBlood2
