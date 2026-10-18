#include "engine/ima_harmony.h"
#include <algorithm>
#include <bit>

namespace OpenBlood2
{
    namespace
    {
        std::uint32_t be32(std::span<const std::uint8_t> data, std::size_t pos)
        {
            return (std::uint32_t(data[pos]) << 24) | (std::uint32_t(data[pos + 1]) << 16) |
                   (std::uint32_t(data[pos + 2]) << 8) | data[pos + 3];
        }
        bool label(std::span<const std::uint8_t> bytes, std::string& out)
        {
            for (const auto c : bytes)
            {
                if (!c)
                    return true;
                if (c < 32 || c > 126)
                    return false;
                out += static_cast<char>(c);
            }
            return false;
        }
        std::uint8_t pitch_flags(std::uint32_t mask)
        {
            const auto count = std::popcount(mask);
            // Count is deliberately not masked to four bits: the original ORs the full byte.
            return static_cast<std::uint8_t>(0x10 | (count > 3 ? 0x20 : 0) | (mask & 0x3c0000 ? 0x40 : 0) | count);
        }
        std::uint32_t bit(int index)
        {
            return std::uint32_t(1) << (static_cast<unsigned>(index) & 31);
        }
        std::uint32_t rotated(std::uint32_t mask, int root)
        {
            const int remainder = root % 12; // Signed remainder and x86 masked shifts, not a normalized rotation.
            mask &= 0xffffff;
            return ((mask >> (static_cast<unsigned>(24 - remainder) & 31)) |
                    (mask << (static_cast<unsigned>(remainder) & 31))) &
                   0xffffff;
        }
        bool outside_templates(const ImaSectionChord& chord, const ImaHarmonicContext& base)
        {
            const auto scale = rotated(base.chord_mask, base.chord_root);
            const auto mask = rotated(chord.chord_pattern, chord.root);
            std::uint32_t templates[4]{};
            int degree = 0, index = chord.root % 12;
            for (int i = 0; i < 12; ++i)
            {
                const auto value = bit(index);
                if (scale & value)
                {
                    ++degree;
                    if (degree == 1 || degree == 3 || degree == 5)
                        for (auto& t : templates)
                            t |= value;
                    else if (degree == 6)
                        templates[1] |= value;
                    else if (degree == 7)
                    {
                        templates[2] |= value;
                        templates[3] |= bit(index == 0 ? 23 : index - 1);
                    }
                }
                if (++index >= 24)
                    index = 0;
            }
            return std::none_of(std::begin(templates), std::end(templates), [&](auto t) { return (t & mask) == mask; });
        }
        std::uint16_t classify(const ImaSectionChord& chord, const ImaSectionChord* next,
                               const ImaHarmonicContext& base)
        {
            const auto scale = chord.scale_pattern ? chord.scale_pattern : base.chord_mask;
            std::uint16_t flags = (chord.chord_pattern & 0x91) == 0x91   ? 0x100
                                  : (chord.chord_pattern & 0x89) == 0x89 ? 0x200
                                                                         : 0x400;
            int degree = -1;
            bool off_scale = false;
            for (int i = 0; i <= (int(chord.root) - int(base.chord_root) + 36) % 12; ++i)
            {
                off_scale = !(scale & bit(i));
                if (!off_scale)
                    ++degree;
            }
            flags |= off_scale ? 0x80 : static_cast<std::uint16_t>(bit(std::max(0, degree)));
            if (next && next->beat == 0)
            {
                degree = 0;
                // The original keeps off_scale if a negative remainder skips this loop.
                for (int i = 0; i <= (int(next->root) - int(base.chord_root) + 36) % 12; ++i)
                {
                    off_scale = !(scale & bit(i));
                    if (!off_scale)
                        ++degree;
                }
                if (!off_scale)
                    flags |= degree == 1 ? 0x800 : degree == 5 ? 0x1000 : 0;
            }
            flags |= outside_templates(chord, base) ? 0x4000 : 0x2000;
            return flags;
        }
    } // namespace
    bool read_ima_personality(std::span<const std::uint8_t> data, ImaPersonality& out, std::string& error)
    {
        out = {};
        error.clear();
        const auto fail = [&]
        {
            error = "IMA personality: invalid or unsupported bounded REPs metadata";
            return false;
        };
        if (data.size() < 12 || data.size() > 1024 * 1024 || !std::equal(data.begin(), data.begin() + 4, "REPs") ||
            be32(data, 4) != data.size() - 8)
            return fail();
        ImaPersonality value;
        value.header_bytes = be32(data, 8);
        if (value.header_bytes < 1164 || value.header_bytes > 65536 || value.header_bytes > data.size() - 12)
            return fail();
        const auto h = data.subspan(12, value.header_bytes);
        value.scale_mask = std::uint32_t(h[16]) | (std::uint32_t(h[17]) << 8) | (std::uint32_t(h[18]) << 16) |
                           (std::uint32_t(h[19]) << 24);
        if (!label(h.subspan(20, 20), value.name) || !label(h.subspan(40, 80), value.description) ||
            !label(h.subspan(120, 20), value.default_name))
            return fail();
        // REPs uses big-endian envelope lengths and no RIFF padding. Child payloads remain opaque.
        for (std::size_t pos = 12 + value.header_bytes; pos < data.size();)
        {
            if (data.size() - pos < 8 || value.opaque_chunks.size() == 256)
                return fail();
            const auto size = be32(data, pos + 4);
            if (size > data.size() - pos - 8)
                return fail();
            value.opaque_chunks.push_back({std::string(reinterpret_cast<const char*>(data.data() + pos), 4),
                                           {},
                                           static_cast<std::uint32_t>(pos),
                                           size,
                                           1});
            pos += 8 + size;
        }
        out = std::move(value);
        return true;
    }
    bool prepare_ima_harmony(const ImaSection& section, const ImaTimeline& timeline, const ImaPersonality& personality,
                             ImaHarmony& out, std::string& error)
    {
        out = {};
        error.clear();
        const auto fail = [&]
        {
            error = "IMA harmony: invalid or mismatched retained chord timeline";
            return false;
        };
        if (section.chords.size() > 4096 || timeline.chords.size() > section.chords.size() ||
            timeline.ticks_per_beat <= 0 || timeline.ticks_per_measure <= 0)
            return fail();
        std::size_t retained = 0;
        for (std::size_t i = 0; i < section.chords.size(); ++i)
        {
            const auto& c = section.chords[i];
            if (c.beat >= timeline.ticks_per_measure / timeline.ticks_per_beat)
                continue;
            if (retained == timeline.chords.size() || timeline.chords[retained].source != i ||
                timeline.chords[retained].time != std::int32_t(c.measure) * timeline.ticks_per_measure +
                                                      std::int32_t(c.beat) * timeline.ticks_per_beat)
                return fail();
            ++retained;
        }
        if (retained != timeline.chords.size())
            return fail();
        ImaHarmony value;
        value.base = {false,
                      personality.scale_mask,
                      personality.scale_mask,
                      static_cast<std::int8_t>(section.default_chord & 127),
                      static_cast<std::int8_t>(section.default_chord & 127),
                      pitch_flags(personality.scale_mask)};
        value.base_flag = (section.default_chord & 128) != 0;
        value.chords.reserve(retained);
        for (std::size_t i = 0; i < retained; ++i)
        {
            const auto& t = timeline.chords[i];
            const auto& c = section.chords[t.source];
            const auto* next = i + 1 < retained ? &section.chords[timeline.chords[i + 1].source] : nullptr;
            value.chords.push_back({t.source,
                                    t.time,
                                    {false, c.chord_pattern, c.scale_pattern ? c.scale_pattern : personality.scale_mask,
                                     c.root, value.base.scale_root,
                                     static_cast<std::uint8_t>(pitch_flags(c.chord_pattern) | (c.flags & 8 ? 128 : 0))},
                                    classify(c, next, value.base)});
        }
        out = std::move(value);
        return true;
    }
} // namespace OpenBlood2
