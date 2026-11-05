#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace OpenBlood2
{
    class ServerClock
    {
      public:
        static constexpr double step = 1.0 / 60;
        static constexpr unsigned catchup_limit = 6;
        // Invalid time never mutates the clock. Excess whole ticks are discarded
        // and counted; only a sub-tick remainder survives to the next iteration.
        bool advance(double seconds, unsigned& ticks)
        {
            if (!std::isfinite(seconds) || seconds < 0 || seconds > 86400)
                return false;
            const double total = remainder_ + seconds;
            const auto due = std::uint64_t(std::floor(total / step + 1e-9));
            ticks = unsigned(std::min<std::uint64_t>(due, catchup_limit));
            remainder_ = std::max(0.0, total - double(due) * step);
            discarded_ticks_ += due - ticks;
            return true;
        }
        std::uint64_t discarded_ticks() const
        {
            return discarded_ticks_;
        }
        double remainder() const
        {
            return remainder_;
        }

      private:
        double remainder_ = 0;
        std::uint64_t discarded_ticks_ = 0;
    };
} // namespace OpenBlood2
