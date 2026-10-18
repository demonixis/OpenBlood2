#include "engine/ima_clock.h"
#include <algorithm>
#include <limits>

namespace OpenBlood2
{
    bool ImaClock::prepare(std::span<const ImaTempo> tempos, std::uint64_t origin, std::string& error)
    {
        error.clear();
        if (tempos.empty() || tempos.size() > max_tempos || tempos.front().tick != 0)
        {
            error = "IMA clock requires a bounded tempo map starting at tick zero";
            return false;
        }
        std::vector<ImaTempoBoundary> next;
        next.reserve(tempos.size());
        for (const auto& tempo : tempos)
        {
            if (tempo.whole <= 0 || tempo.tick < 0 || (!next.empty() && tempo.tick < next.back().authored.tick))
            {
                error = "IMA clock requires positive authored tempo and ordered nonnegative ticks";
                return false;
            }
            // Original normal-speed clamp precedes the 1/16 BPM clock quantization.
            const auto tempo16 = tempo.whole < 10     ? 160U
                                 : tempo.whole >= 350 ? 5600U
                                                      : (unsigned(tempo.whole) << 4) | (tempo.fraction >> 12);
            std::uint64_t time = 0;
            if (!next.empty())
            {
                const auto& previous = next.back();
                const auto delta = std::uint64_t(tempo.tick - previous.authored.tick);
                // IMRT retains truncated 100-nanosecond time between tempo nodes, then
                // truncates that accumulated value to milliseconds for event conversion.
                time = previous.time100ns + delta * 50000000 / previous.tempo16;
            }
            if (time / 10000 > std::uint64_t(std::numeric_limits<std::int32_t>::max()) ||
                time / 10000 > (std::numeric_limits<std::uint64_t>::max() - origin) / 48)
            {
                error = "IMA tempo boundary exceeds the supported clock range";
                return false;
            }
            next.push_back({tempo, tempo16, time});
        }
        boundaries_ = std::move(next);
        origin_ = origin;
        return true;
    }
    bool ImaClock::time_at(std::int32_t tick, std::uint64_t& time, std::string& error) const
    {
        time = 0;
        error.clear();
        if (boundaries_.empty() || tick < 0)
        {
            error = "IMA clock is unprepared or the boundary has a negative tick";
            return false;
        }
        const auto next = std::upper_bound(boundaries_.begin(), boundaries_.end(), tick,
                                           [](auto t, const auto& b) { return t < b.authored.tick; });
        const auto& b = *(next - 1);
        time = b.time100ns + std::uint64_t(tick - b.authored.tick) * 50000000 / b.tempo16;
        return true;
    }
    bool ImaClock::sample(std::int32_t tick, std::uint64_t& frame, std::string& error,
                          std::uint64_t retained_time100ns) const
    {
        frame = 0;
        error.clear();
        if (boundaries_.empty() || tick < 0)
        {
            error = "IMA clock is unprepared or the event has a negative tick";
            return false;
        }
        const auto next = std::upper_bound(boundaries_.begin(), boundaries_.end(), tick,
                                           [](auto time, const auto& b) { return time < b.authored.tick; });
        const auto& boundary = *(next - 1);
        const auto delta = std::uint64_t(tick - boundary.authored.tick);
        if (retained_time100ns > std::numeric_limits<std::uint64_t>::max() - boundary.time100ns)
        {
            error = "IMA retained clock time overflow";
            return false;
        }
        // Original MulDiv rounds to nearest, with positive halves upward. Preserve
        // millisecond quantization before adapting the old output rate to 48 kHz.
        const auto milliseconds = (retained_time100ns + boundary.time100ns) / 10000 +
                                  (delta * 5000 + boundary.tempo16 / 2) / boundary.tempo16;
        if (milliseconds > std::uint64_t(std::numeric_limits<std::int32_t>::max()) ||
            milliseconds > (std::numeric_limits<std::uint64_t>::max() - origin_) / 48)
        {
            error = "IMA event exceeds the supported clock range";
            return false;
        }
        frame = origin_ + milliseconds * 48;
        return true;
    }
} // namespace OpenBlood2
