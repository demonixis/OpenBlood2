#pragma once
#include "game/player_messages.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
namespace OpenBlood2
{
    // Render frames may produce no simulation tick or several (at most six; the
    // rest of a long frame is discarded). Mouse movement offsets are consumed
    // once across the ticks that run; digital controls remain held.
    class NetworkInput
    {
      public:
        static constexpr double step = 1. / 60;
        bool advance(double seconds, const MovementInput& value, bool commands, unsigned& ticks, MovementInput& sampled)
        {
            if (!std::isfinite(seconds) || seconds < 0 || seconds > 86400 || !std::isfinite(value.mouse_forward) ||
                std::abs(value.mouse_forward) > 1e7f || !std::isfinite(value.mouse_strafe) ||
                std::abs(value.mouse_strafe) > 1e7f)
                return false;
            const double forward = commands && value.mouse_move ? forward_ + value.mouse_forward : 0;
            const double strafe = commands ? strafe_ + value.mouse_strafe : 0;
            if (std::abs(forward) > 1e7 || std::abs(strafe) > 1e7)
                return false;
            const double total = remainder_ + seconds;
            const auto due = std::uint64_t(std::floor(total / step + 1e-9));
            ticks = unsigned(std::min<std::uint64_t>(due, 6));
            remainder_ = std::max(0., total - double(due) * step);
            discarded_ += due - ticks;
            sampled = commands ? value : MovementInput{};
            // Spread over the ticks that actually run: discarded catch-up time
            // never discards mouse movement.
            sampled.mouse_forward = ticks ? float(forward / double(ticks)) : 0;
            sampled.mouse_strafe = ticks ? float(strafe / double(ticks)) : 0;
            forward_ = due ? 0 : forward;
            strafe_ = due ? 0 : strafe;
            return true;
        }
        std::uint64_t discarded_ticks() const
        {
            return discarded_;
        }

      private:
        double remainder_ = 0, forward_ = 0, strafe_ = 0;
        std::uint64_t discarded_ = 0;
    };
} // namespace OpenBlood2
