#include "engine/ima_program.h"
#include <algorithm>
#include <limits>

namespace OpenBlood2
{
    bool prepare_ima_program(std::shared_ptr<const ImaSection> section, std::shared_ptr<const ImaStyle> style,
                             std::shared_ptr<const ImaTimeline> timeline, std::shared_ptr<const ImaHarmony> harmony,
                             const ImaBand& band, const ImaCurves& curves, ImaCursorSeed seed,
                             std::uint32_t random_seed, ImaSectionProgram& out, std::string& error,
                             std::size_t work_budget)
    {
        out = {};
        ImaSectionCursor cursor;
        if (!cursor.start(section, style, timeline, harmony, seed, error))
            return false;
        if (section->opaque_note_chunks || section->opaque_curve_chunks)
        {
            error = "IMA section playback refuses unimplemented section-level notes/curves";
            return false;
        }
        ImaSectionProgram next;
        next.tempo = {0, section->tempo, section->tempo_fraction};
        next.duration_ticks = timeline->duration;
        next.random_state = random_seed;
        std::array<ImaMidiSetup, 80> setup;
        if (!ima_band_setup(band, setup, error))
            return false;
        for (const auto& m : setup)
            next.events.push_back({0, m.status, m.data1, m.data2});
        const auto append = [&](const ImaTimedMidiMessage& m)
        {
            if (next.events.size() == DlsSampler::max_pending_midi)
            {
                error = "IMA section playback message budget exceeded";
                return false;
            }
            next.events.push_back(m);
            return true;
        };
        // Explicit local diagnostic randomness. Never consumes original-game/global RNG.
        const auto draw = [](std::uint32_t& state)
        {
            state = state * 214013U + 2531011U;
            return std::uint16_t((state >> 16) & 32767);
        };
        std::array<std::uint16_t, 4096> draws;
        std::size_t max_notes = 0;
        for (const auto& pattern : style->patterns)
            for (const auto& grid : pattern.grids)
                max_notes = std::max(max_notes, grid.notes.size());
        const auto pool_size = std::min(draws.size(), 16 + max_notes * 3);
        // Bound combined traversal work too: individually bounded files must not
        // multiply into billions of visits across silent/muted grids.
        const auto work_per_grid = pool_size + max_notes + style->source.chunks.size() + curves.curves.size() +
                                   timeline->commands.size() + timeline->chords.size() + timeline->mutes.size() +
                                   style->patterns.size() + 16;
        const auto grid_limit =
            std::min<std::size_t>(65536, std::min<std::size_t>(16000000, work_budget) / work_per_grid);
        while (cursor.active())
        {
            if (next.grids == grid_limit)
            {
                error = "IMA section playback grid/work budget exceeded";
                return false;
            }
            auto trial = next.random_state;
            for (auto& value : std::span(draws).first(pool_size))
                value = draw(trial);
            if (!cursor.step_notes(std::span(draws).first(pool_size), cursor.state().tick, band, {}, error, &curves))
                return false;
            const auto& notes = *cursor.last_notes();
            const auto consumed = cursor.last_step()->draws_used + notes.draws_used;
            for (std::size_t i = 0; i < consumed; ++i)
                draw(next.random_state);
            next.draws += consumed;
            ++next.grids;
            next.work_units += work_per_grid;
            next.notes += notes.rows.size() - notes.suppressed;
            next.curve_messages += notes.curve_messages;
            for (const auto& m : notes.dispatch)
                if (!append(m))
                    return false;
        }
        // IMUSIC25 17996..179c2 sends the remaining list in its current order;
        // 4e49 preserves each deadline. End of section does not truncate held notes.
        next.terminal_notes = cursor.active_notes().size();
        for (const auto& note : cursor.active_notes())
            if (!append(note.off))
                return false;
        std::int32_t earliest = 0;
        for (const auto& m : next.events)
            earliest = std::min(earliest, m.time);
        const auto preroll = -std::int64_t(earliest);
        // A positive output epoch preserves anticipations instead of clipping them.
        // Setup stays at output tick zero, before the earliest note/controller.
        std::int64_t last = std::int64_t(next.duration_ticks) + preroll;
        for (std::size_t i = setup.size(); i < next.events.size(); ++i)
        {
            auto& m = next.events[i];
            const auto time = std::int64_t(m.time) + preroll;
            if (time > INT32_MAX)
            {
                error = "IMA section playback epoch exceeds tick range";
                return false;
            }
            m.time = std::int32_t(time);
            last = std::max(last, time);
        }
        if (last > INT32_MAX || preroll > INT32_MAX)
        {
            error = "IMA section playback duration exceeds tick range";
            return false;
        }
        next.preroll_ticks = std::int32_t(preroll);
        next.last_tick = std::int32_t(last);
        ImaClock clock;
        std::uint64_t frame;
        if (!clock.prepare(std::span(&next.tempo, 1), 0, error) || !clock.sample(next.last_tick, frame, error))
            return false;
        out = std::move(next);
        return true;
    }
} // namespace OpenBlood2
