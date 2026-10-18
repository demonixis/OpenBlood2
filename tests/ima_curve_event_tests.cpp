#include "engine/ima_curve_event.h"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
using namespace OpenBlood2;
#define CHECK(x)                                                                                                       \
    do                                                                                                                 \
    {                                                                                                                  \
        if (!(x))                                                                                                      \
        {                                                                                                              \
            std::cerr << __LINE__ << ": " #x " | " << error << '\n';                                                   \
            return false;                                                                                              \
        }                                                                                                              \
    } while (false)
namespace
{
    void byte(std::uint64_t& hash, std::uint8_t value)
    {
        hash = (hash ^ value) * 1099511628211ull;
    }
    void word(std::uint64_t& hash, std::uint32_t value)
    {
        for (unsigned i = 0; i < 4; ++i)
            byte(hash, static_cast<std::uint8_t>(value >> (8 * i)));
    }
    void aggregate(std::uint64_t& hash, const std::vector<ImaTimedMidiMessage>& messages)
    {
        word(hash, messages.size());
        for (const auto& m : messages)
        {
            word(hash, static_cast<std::uint32_t>(m.time));
            byte(hash, m.status);
            byte(hash, m.key);
            byte(hash, m.velocity);
        }
    }
    bool original()
    {
        std::string error;
        std::uint64_t hash = 14695981039346656037ull;
        std::size_t cases = 0, count = 0;
        const std::array<std::array<int, 4>, 9> points{{{-1, 1513, 798, 8192},
                                                        {0, 0, 192, 127},
                                                        {100, 40, 101, 80},
                                                        {20, 40, 20, 80},
                                                        {100, 40, -30, 80},
                                                        {0, 50, 192, 50},
                                                        {-32768, 32767, 32767, -32768},
                                                        {32767, -32768, -32768, 32767},
                                                        {-100, -200, 100, 0}}};
        for (auto type : {3, 4, 5, 6})
            for (auto shape : {0, 1, 2, 3, 4, 255})
                for (auto flags : {0, 1, 2, 3, 128, 255})
                    for (auto p : points)
                    {
                        ImaCurve c;
                        c.type = type;
                        c.channel = (cases * 7) % 16;
                        c.controller = (cases * 19) % 256;
                        c.points = {{std::uint8_t(shape), std::uint8_t(flags), std::int16_t(p[0]), std::int16_t(p[1]),
                                     std::int16_t(p[2]), std::int16_t(p[3])}};
                        std::vector<ImaTimedMidiMessage> messages;
                        CHECK(prepare_ima_curve_events(c, 10000, messages, error));
                        aggregate(hash, messages);
                        count += messages.size();
                        ++cases;
                    }
        std::cout << cases << " original curve cases / " << count << " messages / FNV64 " << hash << '\n';
        CHECK(cases == 1296 && count == 39840 && hash == 11599925503900038861ull);
        return true;
    }
    bool bounds()
    {
        std::string error;
        ImaCurve c;
        c.type = 4;
        c.channel = 15;
        c.controller = 7;
        c.points = {{0, 0, 0, 10, 192, 100}};
        std::vector<ImaTimedMidiMessage> out;
        CHECK(prepare_ima_curve_events(c, 10000, out, error));
        auto owned = out;
        CHECK(!owned.empty() && owned.front().time == 10000 && owned.front().status == 0xb3);
        c.points.clear();
        CHECK(prepare_ima_curve_events(c, 0, out, error, 0) && out.empty() && !owned.empty());
        c.points = {{1, 0, 1, 10, 100, 80}, {1, 0, -1, 10, 100, 40}};
        c.time_offset = 32000; // Helper uses explicit caller tick, not a second header offset.
        CHECK(prepare_ima_curve_events(c, 0, out, error, 2) && out.size() == 2 && out[0].time == 1 &&
              out[1].time == -1);
        CHECK(!prepare_ima_curve_events(c, 0, out, error, 1) && out.empty());
        CHECK(!prepare_ima_curve_events(c, std::numeric_limits<std::int32_t>::max(), out, error) && out.empty());
        CHECK(!prepare_ima_curve_events(c, std::numeric_limits<std::int32_t>::min(), out, error) && out.empty());
        c.channel = 16;
        CHECK(!prepare_ima_curve_events(c, 0, out, error) && out.empty());
        c.channel = 0;
        c.type = 2;
        CHECK(!prepare_ima_curve_events(c, 0, out, error) && out.empty());
        c.type = 3;
        c.points = {{0, 0, 20, 40, 20, 80}};
        CHECK(prepare_ima_curve_events(c, 0, out, error, 1) && out.size() == 1 && out.front().key == 80);
        c.points.assign(4097, {1, 0, 0, 0, 0, 1});
        CHECK(!prepare_ima_curve_events(c, 0, out, error) && out.empty());
        c.points.assign(400, {0, 0, 0, 0, 19200, 19200});
        CHECK(!prepare_ima_curve_events(c, 0, out, error, 100000) && out.empty());
        return true;
    }
    bool private_data(const std::filesystem::path& root)
    {
        std::string error;
        std::vector<std::filesystem::path> paths;
        for (const auto& f : std::filesystem::recursive_directory_iterator(root / "Music"))
            if (f.path().extension() == ".sty")
                paths.push_back(f.path());
        std::sort(paths.begin(), paths.end());
        std::uint64_t hash = 14695981039346656037ull;
        std::size_t points = 0, count = 0;
        for (const auto& path : paths)
        {
            std::ifstream f(path, std::ios::binary);
            std::vector<std::uint8_t> data{std::istreambuf_iterator<char>(f), {}};
            ImaCurves curves;
            CHECK(read_ima_curves(data, curves, error));
            for (auto c : curves.curves)
                for (auto p : c.points)
                {
                    auto one = c;
                    one.points = {p};
                    std::vector<ImaTimedMidiMessage> messages;
                    CHECK(prepare_ima_curve_events(one, 10000, messages, error));
                    aggregate(hash, messages);
                    ++points;
                    count += messages.size();
                }
        }
        std::cout << points << " installed points / " << count << " messages / FNV64 " << hash << '\n';
        CHECK(paths.size() == 39 && points == 47 && count == 3194 && hash == 12221266369440151417ull);
        return true;
    }
} // namespace
int main(int argc, char** argv)
{
    if (argc < 2)
        return 2;
    const std::string_view mode = argv[1];
    return (mode == "original"               ? original()
            : mode == "bounds"               ? bounds()
            : mode == "private" && argc == 3 ? private_data(argv[2])
                                             : false)
               ? 0
               : 1;
}
