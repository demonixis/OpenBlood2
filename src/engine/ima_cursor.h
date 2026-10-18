#pragma once
#include "engine/ima_band.h"
#include "engine/ima_curve.h"
#include "engine/ima_harmony.h"
#include "engine/ima_note_event.h"
#include "engine/ima_selection.h"
#include "engine/ima_variation.h"
#include <memory>

namespace OpenBlood2
{
    struct ImaCursorSeed
    {
        std::array<std::uint16_t, 16> variations{};
        std::uint16_t random_channels = 0, cycle_channels = 0;
        std::uint32_t previous_chord = 0;
        std::int8_t previous_root = 0;
        bool operator==(const ImaCursorSeed&) const = default;
    };
    struct ImaCursorState
    {
        std::int32_t tick = 0;
        std::int16_t grid = 0;
        std::uint16_t serial = 0;
        std::optional<std::size_t> pattern;
        ImaCursorSeed channels;
        std::uint64_t steps = 0;
        bool ended = false;
        bool operator==(const ImaCursorState&) const = default;
    };
    struct ImaCursorStep
    {
        std::int32_t tick = 0;
        std::int16_t grid_ordinal = 0, interval = 0, pattern_grids = 0;
        std::optional<std::size_t> grid;
        ImaSectionSelection section;
        ImaPatternSelection pattern;
        bool command_restart = false, pattern_selected = false, chord_changed = false, variations_changed = false;
        std::size_t draws_used = 0, admitted = 0, muted = 0, excluded_variation = 0;
    };
    struct ImaCursorNote
    {
        std::size_t row = 0; // Index in the selected grid, including filtered rows.
        ImaNotePitch pitch;
        ImaNoteEvents events;
    };
    struct ImaCursorNotes
    {
        std::int32_t grid_time = 0; // Caller-supplied output clock, independent of section tick.
        std::size_t draws_used = 0, suppressed = 0, opaque_curve_chunks = 0;
        std::vector<ImaCursorNote> rows; // Admitted rows in source order; paired events are not sorted.
        std::size_t shortened = 0;       // Chord deadline assignments, including extension under explicit clock jumps.
        bool curves_prepared = false;
        std::size_t curves_admitted = 0, curves_muted = 0, curves_excluded = 0, curve_messages = 0;
        std::vector<ImaTimedMidiMessage> dispatch;  // Merged source-order on/curve messages, then due offs.
        std::vector<ImaTimedMidiMessage> note_offs; // Due through grid_time + interval, original dispatch order.
    };
    struct ImaActiveNote
    {
        ImaTimedMidiMessage off;
        std::uint16_t serial = 0; // Original low-word creation grid, including its wrap behavior.
        bool operator==(const ImaActiveNote&) const = default;
    };
    // Explicit single-section traversal of original normal-mode grid rules, NOT an audio player.
    // Immutable bindings are retained until stop/replacement. No callbacks, I/O or implicit RNG.
    class ImaSectionCursor
    {
      public:
        bool start(std::shared_ptr<const ImaSection>, std::shared_ptr<const ImaStyle>,
                   std::shared_ptr<const ImaTimeline>, std::shared_ptr<const ImaHarmony>, ImaCursorSeed,
                   std::string& error);
        // Consume only the required prefix of the caller's draw pool. Failure leaves the cursor unchanged.
        bool step(std::span<const std::uint16_t> draws, std::string& error);
        // Normal-mode harmonic activation, selected-band transpose, then note event preparation.
        // One draw prefix: variations first, then admitted nonzero notes in source order.
        // Band/map and optional exact-style curves are borrowed for this call. Curves
        // enable source-chunk ordered dispatch preparation, never actual MIDI delivery.
        // Failure preserves state AND prior inspection.
        bool step_notes(std::span<const std::uint16_t> draws, std::int32_t grid_time, const ImaBand&,
                        std::span<const std::uint8_t> percussion_map, std::string& error,
                        const ImaCurves* curves = nullptr);
        void stop();
        bool active() const
        {
            return bool(section_) && !state_.ended;
        }
        const ImaCursorState& state() const
        {
            return state_;
        }
        const std::optional<ImaCursorStep>& last_step() const
        {
            return last_;
        }
        const std::optional<ImaCursorNotes>& last_notes() const
        {
            return notes_;
        }
        const std::vector<ImaActiveNote>& active_notes() const
        {
            return active_notes_;
        }
        static constexpr std::size_t active_note_limit = 65536;
        static constexpr std::size_t dispatch_message_limit = 262144;

      private:
        std::shared_ptr<const ImaSection> section_;
        std::shared_ptr<const ImaStyle> style_;
        std::shared_ptr<const ImaTimeline> timeline_;
        std::shared_ptr<const ImaHarmony> harmony_;
        ImaCursorState state_;
        std::optional<ImaCursorStep> last_;
        std::optional<ImaCursorNotes> notes_;
        std::vector<ImaActiveNote> active_notes_;
    };
} // namespace OpenBlood2
