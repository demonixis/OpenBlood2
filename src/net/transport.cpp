#include "net/transport.h"
#include "net/enet_runtime.h"
#include <enet/enet.h>
#include <limits>
#include <mutex>

namespace OpenBlood2::Net
{
    namespace
    {
        constexpr std::uint32_t connection_magic = 0x4f423209;
        bool address_for(std::string_view name, std::uint16_t port, ENetAddress& address)
        {
            if (name.empty() || name.size() > 253 || name.find('\0') != name.npos)
                return false;
            address.port = port;
            return enet_address_set_host(&address, std::string(name).c_str()) == 0;
        }
        void release_packet(ENetPacket* packet)
        {
            auto& queued = *static_cast<std::size_t*>(packet->userData);
            queued -= packet->dataLength;
        }
    } // namespace
    std::mutex runtime_mutex;
    unsigned runtime_users = 0;
    bool acquire_enet_runtime()
    {
        std::lock_guard lock(runtime_mutex);
        if (!runtime_users && enet_initialize() != 0)
            return false;
        ++runtime_users;
        return true;
    }
    void release_enet_runtime()
    {
        std::lock_guard lock(runtime_mutex);
        if (!--runtime_users)
            enet_deinitialize();
    }
    Transport::~Transport()
    {
        close();
    }

    bool Transport::create(std::string_view address, std::uint16_t port, std::size_t capacity, bool listening,
                           std::string& error)
    {
        error.clear();
        if (host_ || !capacity || capacity > max_players)
        {
            error = "Transport already open or invalid player capacity";
            return false;
        }
        if (!acquire_enet_runtime())
        {
            error = "ENet initialization failed";
            return false;
        }
        // Windows address resolution requires ENet's WSAStartup first.
        ENetAddress bind{};
        if (listening && !address_for(address, port, bind))
        {
            release_enet_runtime();
            error = "Invalid IPv4 bind address";
            return false;
        }
        host_ = enet_host_create(listening ? &bind : nullptr, capacity, unsigned(Channel::Count), 0, 0);
        if (!host_)
        {
            release_enet_runtime();
            error = "Cannot create ENet host (address or port unavailable)";
            return false;
        }
        host_->maximumPacketSize = packet_limit(Channel::Baseline);
        host_->maximumWaitingData = queue_limit;
        slots_.resize(capacity);
        listening_ = listening;
        return true;
    }
    bool Transport::listen(std::string_view address, std::uint16_t port, std::size_t capacity, std::string& error)
    {
        return create(address, port, capacity, true, error);
    }
    bool Transport::open_client(std::string& error)
    {
        return create({}, 0, 1, false, error);
    }

    PeerId Transport::connect(std::string_view name, std::uint16_t port, std::string& error)
    {
        error.clear();
        ENetAddress address{};
        if (!host_ || listening_ || !port || !next_id_ || !address_for(name, port, address))
        {
            error = "Invalid client connection request";
            return 0;
        }
        auto* peer = enet_host_connect(host_, &address, unsigned(Channel::Count), connection_magic);
        if (!peer)
        {
            error = "No available connection slot";
            return 0;
        }
        const auto index = std::size_t(peer - host_->peers);
        slots_[index] = {next_id_++, 0, false};
        enet_peer_timeout(peer, 32, 5000, 15000);
        return slots_[index].id;
    }
    std::size_t Transport::slot(PeerId id) const
    {
        if (id)
            for (std::size_t i = 0; i < slots_.size(); ++i)
                if (slots_[i].id == id)
                    return i;
        return slots_.size();
    }
    PollResult Transport::poll(Event& result, std::uint32_t wait_ms, std::string& error)
    {
        error.clear();
        if (!host_ || wait_ms > 1000)
        {
            error = "Invalid transport poll";
            return PollResult::Failed;
        }
        ENetEvent event{};
        const int status = enet_host_service(host_, &event, wait_ms);
        if (status < 0)
        {
            error = "ENet service failed";
            return PollResult::Failed;
        }
        if (!status)
            return PollResult::Idle;
        auto& entry = slots_[std::size_t(event.peer - host_->peers)];
        if (event.type == ENET_EVENT_TYPE_CONNECT)
        {
            if (event.peer->channelCount != unsigned(Channel::Count) ||
                (listening_ && (event.data != connection_magic || !next_id_)))
            {
                enet_peer_disconnect_now(event.peer, 1);
                return PollResult::Idle;
            }
            if (listening_)
                entry = {next_id_++, 0, true};
            entry.connected = true;
            enet_peer_timeout(event.peer, 32, 5000, 15000);
            result = {EventKind::Connected, entry.id, Channel::Control, 0, {}};
        }
        else if (event.type == ENET_EVENT_TYPE_DISCONNECT)
        {
            result = {EventKind::Disconnected, entry.id, Channel::Control, event.data, {}};
            entry = {};
        }
        else if (event.type == ENET_EVENT_TYPE_RECEIVE)
        {
            const auto channel = Channel(event.channelID);
            const bool reliable = (event.packet->flags & ENET_PACKET_FLAG_RELIABLE) != 0;
            const bool valid =
                entry.connected && event.packet->dataLength > 0 && event.packet->dataLength <= packet_limit(channel) &&
                // ENet promotes an unreliable packet to reliable when its
                // 16-bit sequence wraps. That is valid on the State channel.
                (channel == Channel::State || reliable) && !(event.packet->flags & ENET_PACKET_FLAG_UNSEQUENCED);
            if (valid)
                result = {EventKind::Received,
                          entry.id,
                          channel,
                          0,
                          {event.packet->data, event.packet->data + event.packet->dataLength}};
            enet_packet_destroy(event.packet);
            if (!valid)
            {
                result = {EventKind::Disconnected, entry.id, Channel::Control, 1, {}};
                enet_peer_disconnect_now(event.peer, 1);
                entry = {};
            }
        }
        else
            return PollResult::Idle;
        return PollResult::Event;
    }
    bool Transport::send(PeerId id, Channel channel, std::span<const std::uint8_t> bytes, std::string& error)
    {
        error.clear();
        const auto index = slot(id);
        if (!host_ || index == slots_.size() || !slots_[index].connected || bytes.empty() ||
            bytes.size() > packet_limit(channel))
        {
            error = "Invalid peer, channel or packet size";
            return false;
        }
        auto& entry = slots_[index];
        if (bytes.size() > queue_limit - entry.queued)
        {
            error = "Peer outgoing queue budget exceeded";
            return false;
        }
        auto* packet =
            enet_packet_create(bytes.data(), bytes.size(), channel == Channel::State ? 0 : ENET_PACKET_FLAG_RELIABLE);
        if (!packet)
        {
            error = "Cannot allocate ENet packet";
            return false;
        }
        entry.queued += bytes.size();
        packet->userData = &entry.queued;
        packet->freeCallback = release_packet;
        if (enet_peer_send(&host_->peers[index], std::uint8_t(channel), packet) != 0)
        {
            enet_packet_destroy(packet);
            error = "ENet refused packet";
            return false;
        }
        return true;
    }
    bool Transport::disconnect(PeerId id, std::uint32_t reason)
    {
        const auto index = slot(id);
        if (!host_ || index == slots_.size())
            return false;
        enet_peer_disconnect(&host_->peers[index], reason);
        slots_[index].connected = false;
        return true;
    }
    bool Transport::stats(PeerId id, PeerStats& result) const
    {
        const auto index = slot(id);
        if (!host_ || index == slots_.size())
            return false;
        const auto& peer = host_->peers[index];
        result = {peer.roundTripTime, peer.roundTripTimeVariance, peer.packetsSent, peer.packetsLost,
                  slots_[index].queued};
        return true;
    }
    void Transport::flush()
    {
        if (host_)
            enet_host_flush(host_);
    }
    void Transport::close()
    {
        if (!host_)
            return;
        // Destruction releases packets while their queue counters are still alive.
        enet_host_destroy(host_);
        host_ = nullptr;
        slots_.clear();
        listening_ = false;
        release_enet_runtime();
    }
    std::uint16_t Transport::port() const
    {
        ENetAddress address{};
        return host_ && enet_socket_get_address(host_->socket, &address) == 0 ? address.port : 0;
    }
} // namespace OpenBlood2::Net
