#pragma once
#include "engine/dls_sampler.h"
#include "engine/ima_cursor.h"

namespace OpenBlood2
{
    // Finite, explicit single-section audition. Not playlist selection or a MUSIC service.
    struct ImaSectionProgram
    {
        std::vector<ImaTimedMidiMessage> events; // Submission order, including setup and terminal tails.
        std::uint64_t grids = 0, draws = 0, notes = 0, curve_messages = 0, terminal_notes = 0;
        std::uint32_t random_state = 0;
        std::size_t work_units = 0;
        std::int32_t preroll_ticks = 0, duration_ticks = 0, last_tick = 0;
        ImaTempo tempo;
    };
    // Reader-produced exact bindings. Failure clears output; no sampler/resource mutation.
    bool prepare_ima_program(std::shared_ptr<const ImaSection>, std::shared_ptr<const ImaStyle>,
                             std::shared_ptr<const ImaTimeline>, std::shared_ptr<const ImaHarmony>, const ImaBand&,
                             const ImaCurves&, ImaCursorSeed, std::uint32_t random_seed, ImaSectionProgram&,
                             std::string& error, std::size_t work_budget = 16000000);
} // namespace OpenBlood2
