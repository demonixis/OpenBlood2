#pragma once
#include "net/address.h"
#include "net/admission.h"
#include "net/transport.h"
#include <memory>
#include <optional>
namespace OpenBlood2::Net
{
    inline constexpr std::uint16_t discovery_port = 27889;
    inline constexpr std::size_t discovery_packet_limit = 256;
    struct LanOffer
    {
        std::uint16_t protocol = protocol_version, port = default_port;
        std::uint8_t mode = 1, players = 0, capacity = 16;
        std::string name = "Blood II", world;
        std::uint64_t instance = 1; // Distinguishes hosts while merging their LAN/loopback addresses.
    };
    struct LanMessage
    {
        std::uint64_t nonce = 0;
        std::optional<LanOffer> offer; // Absent: query. Separate from ENet admission.
    };
    bool encode_lan(const LanMessage&, std::vector<std::uint8_t>&);
    bool decode_lan(std::span<const std::uint8_t>, LanMessage&);
    struct LanServer
    {
        ServerAddress address;
        LanOffer offer;
        double seen = 0;
    };
    // One nonblocking socket, at most 64 datagrams/pump, 20 replies/second and 64 rows.
    // Untrusted advertisements never bypass the normal protocol/content admission.
    class LanDiscovery
    {
      public:
        LanDiscovery();
        ~LanDiscovery();
        LanDiscovery(const LanDiscovery&) = delete;
        LanDiscovery& operator=(const LanDiscovery&) = delete;
        bool open(bool responder, std::string& error, std::uint16_t port = discovery_port,
                  std::string_view bind = "0.0.0.0");
        void close();
        bool query(double now, std::string& error, std::string_view target = "255.255.255.255",
                   std::uint16_t port = discovery_port);
        bool pump(double now, const LanOffer*, std::string& error);
        std::uint16_t port() const;
        const std::vector<LanServer>& servers() const
        {
            return servers_;
        }

      private:
        struct Socket;
        std::unique_ptr<Socket> socket_;
        std::vector<LanServer> servers_;
        std::uint64_t nonce_ = 0;
        double last_time_ = -1, queried_ = -10, reply_window_ = -1;
        unsigned replies_ = 0;
    };
} // namespace OpenBlood2::Net
