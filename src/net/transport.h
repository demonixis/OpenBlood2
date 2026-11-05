#pragma once
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

struct _ENetHost;

namespace OpenBlood2::Net
{
    using PeerId = std::uint64_t;
    inline constexpr std::uint16_t default_port = 27888;
    inline constexpr std::size_t max_players = 16;
    enum class Channel : std::uint8_t
    {
        Control,
        Baseline,
        State,
        Count
    };
    constexpr std::size_t packet_limit(Channel channel)
    {
        return channel == Channel::Control    ? 16384
               : channel == Channel::Baseline ? 32768
               : channel == Channel::State    ? 1200
                                              : 0;
    }
    enum class EventKind
    {
        Connected,
        Disconnected,
        Received
    };
    struct Event
    {
        EventKind kind = EventKind::Disconnected;
        PeerId peer = 0;
        Channel channel = Channel::Control;
        std::uint32_t reason = 0;
        std::vector<std::uint8_t> bytes;
    };
    enum class PollResult
    {
        Idle,
        Event,
        Failed
    };
    struct PeerStats
    {
        std::uint32_t round_trip_ms = 0, round_trip_variance_ms = 0;
        std::uint32_t sent_packets = 0, lost_packets = 0;
        std::size_t queued_bytes = 0;
    };
    // Single-thread owner; no borrowed packets/ENetPeer pointers escape this boundary.
    // Peer IDs are never reused, even after close/open. A transport connection is
    // NOT game admission: protocol/content checks must finish before player creation.
    class Transport
    {
      public:
        static constexpr std::size_t queue_limit = 2 * 1024 * 1024;
        Transport() = default;
        Transport(const Transport&) = delete;
        Transport& operator=(const Transport&) = delete;
        ~Transport();
        bool listen(std::string_view address, std::uint16_t port, std::size_t capacity, std::string& error);
        bool open_client(std::string& error);
        PeerId connect(std::string_view host, std::uint16_t port, std::string& error);
        PollResult poll(Event&, std::uint32_t wait_ms, std::string& error);
        bool send(PeerId, Channel, std::span<const std::uint8_t>, std::string& error);
        bool disconnect(PeerId, std::uint32_t reason);
        bool stats(PeerId, PeerStats&) const;
        void flush();
        void close();
        std::uint16_t port() const;
        bool is_open() const
        {
            return host_ != nullptr;
        }

      private:
        struct Slot
        {
            PeerId id = 0;
            std::size_t queued = 0;
            bool connected = false;
        };
        _ENetHost* host_ = nullptr;
        std::vector<Slot> slots_;
        PeerId next_id_ = 1;
        bool listening_ = false;
        bool create(std::string_view, std::uint16_t, std::size_t, bool, std::string&);
        std::size_t slot(PeerId) const;
    };
} // namespace OpenBlood2::Net
