#pragma once
#include "engine/ima_program.h"

namespace OpenBlood2
{
    struct ImaPlaylistPart
    {
        std::int32_t start_tick = 0, duration = 0;
        std::size_t messages = 0;
        std::uint64_t notes = 0;
    };
    struct ImaPlaylistProgram
    {
        std::vector<ImaTimedMidiMessage> events;
        std::vector<ImaTempo> tempos;
        std::vector<ImaPlaylistPart> parts;
        std::int32_t preroll_ticks = 0, duration_ticks = 0, last_tick = 0;
    };
    // One finite pass, no repeat or live switch. All sections share one output
    // epoch/tempo map; their musical durations, not note tails, place successors.
    bool prepare_ima_playlist(std::span<const ImaSectionProgram>, ImaPlaylistProgram&, std::string& error);
} // namespace OpenBlood2
