#include "net/discovery.h"
#include <algorithm>
#include <chrono>
#include <iostream>
#include <random>
#include <thread>
using namespace OpenBlood2::Net;
#define CHECK(x)                                                                                                       \
    do                                                                                                                 \
    {                                                                                                                  \
        if (!(x))                                                                                                      \
        {                                                                                                              \
            std::cerr << __LINE__ << ": " #x ": " << error << '\n';                                                    \
            return false;                                                                                              \
        }                                                                                                              \
    } while (false)
namespace
{
    bool run()
    {
        std::string error;
        LanMessage sample{42, LanOffer{protocol_version, 27888, 1, 2, 16, "LAN host", "worlds/multi/bb_crypt.dat"}};
        std::vector<std::uint8_t> bytes, again;
        CHECK(encode_lan(sample, bytes));
        CHECK(bytes.size() <= discovery_packet_limit && bytes[6] == 42);
        LanMessage decoded;
        decoded.nonce = 999;
        for (std::size_t i = 0; i < bytes.size(); ++i)
            CHECK(!decode_lan(std::span(bytes).first(i), decoded) && decoded.nonce == 999);
        CHECK(decode_lan(bytes, decoded) && encode_lan(decoded, again) && again == bytes);
        std::minstd_rand rng(25);
        for (unsigned i = 0; i < 10000; ++i)
        {
            auto corrupted = bytes;
            corrupted[rng() % corrupted.size()] = std::uint8_t(rng());
            if (decode_lan(corrupted, decoded))
                CHECK(encode_lan(decoded, again) && again == corrupted);
        }
        auto invalid = sample;
        invalid.offer->players = 17;
        CHECK(!encode_lan(invalid, bytes));
        invalid = sample;
        invalid.offer->world += '\n';
        CHECK(!encode_lan(invalid, bytes));
        invalid = sample;
        invalid.offer->name = std::string(33, 'x');
        CHECK(!encode_lan(invalid, bytes));
        invalid = sample;
        invalid.offer->capacity = 0;
        CHECK(!encode_lan(invalid, bytes));
        invalid = sample;
        invalid.offer->mode = 3;
        CHECK(!encode_lan(invalid, bytes));
        CHECK(!decode_lan(std::vector<std::uint8_t>(discovery_packet_limit + 1), decoded));
        LanDiscovery server, client;
        CHECK(server.open(true, error, 0, "127.0.0.1") && server.port());
        CHECK(client.open(false, error) && client.query(0, error, "127.0.0.1", server.port()));
        auto offer = *sample.offer;
        const auto exchange = [&](double now)
        {
            for (unsigned i = 0; i < 50; ++i)
            {
                if (!server.pump(now, &offer, error) || !client.pump(now, nullptr, error))
                    return false;
                if (!client.servers().empty())
                    return true;
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
            return false;
        };
        CHECK(exchange(0));
        CHECK(client.servers().size() == 1 && client.servers()[0].address.host == "127.0.0.1" &&
              client.servers()[0].address.port == offer.port && client.servers()[0].offer.players == 2);
        // A new query nonce updates one existing row; incompatible versions are visible for UI refusal.
        offer.protocol++;
        offer.players = 16;
        offer.world = "worlds/multi/bb_bridge.dat";
        CHECK(client.query(2, error, "127.0.0.1", server.port()));
        CHECK(exchange(2));
        CHECK(client.servers().size() == 1 && client.servers()[0].offer.protocol == protocol_version + 1 &&
              client.servers()[0].offer.players == 16 && client.servers()[0].offer.world == offer.world);
        CHECK(client.pump(8.01, nullptr, error) && client.servers().empty());
        CHECK(!client.pump(1, nullptr, error));
        CHECK(!client.query(8.02, error, "not an IPv4 address", server.port()));
        server.close();
        client.close();
        CHECK(!server.port() && !client.port() && client.servers().empty());
        CHECK(client.open(false, error) && client.pump(0, nullptr, error));
        std::cout << "LAN: bounded/endian codec, all prefixes, 10k mutations, actual UDP request/reply, "
                     "deduplication, incompatible/full status, expiry, clock validation and cleanup passed\n";
        return true;
    }
} // namespace
int main()
{
    return run() ? 0 : 1;
}
