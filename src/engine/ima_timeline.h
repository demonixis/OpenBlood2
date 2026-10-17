#pragma once
#include "engine/ima_section.h"
#include "engine/ima_style.h"
#include <limits>

namespace OpenBlood2
{
    struct ImaTimedRecord
    {
        std::size_t source = 0;
        std::int32_t time = 0;
    };
    struct ImaTimedMute
    {
        std::int32_t time = 0;
        std::uint16_t value = 0;
        std::size_t source = std::numeric_limits<std::size_t>::max(); // No authored mute.
    };
    struct ImaTimeline
    {
        std::int16_t ticks_per_beat = 0, ticks_per_measure = 0;
        std::int32_t duration = 0;
        bool default_beat_unit = false;
        std::vector<ImaTimedRecord> commands, chords;
        std::vector<ImaTimedMute> mutes;
        std::vector<std::size_t> discarded_chords;
        std::size_t records() const
        {
            return commands.size() + chords.size() + mutes.size();
        }
    };
    // Original style-meter reconciliation only, not pattern selection, chord harmonization or playback.
    // Input remains immutable. Outputs own times and indices into the supplied section's raw records.
    // Refuses unsafe/nonpositive/overflowed meters and bounded expansion; failure clears output.
    bool prepare_ima_timeline(const ImaSection&, const ImaStyle&, ImaTimeline&, std::string& error,
                              std::size_t record_budget = 8192);
} // namespace OpenBlood2
