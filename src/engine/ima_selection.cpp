#include "engine/ima_selection.h"
#include <algorithm>
#include <bit>

namespace OpenBlood2
{
    namespace
    {
        std::int16_t low_word(std::int32_t value)
        {
            return std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(value));
        }
        std::uint32_t command_flags(const ImaSectionCommand& c)
        {
            return std::uint32_t(c.value[0]) | (std::uint32_t(c.value[1]) << 8) | (std::uint32_t(c.value[2]) << 16) |
                   (std::uint32_t(c.value[3]) << 24);
        }
        bool valid_timeline(const ImaSection& s, const ImaTimeline& t)
        {
            if (s.records() > 4096 || t.records() > 8192 || s.measures <= 0 || s.ticks_per_measure <= 0 ||
                t.ticks_per_beat <= 0 || t.ticks_per_measure <= 0 || t.ticks_per_measure % t.ticks_per_beat != 0 ||
                t.duration != std::int32_t(s.measures) * t.ticks_per_measure || t.commands.size() != s.commands.size())
                return false;
            for (std::size_t i = 0; i < t.commands.size(); ++i)
            {
                std::int16_t measure;
                if (!ima_section_command_measure(s, s.commands[i].time, measure) || t.commands[i].source != i ||
                    t.commands[i].time != std::int32_t(measure) * t.ticks_per_measure)
                    return false;
            }
            std::size_t kept = 0;
            for (std::size_t i = 0; i < s.chords.size(); ++i)
            {
                const auto& c = s.chords[i];
                if (c.beat >= t.ticks_per_measure / t.ticks_per_beat)
                    continue;
                if (kept == t.chords.size() || t.chords[kept].source != i ||
                    t.chords[kept].time !=
                        std::int32_t(c.measure) * t.ticks_per_measure + std::int32_t(c.beat) * t.ticks_per_beat)
                    return false;
                ++kept;
            }
            if (kept != t.chords.size())
                return false;
            // Mutes are prepared samples. Check their provenance and ordering before indexing.
            for (std::size_t i = 0; i < t.mutes.size(); ++i)
            {
                const auto& m = t.mutes[i];
                if ((i && m.time < t.mutes[i - 1].time) ||
                    (m.source == std::numeric_limits<std::size_t>::max()
                         ? !s.mutes.empty() || m.value != 0
                         : m.source >= s.mutes.size() || m.value != s.mutes[m.source].value))
                    return false;
            }
            return true;
        }
    } // namespace
    bool select_ima_section(const ImaSection& s, const ImaTimeline& t, std::int32_t tick, ImaSectionSelection& out,
                            std::string& error)
    {
        out = {};
        error.clear();
        if (!valid_timeline(s, t))
        {
            error = "IMA selection: invalid or mismatched section timeline";
            return false;
        }
        if (t.chords.empty())
        {
            error = "IMA selection: empty chord list requires the unavailable global fallback chord";
            return false;
        }
        ImaSectionSelection next;
        next.measure = low_word(tick / t.ticks_per_measure);
        // The first entry is selected even before its time. Stop at the first future successor.
        while (next.chord + 1 < t.chords.size() && t.chords[next.chord + 1].time <= tick)
            ++next.chord;
        if (!t.mutes.empty())
        {
            std::size_t i = 0;
            while (i + 1 < t.mutes.size() && t.mutes[i + 1].time <= tick)
                ++i;
            next.mute = i;
            next.mute_mask = t.mutes[i].value;
        }
        auto& r = next.request;
        r = {low_word(std::int32_t(s.measures) - next.measure), 0x200};
        std::size_t command = 0;
        for (; command < t.commands.size() && t.commands[command].time <= tick; ++command)
        {
            const auto flags = command_flags(s.commands[t.commands[command].source]);
            if (flags & 0x13f80)
                r.flags = flags & 0x13f80;
            if (t.commands[command].time == tick)
            {
                r.flags |= flags & 0x63;
                break; // First equal command, not the last duplicate.
            }
        }
        if (command < t.commands.size())
        {
            if (t.commands[command].time <= tick)
                ++command;
            if (command < t.commands.size())
            {
                const auto boundary = t.commands[command].time / t.ticks_per_measure;
                r.measures = low_word(boundary - next.measure);
            }
        }
        for (std::size_t i = next.chord; i < t.chords.size(); ++i)
        {
            if (t.chords[i].time < tick)
                continue;
            const auto& c = s.chords[t.chords[i].source];
            const auto delta = std::max<std::int16_t>(0, low_word(std::int32_t(c.measure) - next.measure));
            if (delta)
            {
                r.measures = std::min(r.measures, delta);
                if (r.flags & 8)
                {
                    r.measures = 1;
                    break;
                }
            }
            if (!c.beat)
                continue;
            if (delta)
            {
                r.measures = std::min(r.measures, delta);
                r.flags = (r.flags & ~0x1cu) | 4;
                break;
            }
            if (c.beat & 1)
            {
                r.measures = 1;
                r.flags = (r.flags & ~0x1cu) | 0x10;
                break;
            }
            r.flags = (r.flags & ~0x1cu) | 8;
        }
        if (!(r.flags & 0x18))
            r.flags |= 4;
        r.measures = std::min<std::int16_t>(r.measures, 12); // No lower clamp in the original.
        out = next;
        return true;
    }
    bool select_ima_pattern(const ImaStyle& style, ImaPatternRequest request, ImaPatternSelection& out,
                            std::string& error)
    {
        out = {};
        error.clear();
        if (style.patterns.size() > 4096)
        {
            error = "IMA selection: pattern count exceeds bounded profile";
            return false;
        }
        if (style.patterns.empty())
            return true;
        std::optional<std::size_t> last, best;
        std::int16_t length = 0;
        if (request.measures > 0)
            for (std::size_t i = 0; i < style.patterns.size(); ++i)
            {
                const auto& p = style.patterns[i];
                if ((p.flags & request.flags) != request.flags || ((p.flags ^ request.flags) & 0x63))
                    continue;
                last = i;
                // Original scans requested..1 in descending order, retaining the first matching length.
                // Its subordinate flag fallbacks are reached only after this full-match gate, so the
                // last full match always wins over them when no positive length matches.
                if (p.timing[3] > length && p.timing[3] <= request.measures)
                {
                    best = i;
                    length = p.timing[3];
                }
            }
        out.pattern = best ? best : last ? last : std::optional<std::size_t>(0);
        out.match = best   ? ImaPatternMatch::Length
                    : last ? ImaPatternMatch::LastEligible
                           : ImaPatternMatch::HeadFallback;
        return true;
    }
    const char* ima_pattern_match_name(ImaPatternMatch match)
    {
        switch (match)
        {
            case ImaPatternMatch::Length:
                return "flags and descending length";
            case ImaPatternMatch::LastEligible:
                return "last eligible flags (length fallback)";
            case ImaPatternMatch::HeadFallback:
                return "first pattern fallback";
            case ImaPatternMatch::Empty:
                return "no pattern";
        }
        return "unknown";
    }
} // namespace OpenBlood2
