#pragma once
#include "client/hud.h"
#include "net/player_events.h"

namespace OpenBlood2
{
    // Reliable addressed inbox. HUD values are applied exactly once; the bounded
    // owned events are handed to original presentation consumers by take().
    class NetworkPlayerEvents
    {
      public:
        enum class Reception
        {
            Applied,
            Ignored,
            Refused
        };
        bool begin(const Net::Joined&);
        Reception receive(const Net::PrivateEvent&, std::string& error);
        void take(std::vector<Net::PrivateEvent>& out)
        {
            out.clear();
            out.swap(events_);
        }
        const ClientHud& hud() const
        {
            return hud_;
        }
        std::uint64_t sequence() const
        {
            return sequence_;
        }
        std::size_t pending() const
        {
            return events_.size();
        }

      private:
        Net::Joined owner_;
        ClientHud hud_;
        std::uint64_t sequence_ = 0, tick_ = 0;
        std::vector<Net::PrivateEvent> events_;
    };
} // namespace OpenBlood2
