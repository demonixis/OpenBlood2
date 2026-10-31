#include "net/discovery.h"
#include "game/game_settings.h"
#include "net/codec.h"
#include "net/enet_runtime.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <enet/enet.h>
namespace OpenBlood2::Net
{
    namespace
    {
        constexpr std::uint32_t magic = 0x4e4c3242; // B2LN, independent discovery schema 1.
        bool text_ok(std::string_view s, std::size_t max)
        {
            return !s.empty() && s.size() <= max &&
                   std::all_of(s.begin(), s.end(), [](unsigned char c) { return c >= 32 && c != 127; });
        }
    } // namespace
    bool encode_lan(const LanMessage& m, std::vector<std::uint8_t>& out)
    {
        if (!m.nonce)
            return false;
        if (m.offer)
        {
            const auto& o = *m.offer;
            if (!o.instance || !o.port || !o.protocol || !network_game_type(o.mode) || !o.capacity ||
                o.capacity > max_players || o.players > o.capacity || !text_ok(o.name, 32) || !text_ok(o.world, 100))
                return false;
        }
        Writer w(discovery_packet_limit);
        w.u32(magic);
        w.u8(1);
        w.u8(m.offer.has_value());
        w.u64(m.nonce);
        if (m.offer)
        {
            const auto& o = *m.offer;
            w.u64(o.instance);
            w.u16(o.protocol);
            w.u16(o.port);
            w.u8(o.mode);
            w.u8(o.players);
            w.u8(o.capacity);
            w.text(o.name, 32);
            w.text(o.world, 100);
        }
        if (!w.good())
            return false;
        out = w.bytes();
        return true;
    }
    bool decode_lan(std::span<const std::uint8_t> bytes, LanMessage& out)
    {
        if (bytes.size() > discovery_packet_limit)
            return false;
        Reader r(bytes);
        if (r.u32() != magic || r.u8() != 1)
            return false;
        const auto kind = r.u8();
        if (kind > 1)
            return false;
        LanMessage m;
        m.nonce = r.u64();
        if (kind)
        {
            m.offer.emplace();
            auto& o = *m.offer;
            o.instance = r.u64();
            o.protocol = r.u16();
            o.port = r.u16();
            o.mode = r.u8();
            o.players = r.u8();
            o.capacity = r.u8();
            o.name = r.text(32);
            o.world = r.text(100);
        }
        std::vector<std::uint8_t> checked;
        if (!r.done() || !encode_lan(m, checked))
            return false;
        out = std::move(m);
        return true;
    }
    struct LanDiscovery::Socket
    {
        ENetSocket handle = ENET_SOCKET_NULL;
        bool responder = false, runtime = false;
        ~Socket()
        {
            if (handle != ENET_SOCKET_NULL)
                enet_socket_destroy(handle);
            if (runtime)
                release_enet_runtime();
        }
    };
    LanDiscovery::LanDiscovery() = default;
    LanDiscovery::~LanDiscovery() = default;
    void LanDiscovery::close()
    {
        socket_.reset();
        servers_.clear();
        nonce_ = 0;
        last_time_ = -1;
        queried_ = -10;
        reply_window_ = -1;
        replies_ = 0;
    }
    bool LanDiscovery::open(bool responder, std::string& error, std::uint16_t port, std::string_view bind)
    {
        close();
        error.clear();
        auto s = std::make_unique<Socket>();
        if (!(s->runtime = acquire_enet_runtime()))
        {
            error = "Cannot initialize LAN networking";
            return false;
        }
        s->handle = enet_socket_create(ENET_SOCKET_TYPE_DATAGRAM);
        s->responder = responder;
        ENetAddress address{};
        address.port = responder ? port : 0;
        if (bind.size() > 253 || bind.find('\0') != bind.npos ||
            enet_address_set_host_ip(&address, std::string(bind).c_str()) || s->handle == ENET_SOCKET_NULL ||
            enet_socket_set_option(s->handle, ENET_SOCKOPT_NONBLOCK, 1) ||
            enet_socket_set_option(s->handle, ENET_SOCKOPT_BROADCAST, 1) ||
            (responder && enet_socket_set_option(s->handle, ENET_SOCKOPT_REUSEADDR, 1)) ||
            enet_socket_bind(s->handle, &address))
        {
            error = "LAN discovery socket unavailable; direct connection remains available";
            return false;
        }
        socket_ = std::move(s);
        nonce_ = std::uint64_t(std::chrono::steady_clock::now().time_since_epoch().count()) | 1;
        return true;
    }
    std::uint16_t LanDiscovery::port() const
    {
        ENetAddress address{};
        return socket_ && !enet_socket_get_address(socket_->handle, &address) ? address.port : 0;
    }
    bool LanDiscovery::query(double now, std::string& error, std::string_view target, std::uint16_t port)
    {
        error.clear();
        if (!socket_ || socket_->responder || !std::isfinite(now) || now < 0 || now < last_time_ || !port ||
            target.size() > 253 || target.find('\0') != target.npos)
        {
            error = "Invalid LAN search";
            return false;
        }
        if (now - queried_ < 1)
            return true;
        queried_ = now;
        if (++nonce_ == 0)
            ++nonce_;
        std::vector<std::uint8_t> bytes;
        if (!encode_lan({nonce_, {}}, bytes))
            return false;
        ENetAddress address{};
        address.port = port;
        if (enet_address_set_host_ip(&address, std::string(target).c_str()))
        {
            error = "Invalid LAN search address";
            return false;
        }
        ENetBuffer b{};
        b.data = bytes.data();
        b.dataLength = bytes.size();
        if (enet_socket_send(socket_->handle, &address, &b, 1) != int(bytes.size()))
        {
            error = "LAN broadcast unavailable; enter an address to connect";
            return false;
        }
        // Broadcast is often not looped back by the OS; include local hosted games.
        if (target == "255.255.255.255")
        {
            enet_address_set_host_ip(&address, "127.0.0.1");
            enet_socket_send(socket_->handle, &address, &b, 1);
        }
        return true;
    }
    bool LanDiscovery::pump(double now, const LanOffer* offer, std::string& error)
    {
        error.clear();
        if (!socket_ || !std::isfinite(now) || now < 0 || now < last_time_ || socket_->responder != bool(offer))
        {
            error = "Invalid LAN discovery clock/state";
            return false;
        }
        last_time_ = now;
        std::erase_if(servers_, [now](const auto& s) { return now - s.seen > 6; });
        if (now - reply_window_ >= 1)
        {
            reply_window_ = now;
            replies_ = 0;
        }
        for (unsigned i = 0; i < 64; ++i)
        {
            std::array<std::uint8_t, discovery_packet_limit + 1> bytes{};
            ENetAddress address{};
            ENetBuffer b{};
            b.data = bytes.data();
            b.dataLength = bytes.size();
            const int n = enet_socket_receive(socket_->handle, &address, &b, 1);
            if (!n)
                break;
            if (n == -2)
                continue; // Oversized/truncated UDP datagram, never a session failure.
            if (n < 0)
            {
                error = "LAN discovery receive failed";
                return false;
            }
            LanMessage m;
            if (!decode_lan(std::span(bytes).first(std::size_t(n)), m))
                continue;
            if (offer && !m.offer && replies_ < 20)
            {
                std::vector<std::uint8_t> reply;
                if (!encode_lan({m.nonce, *offer}, reply))
                {
                    error = "Invalid LAN offer";
                    return false;
                }
                ++replies_;
                ENetBuffer buffer{};
                buffer.data = reply.data();
                buffer.dataLength = reply.size();
                enet_socket_send(socket_->handle, &address, &buffer, 1);
            }
            else if (!offer && m.offer && m.nonce == nonce_ && now - queried_ <= 3)
            {
                char ip[64]{};
                if (enet_address_get_host_ip(&address, ip, sizeof ip))
                    continue;
                const auto found =
                    std::find_if(servers_.begin(), servers_.end(), [&](const auto& s)
                                 { return s.offer.instance == m.offer->instance && s.address.port == m.offer->port; });
                LanServer row{{ip, m.offer->port}, *m.offer, now};
                if (found != servers_.end())
                {
                    if (row.address.host == "127.0.0.1" && found->address.host != "127.0.0.1")
                        row.address = found->address;
                    *found = std::move(row);
                }
                else if (servers_.size() < 64)
                    servers_.push_back(std::move(row));
            }
        }
        return true;
    }
} // namespace OpenBlood2::Net
