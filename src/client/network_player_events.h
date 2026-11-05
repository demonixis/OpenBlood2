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
        const std::vector<Net::RosterPlayer>& roster() const
        {
            return roster_;
        }
        const Net::RosterPlayer* player(ObjectId) const;

      private:
        Net::Joined owner_;
        ClientHud hud_;
        std::uint64_t sequence_ = 0, tick_ = 0;
        std::vector<Net::PrivateEvent> events_;
        std::vector<Net::RosterPlayer> roster_;
        std::uint64_t highest_player_ = 0;
        bool replace_roster(const Net::PlayerRoster&, std::string& error);
    };
} // namespace OpenBlood2
