#pragma once
#include "engine/ima_style.h"

namespace OpenBlood2
{
    struct ImaNoteEventContext
    {
        std::int32_t grid_time = 0;
        std::int16_t resolved_pitch = 0; // After pitch/mode/range processing, which this helper does not perform.
        std::int8_t transpose = 0;       // Explicit effective-band transpose, not a band-selection policy.
        std::span<const std::uint8_t> percussion_map; // Empty or 128 entries; borrowed only during this call.
    };
    struct ImaTimedMidiMessage
    {
        std::int32_t time = 0;
        std::uint8_t status = 0, key = 0, velocity = 0;
        bool operator==(const ImaTimedMidiMessage&) const = default;
    };
    struct ImaNoteEvents
    {
        bool emitted = false; // Zero resolved pitch is a successful no-event result, before RNG/transpose.
        std::uint8_t draws_used = 0;
        std::array<ImaTimedMidiMessage, 2> messages{}; // Note on, then note off (same velocity).
    };
    // Original post-harmonization processing. Exactly one 15-bit draw per nonzero
    // time/duration/velocity range, in that order; zero pitch requires no draws.
    // No scheduling, synth call, RNG consumption or retained context. Failures clear output.
    bool prepare_ima_note_events(const ImaNote&, const ImaNoteEventContext&, std::span<const std::uint16_t> draws,
                                 ImaNoteEvents&, std::string& error);
} // namespace OpenBlood2
