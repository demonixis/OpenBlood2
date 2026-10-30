#pragma once
#include <algorithm>
#include <cmath>
namespace OpenBlood2
{
    // Map newer server stamps to a monotonic client presentation clock. Repeated
    // fragments cannot restart it; silence permits only bounded time advancement.
    class NetworkClock
    {
      public:
        static constexpr double interpolation_delay = .1, maximum_lead = .25;
        bool observe(double server, double arrival, double one_way)
        {
            if (!std::isfinite(server) || server < 0 || server > 1e12 || !std::isfinite(arrival) ||
                arrival < last_local_ || !std::isfinite(one_way) || one_way < 0 || one_way > 60)
                return false;
            last_local_ = arrival;
            if (ready_ && server <= server_)
                return true;
            server_ = server;
            anchor_ = arrival;
            latency_ = std::min(one_way, maximum_lead);
            ready_ = true;
            return true;
        }
        bool sample(double local, double& out)
        {
            if (!ready_ || !std::isfinite(local) || local < last_local_)
                return false;
            last_local_ = local;
            presented_ = std::max(presented_, server_ + std::min(maximum_lead, latency_ + local - anchor_));
            out = presented_;
            return true;
        }
        bool ready() const
        {
            return ready_;
        }

      private:
        bool ready_ = false;
        double server_ = 0, anchor_ = 0, latency_ = 0, presented_ = 0, last_local_ = 0;
    };
} // namespace OpenBlood2
