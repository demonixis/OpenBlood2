#pragma once
#include "net/transport.h"
#include "server/session.h"
#include <chrono>

namespace OpenBlood2
{
    // One owner/thread, shared by the console server and eventual playable host.
    // Transport peers acquire original players only after both admission stages.
    class ServerNetwork
    {
      public:
        using Clock = std::chrono::steady_clock;
        explicit ServerNetwork(ServerSession& session) : session_(session) {}
        ~ServerNetwork();
        bool listen(std::string_view, std::uint16_t port, unsigned capacity, std::string& error);
        bool pump(Clock::time_point now, std::string& error);
        bool close();
        std::uint16_t port() const
        {
            return transport_.port();
        }
        std::size_t peer_count() const
        {
            return peers_.size();
        }
        std::uint64_t admitted = 0, departed = 0, rejected = 0, timed_out = 0;
        std::uint64_t membership_updates = 0;
        std::uint64_t state_updates = 0, state_drops = 0, state_bytes = 0;
        std::string last_rejection;

      private:
        enum class Phase
        {
            Hello,
            Loading,
            Baseline,
            Joined
        };
        struct Peer
        {
            Net::PeerId peer = 0;
            Phase phase = Phase::Hello;
            std::uint64_t player = 0;
            Clock::time_point deadline, heartbeat;
            Net::WorldSnapshot previous;
            std::uint64_t publish_at = 0;
        };
        ServerSession& session_;
        Net::Transport transport_;
        std::vector<Peer> peers_;
        unsigned capacity_ = 0;
        bool clock_started_ = false;
        Clock::time_point last_time_;
        bool remove(std::size_t index, std::uint32_t reason, bool disconnect, std::string& error);
        bool receive(std::size_t index, const Net::Event&, Clock::time_point, std::string& error);
        bool send(Net::PeerId, const Net::AdmissionMessage&, std::string& error);
    };
} // namespace OpenBlood2
