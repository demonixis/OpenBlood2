#include "engine/ima_cursor.h"
#include <bit>

namespace OpenBlood2
{
    namespace
    {
        std::int16_t grid_count(const ImaPattern& p)
        {
            return std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(p.length / p.timing[0]));
        }
    } // namespace
    bool ImaSectionCursor::start(std::shared_ptr<const ImaSection> section, std::shared_ptr<const ImaStyle> style,
                                 std::shared_ptr<const ImaTimeline> timeline, std::shared_ptr<const ImaHarmony> harmony,
                                 ImaCursorSeed seed, std::string& error)
    {
        error.clear();
        if (!section || !style || !timeline || !harmony)
        {
            error = "IMA cursor: missing immutable binding";
            return false;
        }
        ImaSectionSelection selected;
        if (!select_ima_section(*section, *timeline, 0, selected, error))
            return false;
        if (harmony->chords.size() != timeline->chords.size() || style->patterns.empty() ||
            style->patterns.size() > 4096)
        {
            error = "IMA cursor: missing/mismatched harmony or patterns";
            return false;
        }
        for (std::size_t i = 0; i < harmony->chords.size(); ++i)
            if (harmony->chords[i].source != timeline->chords[i].source ||
                harmony->chords[i].time != timeline->chords[i].time)
            {
                error = "IMA cursor: mismatched retained chord";
                return false;
            }
        std::size_t grids = 0, notes = 0;
        for (const auto& p : style->patterns)
        {
            if (p.timing[0] <= 0 || p.length <= 0 || grid_count(p) <= 0 || p.grids.size() > 65536 - grids)
            {
                error = "IMA cursor: nonprogressing pattern timing or grid budget exceeded";
                return false;
            }
            grids += p.grids.size();
            for (const auto& g : p.grids)
            {
                if (g.notes.size() > 262144 - notes)
                {
                    error = "IMA cursor: note budget exceeded";
                    return false;
                }
                notes += g.notes.size();
                for (const auto& n : g.notes)
                    if (n.channel >= 16)
                    {
                        error = "IMA cursor: invalid source channel";
                        return false;
                    }
            }
        }
        section_ = std::move(section);
        style_ = std::move(style);
        timeline_ = std::move(timeline);
        harmony_ = std::move(harmony);
        state_ = {};
        state_.channels = seed;
        last_.reset();
        return true;
    }
    bool ImaSectionCursor::step(std::span<const std::uint16_t> draws, std::string& error)
    {
        error.clear();
        if (!active())
        {
            error = "IMA cursor: stopped or single traversal finished";
            return false;
        }
        auto next = state_;
        ImaCursorStep report;
        report.tick = next.tick;
        // The original captures the ordinal BEFORE a command resets the pattern cursor.
        report.grid_ordinal = next.grid;
        if (!select_ima_section(*section_, *timeline_, next.tick, report.section, error))
            return false;
        const auto& chord = harmony_->chords[report.section.chord];
        report.chord_changed = chord.harmony.chord_mask != next.channels.previous_chord ||
                               chord.harmony.chord_root != next.channels.previous_root;
        for (const auto& command : timeline_->commands)
        {
            if (command.time > next.tick)
                break;
            if (command.time == next.tick)
            {
                report.command_restart = true;
                next.grid = 0;
                break;
            }
        }
        if (next.grid == 0)
        {
            if (!select_ima_pattern(*style_, report.section.request, report.pattern, error))
                return false;
            next.pattern = report.pattern.pattern;
            report.pattern_selected = true;
        }
        else
            report.pattern.pattern = next.pattern;
        // start preflights nonempty immutable styles; every pattern choice has a head fallback.
        const auto& pattern = style_->patterns[*next.pattern];
        report.interval = pattern.timing[0];
        report.pattern_grids = grid_count(pattern);
        if (static_cast<std::size_t>(report.grid_ordinal) < pattern.grids.size())
            report.grid = static_cast<std::size_t>(report.grid_ordinal);
        if (next.grid == 0 || report.chord_changed)
        {
            report.variations_changed = true;
            const auto count = static_cast<std::size_t>(std::popcount(next.channels.random_channels));
            if (draws.size() < count)
            {
                error = "IMA cursor: insufficient explicit variation draws";
                return false;
            }
            ImaVariationChoice choice;
            if (!choose_ima_variations(prepare_ima_pattern_channels(pattern), chord.variation_flags,
                                       next.channels.variations, next.channels.random_channels,
                                       next.channels.cycle_channels, draws.first(count), choice, error))
                return false;
            next.channels.variations = choice.selected;
            report.draws_used = choice.draws_used;
        }
        if (report.grid)
            for (const auto& note : pattern.grids[*report.grid].notes)
            {
                if (report.section.mute_mask & (1u << note.channel))
                    ++report.muted;
                else if (!(next.channels.variations[note.channel] & note.variations))
                    ++report.excluded_variation;
                else
                    ++report.admitted;
            }
        next.channels.previous_chord = chord.harmony.chord_mask;
        next.channels.previous_root = chord.harmony.chord_root;
        // Maximum admitted duration is 32767*32767; adding one positive int16 interval is safe.
        next.tick += report.interval;
        ++next.grid; // Positive grid_count <=32767; wrap below prevents signed overflow.
        ++next.serial;
        ++next.steps;
        if (next.tick >= timeline_->duration)
        {
            next.tick = 0;
            next.grid = 0;
            next.serial = 0;
            next.ended = true; // Stop before the original repeat/playlist branch, explicitly.
        }
        else if (next.grid >= report.pattern_grids)
            next.grid = 0;
        state_ = next;
        last_ = report;
        return true;
    }
    void ImaSectionCursor::stop()
    {
        *this = {};
    }
} // namespace OpenBlood2
