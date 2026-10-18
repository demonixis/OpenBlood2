#include "engine/ima_cursor.h"
#include "engine/ima_curve_event.h"
#include <algorithm>
#include <bit>
#include <limits>

namespace OpenBlood2
{
    namespace
    {
        std::int16_t grid_count(const ImaPattern& p)
        {
            return std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(p.length / p.timing[0]));
        }
        bool chord_contains(std::uint8_t key, const ImaHarmonicContext& chord)
        {
            // IMUSIC25 145ab: unsigned word comparison, then low-word subtraction and x86 shift masking.
            // Do not normalize signed roots modulo 12: malformed/negative roots have distinct source results.
            auto degree = std::uint16_t(key % 12);
            const auto root = static_cast<std::uint16_t>(chord.chord_root);
            if (root > degree)
                degree += 12;
            degree = static_cast<std::uint16_t>(degree - root);
            auto mask = chord.chord_mask | (chord.chord_mask << 12);
            mask |= mask << 12;
            return (mask & (std::uint32_t(1) << (degree & 31))) != 0;
        }
        bool prepare_dispatch(const ImaStyle& style, const ImaCurves& curves, const ImaCursorStep& step,
                              const ImaCursorState& state, ImaCursorNotes& notes, std::string& error)
        {
            const auto fail = [&]()
            {
                error = "IMA cursor dispatch: mismatched source ranges, record order or event budget";
                return false;
            };
            if (style.source.chunks.size() > 8192 || curves.curves.size() > 65536)
                return fail();
            std::size_t records = curves.curves.size();
            for (std::size_t i = 0; i < curves.curves.size(); ++i)
            {
                const auto& c = curves.curves[i];
                if (c.points.size() > 65536 - records)
                    return fail();
                records += c.points.size();
                if ((!i || curves.curves[i - 1].chunk_offset != c.chunk_offset)
                        ? c.ordinal != 0
                        : c.ordinal != curves.curves[i - 1].ordinal + 1)
                    return fail();
                if (i && c.chunk_offset < curves.curves[i - 1].chunk_offset)
                    return fail();
            }
            const auto append = [&](std::span<const ImaTimedMidiMessage> messages)
            {
                if (messages.size() > ImaSectionCursor::dispatch_message_limit - notes.dispatch.size())
                    return fail();
                notes.dispatch.insert(notes.dispatch.end(), messages.begin(), messages.end());
                return true;
            };
            if (step.grid)
            {
                const auto& pattern = style.patterns[*step.pattern.pattern];
                const auto& grid = pattern.grids[*step.grid];
                bool found = false;
                std::uint32_t pattern_offset = 0;
                std::size_t note_row = 0, prepared_row = 0, curve_chunks = 0;
                for (const auto& chunk : style.source.chunks)
                {
                    if (found && chunk.depth <= 2)
                        break;
                    if (chunk.depth == 1)
                        pattern_offset = chunk.id == "RIFF" && chunk.form == "AAPT" ? chunk.offset : 0;
                    if (!found)
                    {
                        if (chunk.offset == grid.offset && chunk.depth == 2 && chunk.id == "LIST" &&
                            chunk.form == "AACL" && pattern_offset == pattern.offset)
                            found = true;
                        continue;
                    }
                    if (chunk.depth != 3)
                        continue;
                    if (chunk.id == "note")
                    {
                        if (chunk.size < 2 || (chunk.size - 2) % 18)
                            return fail();
                        const auto count = (chunk.size - 2) / 18;
                        if (count > grid.notes.size() - note_row)
                            return fail();
                        for (unsigned i = 0; i < count; ++i, ++note_row)
                            if (prepared_row < notes.rows.size() && notes.rows[prepared_row].row == note_row)
                            {
                                const auto& row = notes.rows[prepared_row++];
                                if (row.events.emitted && !append(std::span(row.events.messages).first(1)))
                                    return false;
                            }
                    }
                    else if (chunk.id == "curv")
                    {
                        ++curve_chunks;
                        std::uint64_t bytes = 4;
                        auto it = std::lower_bound(curves.curves.begin(), curves.curves.end(), chunk.offset,
                                                   [](const auto& c, auto offset) { return c.chunk_offset < offset; });
                        for (; it != curves.curves.end() && it->chunk_offset == chunk.offset; ++it)
                        {
                            if (it->pattern_offset != pattern.offset || it->grid_offset != grid.offset ||
                                it->channel >= 16)
                                return fail();
                            bytes += 9 + 10 * it->points.size();
                            if (step.section.mute_mask & (1u << it->channel))
                                ++notes.curves_muted;
                            else if (!(state.channels.variations[it->channel] & it->variations))
                                ++notes.curves_excluded;
                            else
                            {
                                ++notes.curves_admitted;
                                std::vector<ImaTimedMidiMessage> events;
                                if (!prepare_ima_curve_events(*it, notes.grid_time, events, error) || !append(events))
                                    return false;
                                notes.curve_messages += events.size();
                            }
                        }
                        if (bytes != chunk.size)
                            return fail();
                    }
                }
                if (!found || note_row != grid.notes.size() || prepared_row != notes.rows.size() ||
                    curve_chunks != grid.curve_chunks)
                    return fail();
            }
            if (!append(notes.note_offs))
                return false;
            notes.opaque_curve_chunks = 0;
            notes.curves_prepared = true;
            return true;
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
        notes_.reset();
        active_notes_.clear();
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
        notes_.reset();
        active_notes_.clear(); // Admission-only traversal explicitly discards tracked note lifetime.
        return true;
    }
    bool ImaSectionCursor::step_notes(std::span<const std::uint16_t> draws, std::int32_t grid_time, const ImaBand& band,
                                      std::span<const std::uint8_t> percussion_map, std::string& error,
                                      const ImaCurves* curves)
    {
        error.clear();
        if (!percussion_map.empty() && percussion_map.size() != 128)
        {
            error = "IMA cursor notes: require an absent or 128-entry percussion map";
            return false;
        }
        // Advance a candidate without copying potentially large prior note inspection.
        ImaSectionCursor next;
        next.section_ = section_;
        next.style_ = style_;
        next.timeline_ = timeline_;
        next.harmony_ = harmony_;
        next.state_ = state_;
        if (!next.step(draws, error))
            return false;
        const auto& report = *next.last_;
        ImaCursorNotes notes;
        notes.grid_time = grid_time;
        auto used = report.draws_used;
        if (report.grid)
        {
            const auto& pattern = style_->patterns[*report.pattern.pattern];
            const auto& grid = pattern.grids[*report.grid];
            notes.opaque_curve_chunks = grid.curve_chunks;
            auto context = harmony_->chords[report.section.chord].harmony;
            // IMUSIC25 normal modes 2/3 set the local conversion mode to 2 (RVA 16b38).
            context.enabled = true;
            notes.rows.reserve(report.admitted);
            for (std::size_t i = 0; i < grid.notes.size(); ++i)
            {
                const auto& note = grid.notes[i];
                if ((report.section.mute_mask & (1u << note.channel)) ||
                    !(next.state_.channels.variations[note.channel] & note.variations))
                    continue;
                ImaCursorNote row;
                row.row = i;
                if (!resolve_ima_note_pitch(note, pattern, context, row.pitch, error))
                    return false;
                const std::size_t count = row.pitch.pitch
                                              ? unsigned(note.time_range != 0) + unsigned(note.duration_range != 0) +
                                                    unsigned(note.velocity_range != 0)
                                              : 0;
                if (draws.size() - used < count)
                {
                    error = "IMA cursor notes: insufficient explicit note draws";
                    return false;
                }
                if (!prepare_ima_note_events(
                        note, {grid_time, row.pitch.pitch, band.channels[note.channel].transpose, percussion_map},
                        draws.subspan(used, count), row.events, error))
                    return false;
                used += count;
                notes.suppressed += !row.events.emitted;
                notes.rows.push_back(row);
            }
        }
        notes.draws_used = used - report.draws_used;
        const auto introduced = notes.rows.size() - notes.suppressed;
        const std::int64_t horizon = std::int64_t(grid_time) + report.interval;
        if (introduced > active_note_limit - active_notes_.size() || horizon > std::numeric_limits<std::int32_t>::max())
        {
            error = "IMA cursor notes: active-note budget or output horizon exceeded";
            return false;
        }
        // Source note allocation prepends each new off record before inspecting chord changes.
        std::vector<ImaActiveNote> pending;
        pending.reserve(active_notes_.size() + introduced);
        for (auto i = notes.rows.rbegin(); i != notes.rows.rend(); ++i)
            if (i->events.emitted)
                pending.push_back({i->events.messages[1], state_.serial});
        pending.insert(pending.end(), active_notes_.begin(), active_notes_.end());
        const auto& chord = harmony_->chords[report.section.chord].harmony;
        for (auto note : pending)
        {
            if (report.chord_changed && note.serial != state_.serial && !chord_contains(note.off.key, chord))
            {
                // The source assigns, rather than taking min(old deadline, tick+1).
                note.off.time = grid_time + 1;
                ++notes.shortened;
            }
            if (note.off.time <= horizon)
                notes.note_offs.push_back(note.off);
            else
                next.active_notes_.push_back(note);
        }
        // Both partitions are built by head insertion in the original. Pending order reverses every step.
        std::reverse(notes.note_offs.begin(), notes.note_offs.end());
        std::reverse(next.active_notes_.begin(), next.active_notes_.end());
        if (curves && !prepare_dispatch(*style_, *curves, report, next.state_, notes, error))
            return false;
        next.notes_ = std::move(notes);
        *this = std::move(next);
        return true;
    }
    void ImaSectionCursor::stop()
    {
        *this = {};
    }
} // namespace OpenBlood2
