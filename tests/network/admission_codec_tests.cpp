#include "net/admission.h"
#include <iostream>
#include <random>

using namespace OpenBlood2;
using namespace OpenBlood2::Net;
#define CHECK(x)                                                                                                       \
    do                                                                                                                 \
    {                                                                                                                  \
        if (!(x))                                                                                                      \
        {                                                                                                              \
            std::cerr << __LINE__ << ": " #x "\n";                                                                     \
            return false;                                                                                              \
        }                                                                                                              \
    } while (false)
namespace
{
    bool run()
    {
        Hello hello;
        for (unsigned i = 0; i < hello.content.size(); ++i)
            hello.content[i] = std::uint8_t(i);
        const std::array<AdmissionMessage, 6> messages{
            hello,         WorldOffer{0x123456789abcdef0ull, 1, 16, "worlds/multi/bb_crypt.dat"},
            Ready{2, {}},  Joined{3, 4, {65535, 9}},
            Heartbeat{10}, BaselineReady{10, 20}};
        std::vector<std::uint8_t> bytes;
        CHECK(encode_admission(hello, bytes));
        std::vector<std::uint8_t> golden{0x42, 0x32, 0x4d, 0x39, 12, 0, 1, 0};
        for (unsigned i = 0; i < 32; ++i)
            golden.push_back(std::uint8_t(i));
        CHECK(bytes == golden);
        for (const auto& message : messages)
        {
            CHECK(encode_admission(message, bytes) && bytes.size() <= admission_packet_limit);
            AdmissionMessage decoded = Heartbeat{99};
            CHECK(decode_admission(bytes, decoded) == DecodeAdmission::Ok && decoded == message);
            for (std::size_t size = 0; size < bytes.size(); ++size)
            {
                decoded = Heartbeat{99};
                CHECK(decode_admission(std::span(bytes).first(size), decoded) != DecodeAdmission::Ok);
                CHECK(decoded == AdmissionMessage(Heartbeat{99}));
            }
            auto trailing = bytes;
            trailing.push_back(0);
            CHECK(decode_admission(trailing, decoded) == DecodeAdmission::Malformed);
            auto version = bytes;
            ++version[4];
            CHECK(decode_admission(version, decoded) == DecodeAdmission::Protocol);
            auto kind = bytes;
            kind[6] = 255;
            CHECK(decode_admission(kind, decoded) == DecodeAdmission::Malformed);
        }
        const auto reject = [&](AdmissionMessage message)
        {
            auto saved = bytes;
            return !encode_admission(message, bytes) && bytes == saved;
        };
        CHECK(reject(Joined{1, 1, {65536, 1}}));
        CHECK(reject(Joined{1, 1, {1, 0}}));
        CHECK(reject(Heartbeat{0}));
        CHECK(reject(WorldOffer{1, 1, 17, "worlds/multi/bb_crypt.dat"}));
        CHECK(reject(WorldOffer{1, 1, 16, "worlds/multi/bb_../secret.dat"}));
        CHECK(reject(WorldOffer{1, 1, 16, "worlds/multi/ctf_crypt.dat"}));
        Ready ready{1, {}};
        ready.profile.name = std::string(101, 'x');
        CHECK(reject(ready));
        ready.profile.name = "line\nbreak";
        CHECK(reject(ready));
        ready.profile.name = "";
        CHECK(reject(ready));
        ready.profile = {};
        ready.profile.character = 4;
        CHECK(reject(ready));
        ready.profile = {};
        ready.profile.skin = 7;
        CHECK(reject(ready));
        ready.profile = {};
        ready.profile.attributes[0] = 0;
        CHECK(reject(ready));
        ready.profile = {};
        ready.profile.weapons[0] = 18;
        CHECK(reject(ready));
        ready.profile.weapons[0] = 128;
        CHECK(reject(ready));
        ready.profile.weapons[0] = 129;
        CHECK(encode_admission(ready, bytes));
        std::mt19937 rng(0xB209);
        AdmissionMessage out;
        for (unsigned i = 0; i < 100000; ++i)
        {
            if (i & 1)
            {
                CHECK(encode_admission(messages[i % messages.size()], bytes));
                bytes[rng() % bytes.size()] ^= std::uint8_t(rng());
            }
            else
            {
                bytes.resize(rng() % 300);
                for (auto& b : bytes)
                    b = std::uint8_t(rng());
            }
            if (decode_admission(bytes, out) == DecodeAdmission::Ok)
            {
                std::vector<std::uint8_t> canonical;
                CHECK(encode_admission(out, canonical) && canonical == bytes);
            }
        }
        std::cout
            << "Admission golden bytes, atomic decode, every truncation, bounded fields and 100000 seeded mutations\n";
        return true;
    }
} // namespace
int main()
{
    return run() ? 0 : 1;
}
