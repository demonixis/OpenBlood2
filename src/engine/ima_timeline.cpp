#include "engine/ima_timeline.h"
#include <algorithm>

namespace OpenBlood2
{
    bool prepare_ima_timeline(const ImaSection& section, const ImaStyle& style, ImaTimeline& out, std::string& error,
                              std::size_t record_budget)
    {
        out = {};
        error.clear();
        const int unit = style.beat_unit ? style.beat_unit : 4;
        const int beat = unit > 0 ? 768 / unit : 0;
        const int measure = beat * int(style.beats_per_measure);
        if (section.measures <= 0 || section.ticks_per_measure <= 0 || beat <= 0 || measure <= 0 || measure > 32767)
        {
            error = "IMA timeline: unsafe authored length/divisor or style meter";
            return false;
        }
        const auto budget = std::min<std::size_t>(8192, record_budget);
        const auto duration = std::int32_t(section.measures) * measure;
        const auto mute_count = static_cast<std::size_t>((duration + std::int64_t(beat) - 1) / beat);
        if (section.records() > 4096 || mute_count > budget || section.commands.size() > budget - mute_count)
        {
            error = "IMA timeline: input or expanded record budget exceeded";
            return false;
        }
        std::size_t kept_chords = 0;
        for (const auto& c : section.chords)
            kept_chords += c.beat < measure / beat;
        if (kept_chords > budget - mute_count - section.commands.size())
        {
            error = "IMA timeline: expanded chord budget exceeded";
            return false;
        }
        ImaTimeline next;
        next.ticks_per_beat = static_cast<std::int16_t>(beat);
        next.ticks_per_measure = static_cast<std::int16_t>(measure);
        next.duration = duration;
        next.default_beat_unit = style.beat_unit == 0;
        next.commands.reserve(section.commands.size());
        for (std::size_t i = 0; i < section.commands.size(); ++i)
        {
            std::int16_t authored_measure;
            if (!ima_section_command_measure(section, section.commands[i].time, authored_measure))
            {
                error = "IMA timeline: invalid authored command divisor";
                return false;
            }
            next.commands.push_back({i, std::int32_t(authored_measure) * measure});
        }
        // Original traversal uses authored order, including an initial future record's value.
        // It does not sort input before sampling or interpolate between values.
        std::size_t cursor = 0, source = section.mutes.empty() ? std::numeric_limits<std::size_t>::max() : 0;
        std::uint16_t value = section.mutes.empty() ? 0 : section.mutes.front().value;
        next.mutes.reserve(mute_count);
        for (std::int32_t tick = 0; tick < duration; tick += beat)
        {
            while (cursor < section.mutes.size() && section.mutes[cursor].time <= tick)
            {
                source = cursor;
                value = section.mutes[cursor++].value;
            }
            next.mutes.push_back({tick, value, source}); // word6 is not copied by the original resampler.
        }
        next.chords.reserve(kept_chords);
        for (std::size_t i = 0; i < section.chords.size(); ++i)
        {
            const auto& c = section.chords[i];
            if (c.beat >= measure / beat)
                next.discarded_chords.push_back(i);
            else
                next.chords.push_back({i, std::int32_t(c.measure) * measure + std::int32_t(c.beat) * beat});
        }
        out = std::move(next);
        return true;
    }
} // namespace OpenBlood2
