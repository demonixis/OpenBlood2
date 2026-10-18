#pragma once
#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace OpenBlood2
{
    struct ImaTempo
    {
        std::int32_t tick = 0;
        std::int16_t whole = 120;
        std::uint16_t fraction = 0;
    };
    struct ImaTempoBoundary
    {
        ImaTempo authored;
        unsigned tempo16 = 0;        // Effective clamped tempo in sixteenths of a BPM.
        std::uint64_t time100ns = 0; // Original retained fractional time at this tempo boundary.
    };
    // Original IMRT25 normal-speed tempo map, with an explicit 48 kHz output origin.
    // This value object neither advances a cursor nor owns any audio/game resources.
    class ImaClock
    {
      public:
        static constexpr unsigned sample_rate = 48000;
        static constexpr std::size_t max_tempos = 4096;
        // First tick must be zero; later nonnegative ticks are nondecreasing. Equal-tick
        // changes retain submission order, with the last one active at that boundary.
        // Failure preserves this map. Unsupported signed times/overflow fail explicitly.
        bool prepare(std::span<const ImaTempo>, std::uint64_t origin_frame, std::string& error);
        bool sample(std::int32_t tick, std::uint64_t& frame, std::string& error,
                    std::uint64_t retained_time100ns = 0) const;
        // Truncated time at a new tempo boundary, before millisecond event rounding.
        bool time_at(std::int32_t tick, std::uint64_t& time100ns, std::string& error) const;
        const auto& boundaries() const
        {
            return boundaries_;
        }
        std::uint64_t origin_frame() const
        {
            return origin_;
        }

      private:
        std::vector<ImaTempoBoundary> boundaries_;
        std::uint64_t origin_ = 0;
    };
} // namespace OpenBlood2
