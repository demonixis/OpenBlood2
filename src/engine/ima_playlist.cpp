#include "engine/ima_playlist.h"
#include <algorithm>
#include <limits>

namespace OpenBlood2
{
    bool prepare_ima_playlist(std::span<const ImaSectionProgram> sections, ImaPlaylistProgram& out, std::string& error)
    {
        out = {};
        error.clear();
        if (sections.empty() || sections.size() > 64)
        {
            error = "IMA finite playlist requires 1..64 sections";
            return false;
        }
        ImaPlaylistProgram next;
        std::int64_t offset = 0, earliest = 0, last = 0;
        for (const auto& section : sections)
        {
            if (section.duration_ticks <= 0 || section.preroll_ticks < 0 || section.events.size() < 80 ||
                section.events.size() > DlsSampler::max_pending_midi - next.events.size() ||
                offset + section.duration_ticks > INT32_MAX)
            {
                error = "IMA finite playlist duration/message budget exceeded or invalid section";
                return false;
            }
            next.parts.push_back({std::int32_t(offset), section.duration_ticks, section.events.size(), section.notes});
            auto tempo = section.tempo;
            tempo.tick = std::int32_t(offset);
            next.tempos.push_back(tempo);
            for (auto message : section.events)
            {
                // Undo each section audition's local preroll, including its setup.
                // A single global shift below retains anticipations across boundaries.
                const auto time = offset + message.time - section.preroll_ticks;
                if (time < INT32_MIN || time > INT32_MAX)
                {
                    error = "IMA finite playlist event tick overflow";
                    return false;
                }
                message.time = std::int32_t(time);
                earliest = std::min(earliest, time);
                last = std::max(last, time);
                next.events.push_back(message);
            }
            offset += section.duration_ticks;
        }
        const auto preroll = -earliest;
        last = std::max(last, offset) + preroll;
        if (last > INT32_MAX || preroll > INT32_MAX)
        {
            error = "IMA finite playlist output epoch overflow";
            return false;
        }
        for (auto& message : next.events)
            message.time += std::int32_t(preroll);
        // First tempo extends through the preroll. Later changes occur at their
        // musical section starts; tails/anticipations use that same global map.
        for (std::size_t i = 0; i < next.parts.size(); ++i)
        {
            next.parts[i].start_tick += std::int32_t(preroll);
            if (i)
                next.tempos[i].tick += std::int32_t(preroll);
        }
        next.preroll_ticks = std::int32_t(preroll);
        next.duration_ticks = std::int32_t(offset);
        next.last_tick = std::int32_t(last);
        ImaClock clock;
        std::uint64_t frame;
        if (!clock.prepare(next.tempos, 0, error) || !clock.sample(next.last_tick, frame, error))
            return false;
        out = std::move(next);
        return true;
    }
} // namespace OpenBlood2
