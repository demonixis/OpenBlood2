#include "engine/ima_section.h"
#include "engine/formats/reader.h"
#include <algorithm>

namespace OpenBlood2
{
    namespace
    {
        std::int16_t signed16(Reader& r)
        {
            return std::bit_cast<std::int16_t>(r.u16());
        }
        std::int32_t signed32(Reader& r)
        {
            return std::bit_cast<std::int32_t>(r.u32());
        }
        template <std::size_t N> void bytes(Reader& r, std::array<std::uint8_t, N>& values)
        {
            for (auto& v : values)
                v = r.u8();
        }
        bool label(std::span<const std::uint8_t> data, std::string& out)
        {
            if (data.empty() || data.size() % 2)
                return false;
            Reader r(data);
            while (r.remaining())
            {
                const auto c = r.u16();
                if (!c)
                    return true; // Original conversion ignores padding after the terminator.
                if (c < 32 || c > 126)
                    return false;
                out.push_back(static_cast<char>(c));
            }
            return false;
        }
    } // namespace
    bool read_ima_section(std::span<const std::uint8_t> data, ImaSection& out, std::string& error,
                          std::size_t record_budget)
    {
        out = {};
        error.clear();
        ImaSection next;
        if (!read_music_asset(data, MusicAssetKind::Section, next.source, error))
            return false;
        bool header = false, personality = false, commands = false, mutes = false, chords = false;
        bool valid = true;
        const auto budget = std::min<std::size_t>(4096, record_budget);
        for (const auto& c : next.source.chunks)
        {
            if (!valid)
                break;
            if (c.depth != 1)
                continue;
            const auto payload = data.subspan(c.offset + 8, c.size);
            Reader r(payload);
            if (c.id == "secn")
            {
                if (header || c.size != 120 || !label(payload.subspan(4, 32), next.name))
                {
                    valid = false;
                    break;
                }
                header = true;
                next.word0 = r.u32();
                r.skip(32);
                next.tempo = signed16(r);
                next.word38 = signed16(r);
                next.measures = signed16(r);
                next.ticks_per_measure = signed16(r);
                next.ticks_per_beat = signed16(r);
                next.tempo_fraction = r.u16();
                next.word48 = r.u32();
                next.default_chord = r.u8();
                bytes(r, next.padding53);
                bytes(r, next.style_id);
                bytes(r, next.personality_id);
                bytes(r, next.tail);
            }
            else if (c.id == "prnm")
            {
                if (personality || c.size > 80 || !label(payload, next.personality))
                {
                    valid = false;
                    break;
                }
                personality = true;
            }
            else if (c.id == "cmnd" || c.id == "mute" || c.id == "chrd")
            {
                bool& seen = c.id == "cmnd" ? commands : c.id == "mute" ? mutes : chords;
                const unsigned stride = c.id == "chrd" ? 117 : 8;
                if (!header || seen || c.size < 2 || r.u16() != stride || (c.size - 2) % stride ||
                    (c.size - 2) / stride > budget - next.records())
                {
                    valid = false;
                    break;
                }
                seen = true;
                while (r.remaining() && valid)
                {
                    if (c.id == "cmnd")
                    {
                        ImaSectionCommand v;
                        v.time = signed32(r);
                        bytes(r, v.value);
                        next.commands.push_back(v);
                    }
                    else if (c.id == "mute")
                    {
                        ImaSectionMute v;
                        v.time = signed32(r);
                        v.value = r.u16();
                        v.word6 = r.u16();
                        next.mutes.push_back(v);
                    }
                    else
                    {
                        ImaSectionChord v;
                        valid = label(payload.subspan(c.size - r.remaining(), 32), v.name);
                        r.skip(32);
                        v.flags = r.u8();
                        v.beat = std::bit_cast<std::int8_t>(r.u8());
                        v.measure = signed16(r);
                        v.chord_pattern = r.u32();
                        v.scale_pattern = r.u32();
                        v.word44 = r.u32();
                        v.root = std::bit_cast<std::int8_t>(r.u8());
                        bytes(r, v.tail);
                        next.chords.push_back(std::move(v));
                    }
                }
            }
            else if (c.id == "note")
                ++next.opaque_note_chunks;
            else if (c.id == "curv")
                ++next.opaque_curve_chunks;
        }
        if (!valid || !header)
        {
            error = "IMA section: missing/duplicate header, unsupported size/stride/label or record budget exceeded";
            return false;
        }
        out = std::move(next);
        return true;
    }
    bool ima_section_timing(const ImaSection& section, ImaSectionTiming& out, std::string& error)
    {
        out = {};
        error.clear();
        if (section.tempo <= 0 || section.measures <= 0 || section.ticks_per_measure <= 0 ||
            section.ticks_per_beat <= 0)
        {
            error = "IMA section: nonpositive authored tempo, length or meter";
            return false;
        }
        out.duration_ticks = std::int32_t(section.measures) * section.ticks_per_measure;
        const auto tempo64 = (std::uint32_t(section.tempo) << 6) | (section.tempo_fraction >> 10);
        out.microseconds_per_quarter = 3840000000u / tempo64;
        return true;
    }
    bool ima_section_command_measure(const ImaSection& section, std::int32_t time, std::int16_t& measure)
    {
        measure = 0;
        if (section.ticks_per_measure <= 0)
            return false;
        // The original signed division stores only AX. Preserve truncation and 16-bit wrap explicitly.
        measure = std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(time / section.ticks_per_measure));
        return true;
    }
} // namespace OpenBlood2
