#include "net/codec.h"
#include "net/content.h"
#include "server/clock.h"
#include <fstream>
#include <iostream>
#include <limits>
#include <random>

using namespace OpenBlood2;
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
    bool codec()
    {
        Net::Writer writer(64);
        writer.u16(0x1234);
        writer.u32(0x89abcdef);
        writer.u64(0xfedcba9876543210ULL);
        writer.f32(1.0f);
        writer.text("abc", 3);
        const std::vector<std::uint8_t> golden{0x34, 0x12, 0xef, 0xcd, 0xab, 0x89, 0x10, 0x32, 0x54, 0x76, 0x98, 0xba,
                                               0xdc, 0xfe, 0,    0,    0x80, 0x3f, 3,    0,    'a',  'b',  'c'};
        CHECK(writer.good() && writer.bytes() == golden);
        Net::Reader reader(golden);
        CHECK(reader.u16() == 0x1234 && reader.u32() == 0x89abcdef && reader.u64() == 0xfedcba9876543210ULL);
        CHECK(reader.f32() == 1 && reader.text(3) == "abc" && reader.done());
        for (std::size_t i = 0; i < golden.size(); ++i)
        {
            Net::Reader truncated{std::span(golden).first(i)};
            truncated.u16();
            truncated.u32();
            truncated.u64();
            truncated.f32();
            truncated.text(3);
            CHECK(!truncated.done());
        }
        Net::Writer tiny(1);
        tiny.u16(42);
        CHECK(!tiny.good() && tiny.bytes().size() <= 1);
        Net::Writer nan(4);
        nan.f32(std::numeric_limits<float>::quiet_NaN());
        CHECK(!nan.good());
        Net::Writer text(100);
        text.text(std::string_view("x\0y", 3), 10);
        CHECK(!text.good());
        const std::vector<std::uint8_t> invalid_float{0, 0, 0x80, 0x7f};
        Net::Reader infinity(invalid_float);
        infinity.f32();
        CHECK(!infinity.good());
        const std::vector<std::uint8_t> oversized{0xff, 0xff};
        Net::Reader long_string(oversized);
        CHECK(long_string.text(32).empty() && !long_string.good());
        std::mt19937 rng(7);
        for (unsigned i = 0; i < 10000; ++i)
        {
            std::vector<std::uint8_t> bytes(rng() % 256);
            for (auto& b : bytes)
                b = std::uint8_t(rng());
            Net::Reader probe(bytes);
            probe.u64();
            probe.text(32);
            probe.f32();
            probe.raw(rng());
            CHECK(probe.remaining() <= bytes.size());
        }
        return true;
    }
    bool clock_test()
    {
        ServerClock timer;
        unsigned ticks = 999;
        for (unsigned i = 0; i < 120; ++i)
        {
            CHECK(timer.advance(1.0 / 120, ticks));
            CHECK(ticks == i % 2);
        }
        CHECK(timer.discarded_ticks() == 0);
        CHECK(timer.advance(1, ticks) && ticks == 6 && timer.discarded_ticks() == 54);
        CHECK(timer.advance(0, ticks) && ticks == 0);
        const auto remainder = timer.remainder();
        for (double bad :
             {-1.0, 86401.0, std::numeric_limits<double>::infinity(), std::numeric_limits<double>::quiet_NaN()})
        {
            ticks = 123;
            CHECK(!timer.advance(bad, ticks) && ticks == 123);
            CHECK(timer.remainder() == remainder && timer.discarded_ticks() == 54);
        }
        return true;
    }
    // Synthetic REZ v1 with a three-byte payload, no commercial data.
    std::vector<std::uint8_t> archive()
    {
        std::vector<std::uint8_t> bytes(171);
        bytes[0] = bytes[62] = bytes[124] = 13;
        bytes[1] = bytes[63] = bytes[125] = 10;
        bytes[126] = 26;
        bytes[168] = 1;
        bytes[169] = 2;
        bytes[170] = 3;
        Net::Writer dir(128);
        dir.u32(0);
        dir.u32(168);
        dir.u32(3);
        dir.u32(0);
        dir.u32(1);
        dir.u32(0x444154);
        dir.u32(0);
        const std::uint8_t name[]{'t', 'e', 's', 't', 0, 0};
        dir.raw(name);
        bytes.insert(bytes.end(), dir.bytes().begin(), dir.bytes().end());
        auto patch = [&](unsigned at, unsigned v)
        {
            for (unsigned i = 0; i < 4; ++i)
                bytes[at + i] = std::uint8_t(v >> (i * 8));
        };
        patch(127, 1);
        patch(131, 171);
        patch(135, unsigned(dir.bytes().size()));
        return bytes;
    }
    bool save(const std::filesystem::path& path, const std::vector<std::uint8_t>& bytes)
    {
        std::ofstream file(path, std::ios::binary);
        return bool(file.write(reinterpret_cast<const char*>(bytes.data()), std::streamsize(bytes.size())));
    }
    bool content(const std::filesystem::path& root)
    {
        std::error_code ec;
        std::filesystem::create_directories(root / "second", ec);
        CHECK(!ec);
        const auto bytes = archive();
        CHECK(save(root / "BLOOD2.REZ", bytes) && save(root / "second" / "blood2.rez", bytes));
        Resources a, b;
        std::string error;
        CHECK(a.mount(root / "BLOOD2.REZ", error) && b.mount(root / "second" / "blood2.rez", error));
        Sha256Digest first{}, second{};
        CHECK(Net::content_fingerprint(a, first, error) && Net::content_fingerprint(b, second, error) &&
              first == second);
        auto changed = bytes;
        changed[169] ^= 7;
        CHECK(save(root / "second" / "blood2.rez", changed));
        CHECK(Net::content_fingerprint(b, second, error) && first != second);
        // Same directory digest and length cannot hide an altered payload.
        CHECK(a.mounts()[0].directory_digest == b.mounts()[0].directory_digest);
        changed.push_back(0);
        CHECK(save(root / "second" / "blood2.rez", changed));
        const auto unchanged = second;
        CHECK(!Net::content_fingerprint(b, second, error) && second == unchanged);
        a.set_demo(true);
        CHECK(!Net::content_fingerprint(a, second, error));
        a.set_demo(false);
        a.set_nightmares(true);
        CHECK(!Net::content_fingerprint(a, second, error));
        a.set_nightmares(false);
        Resources mod;
        CHECK(mod.mount(root / "BLOOD2.REZ", error, MountRole::Mod));
        CHECK(!Net::content_fingerprint(mod, second, error));
        return true;
    }
} // namespace
int main(int argc, char** argv)
{
    if (argc < 2)
        return 1;
    const std::string_view name = argv[1];
    return (name == "codec"                  ? codec()
            : name == "clock"                ? clock_test()
            : name == "content" && argc == 3 ? content(argv[2])
                                             : false)
               ? 0
               : 1;
}
