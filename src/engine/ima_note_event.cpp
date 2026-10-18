#include "engine/ima_note_event.h"
#include <algorithm>
#include <bit>
#include <limits>

namespace OpenBlood2
{
    bool prepare_ima_note_events(const ImaNote& note, const ImaNoteEventContext& context,
                                 std::span<const std::uint16_t> draws, ImaNoteEvents& out, std::string& error)
    {
        out = {};
        error.clear();
        if ((note.type != 8 && note.type != 9) || note.channel >= 16 ||
            (!context.percussion_map.empty() && context.percussion_map.size() != 128))
        {
            error = "IMA note event: unsupported type/channel or percussion map size";
            return false;
        }
        const std::size_t required = context.resolved_pitch
                                         ? unsigned(note.time_range != 0) + unsigned(note.duration_range != 0) +
                                               unsigned(note.velocity_range != 0)
                                         : 0;
        if (draws.size() != required ||
            std::any_of(draws.begin(), draws.end(), [](auto value) { return value > 32767; }))
        {
            error = "IMA note event: require one 15-bit draw per active range, none for zero pitch";
            return false;
        }
        if (!context.resolved_pitch)
            return true;
        int pitch = context.resolved_pitch;
        if (note.channel == 5 && !context.percussion_map.empty())
        {
            if (pitch < 0 || pitch >= 128)
            {
                error = "IMA note event: resolved pitch outside explicit percussion map";
                return false;
            }
            pitch = context.percussion_map[pitch];
        }
        pitch = std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(pitch + context.transpose));
        if (pitch < 1)
            pitch += ((12 - pitch) / 12) * 12;
        if (pitch > 127)
            pitch -= ((pitch - 116) / 12) * 12;

        std::size_t cursor = 0;
        const auto jitter = [&](std::uint8_t raw) -> int
        {
            if (!raw)
                return 0;
            const int range = std::bit_cast<std::int8_t>(raw);
            // Original idiv remainder has the nonnegative numerator's sign; sar halves downward.
            const int half = range < 0 ? -((-range + 1) / 2) : range / 2;
            return draws[cursor++] % (range < 0 ? -range : range) - half;
        };
        const std::int64_t start = std::int64_t(context.grid_time) + note.time_offset + jitter(note.time_range);
        const int duration = std::max(1, int(note.duration) + jitter(note.duration_range));
        const int velocity =
            std::clamp(int(std::bit_cast<std::int8_t>(note.velocity)) + jitter(note.velocity_range), 1, 127);
        const std::int64_t end = start + duration;
        if (start < std::numeric_limits<std::int32_t>::min() || start > std::numeric_limits<std::int32_t>::max() ||
            end < std::numeric_limits<std::int32_t>::min() || end > std::numeric_limits<std::int32_t>::max())
        {
            error = "IMA note event: start/end time exceeds signed 32-bit tick range";
            return false;
        }
        const auto channel = (note.channel + 4u) & 15u;
        out.emitted = true;
        out.draws_used = static_cast<std::uint8_t>(cursor);
        out.messages = {{{static_cast<std::int32_t>(start), static_cast<std::uint8_t>(0x90u | channel),
                          static_cast<std::uint8_t>(pitch), static_cast<std::uint8_t>(velocity)},
                         {static_cast<std::int32_t>(end), static_cast<std::uint8_t>(0x80u | channel),
                          static_cast<std::uint8_t>(pitch), static_cast<std::uint8_t>(velocity)}}};
        return true;
    }
} // namespace OpenBlood2
