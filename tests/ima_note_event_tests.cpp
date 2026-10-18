#include "engine/ima_note_event.h"
#include <algorithm>
#include <bit>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
using namespace OpenBlood2;
using Bytes = std::vector<std::uint8_t>;
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
    void word(Bytes& b, std::uint32_t v, unsigned n = 4)
    {
        for (unsigned i = 0; i < n; ++i)
            b.push_back(static_cast<std::uint8_t>(v >> (i * 8)));
    }
    Bytes encode(const ImaNoteEvents& events)
    {
        Bytes b{static_cast<std::uint8_t>(events.emitted), events.draws_used};
        if (events.emitted)
            for (const auto& e : events.messages)
            {
                word(b, static_cast<std::uint32_t>(e.time));
                b.push_back(e.status);
                b.push_back(e.key);
                b.push_back(e.velocity);
            }
        return b;
    }
    std::string hex(const Bytes& bytes)
    {
        const char* digits = "0123456789abcdef";
        std::string s;
        for (auto b : bytes)
        {
            s += digits[b >> 4];
            s += digits[b & 15];
        }
        return s;
    }
    std::array<std::uint8_t, 128> map()
    {
        std::array<std::uint8_t, 128> out{};
        for (unsigned i = 0; i < 128; ++i)
            out[i] = (i * 7 + 13) & 127;
        return out;
    }
    ImaNote fixture(std::array<std::uint8_t, 3> ranges = {})
    {
        ImaNote n;
        n.type = 9;
        n.channel = 0;
        n.time_offset = -24;
        n.duration = 192;
        n.velocity = 100;
        n.time_range = ranges[0];
        n.duration_range = ranges[1];
        n.velocity_range = ranges[2];
        return n;
    }
    constexpr std::array<std::uint16_t, 3> draws{32767, 123, 7654};
    std::span<const std::uint16_t> random_values(const ImaNote& n, std::int16_t pitch)
    {
        return std::span(draws).first(pitch ? unsigned(n.time_range != 0) + unsigned(n.duration_range != 0) +
                                                  unsigned(n.velocity_range != 0)
                                            : 0);
    }
    bool behavior()
    {
        std::string error;
        ImaNoteEvents out;
        // Canonical results from the isolated original executable; synthetic inputs only.
        const std::array<std::string_view, 16> expected{
            "0100d003000094306490040000843064", "0101d003000094306490040000843064", "0101d003000094306490040000843064",
            "0101d003000094302690040000843026", "0103d103000094306592040000843065", "01038f0400009430650a060000843065",
            "0102d103000094307f9104000084307f", "0103920300009430478e040000843047", "0000",
            "0103a7000000937801a8000000837801", "0103a7000000930b01a8000000830b01", "0103a7000000930501a8000000830501",
            "0103a7000000937a01a8000000837a01", "0103a7000000930a01a8000000830a01", "0100d003000094486490040000844864",
            "0100d0030000993d6490040000893d64"};
        unsigned i = 0;
        for (auto ranges : std::array<std::array<std::uint8_t, 3>, 8>{{{0, 0, 0},
                                                                       {1, 0, 0},
                                                                       {0, 3, 0},
                                                                       {0, 0, 127},
                                                                       {4, 5, 6},
                                                                       {128, 129, 255},
                                                                       {255, 0, 128},
                                                                       {127, 127, 127}}})
        {
            const auto n = fixture(ranges);
            CHECK(prepare_ima_note_events(n, {1000, 60, -12, {}}, random_values(n, 60), out, error));
            CHECK(hex(encode(out)) == expected[i++]);
        }
        for (auto pair :
             std::array<std::array<int, 2>, 6>{{{0, 12}, {-32768, -128}, {-1, 0}, {1, -128}, {127, 127}, {32767, 127}}})
        {
            auto n = fixture({128, 255, 127});
            n.channel = 15;
            n.velocity = 255;
            n.duration = -32768;
            const auto pitch = static_cast<std::int16_t>(pair[0]);
            const auto transpose = static_cast<std::int8_t>(pair[1]);
            CHECK(prepare_ima_note_events(n, {0, pitch, transpose, {}}, random_values(n, pitch), out, error));
            CHECK(hex(encode(out)) == expected[i++]);
        }
        auto mapping = map();
        for (unsigned channel : {0, 5})
        {
            auto n = fixture();
            n.channel = channel;
            CHECK(prepare_ima_note_events(n, {1000, 60, 12, mapping}, {}, out, error));
            CHECK(hex(encode(out)) == expected[i++]);
        }
        const auto retained = encode(out);
        mapping.fill(0);
        CHECK(encode(out) == retained); // No retained map span.
        auto n = fixture();
        n.type = 8;
        CHECK(prepare_ima_note_events(n, {0, 60, 0, {}}, {}, out, error));
        CHECK(out.messages[0].time == -24 && out.messages[0].status == 0x94 && out.messages[1].status == 0x84 &&
              out.messages[1].velocity == 100);
        n.type = 9;
        n.channel = 5;
        mapping[60] = 0;
        CHECK(prepare_ima_note_events(n, {0, 60, 0, mapping}, {}, out, error) &&
              out.messages[0].key == 12); // Mapping zero differs from pre-map zero.
        n.time_range = 1;
        CHECK(prepare_ima_note_events(n, {0, 60, 0, {}}, std::span(draws).first(1), out, error) &&
              out.draws_used == 1 && out.messages[0].time == -24);
        n.time_range = 0;
        n.velocity = 0;
        CHECK(prepare_ima_note_events(n, {0, 60, 0, {}}, {}, out, error) && out.messages[0].velocity == 1);
        n.velocity = 127;
        n.velocity_range = 128; // Original signed -128 increases rather than centering around zero.
        CHECK(prepare_ima_note_events(n, {0, 60, 0, {}}, std::span(draws).first(1), out, error) &&
              out.messages[0].velocity == 127);
        std::cout << "Original event tail: signed ranges/velocity, exact draw order, suppression, octave folding, "
                     "remap and paired routing\n";
        return true;
    }
    bool bounds()
    {
        std::string error;
        ImaNoteEvents out;
        auto n = fixture();
        ImaNoteEventContext context{0, 60, 0, {}};
        auto rejected = [&](std::span<const std::uint16_t> random = std::span<const std::uint16_t>{})
        {
            out.emitted = true;
            out.draws_used = 3;
            out.messages[0].key = 99;
            const bool ok = prepare_ima_note_events(n, context, random, out, error);
            return !ok && !error.empty() && encode(out) == Bytes({0, 0}) && out.messages[0].key == 0 &&
                   out.messages[1].time == 0;
        };
        for (auto type : {0, 7, 10, 255})
        {
            n.type = type;
            CHECK(rejected());
        }
        n.type = 9;
        for (auto channel : {16, 255})
        {
            n.channel = channel;
            CHECK(rejected());
        }
        n.channel = 0;
        auto mapping = map();
        context.percussion_map = std::span(mapping).first(127);
        CHECK(rejected());
        context.percussion_map = mapping;
        n.channel = 5;
        for (auto pitch : {-1, 128, 32767})
        {
            context.resolved_pitch = pitch;
            CHECK(rejected());
        }
        context.resolved_pitch = 0;
        CHECK(prepare_ima_note_events(n, context, {}, out, error) && !out.emitted);
        context = {0, 60, 0, {}};
        n = fixture({1, 2, 3});
        CHECK(rejected(std::span(draws).first(2)));
        const std::array<std::uint16_t, 3> bad{0, 32768, 0};
        CHECK(rejected(bad));
        context.resolved_pitch = 0;
        CHECK(rejected(draws));
        CHECK(prepare_ima_note_events(n, context, {}, out, error) && error.empty());
        context = {std::numeric_limits<std::int32_t>::max(), 60, 0, {}};
        n = fixture();
        n.time_offset = 32767;
        CHECK(rejected());
        n.time_offset = 0;
        n.duration = 1;
        CHECK(rejected());
        --context.grid_time;
        CHECK(prepare_ima_note_events(n, context, {}, out, error) &&
              out.messages[1].time == std::numeric_limits<std::int32_t>::max());
        context.grid_time = std::numeric_limits<std::int32_t>::min();
        n.time_offset = -1;
        CHECK(rejected());
        n.time_offset = 0;
        CHECK(prepare_ima_note_events(n, context, {}, out, error) &&
              out.messages[0].time == std::numeric_limits<std::int32_t>::min());
        std::cout
            << "Unsupported type/channel/map, unsafe map index, RNG count/range, tick overflow and atomic clearing\n";
        return true;
    }
    bool private_data(const std::filesystem::path& root)
    {
        std::string error;
        std::vector<std::filesystem::path> files;
        for (const auto& f : std::filesystem::recursive_directory_iterator(root / "Music"))
            if (f.path().extension() == ".sty")
                files.push_back(f.path());
        std::sort(files.begin(), files.end());
        const auto mapping = map();
        std::uint64_t hash = 14695981039346656037ull;
        std::size_t count = 0, events = 0;
        for (const auto& f : files)
        {
            std::ifstream in(f, std::ios::binary);
            Bytes data{std::istreambuf_iterator<char>(in), {}};
            ImaStyle style;
            CHECK(read_ima_style(data, style, error));
            for (const auto& p : style.patterns)
                for (const auto& g : p.grids)
                    for (const auto& n : g.notes)
                    {
                        // Explicit raw-pitch branch and synthetic timing/map, not the authored song's resolved
                        // pitch/time.
                        const auto pitch = std::bit_cast<std::int8_t>(n.midi_note);
                        for (unsigned scenario = 0; scenario < 2; ++scenario)
                        {
                            ImaNoteEventContext context{
                                scenario ? 12345 : 10000, pitch, static_cast<std::int8_t>(scenario ? -17 : 0),
                                scenario ? std::span(mapping) : std::span<const std::uint8_t>{}};
                            ImaNoteEvents out;
                            CHECK(prepare_ima_note_events(n, context, random_values(n, pitch), out, error));
                            events += out.emitted ? 2 : 0;
                            for (auto b : encode(out))
                            {
                                hash ^= b;
                                hash *= 1099511628211ull;
                            }
                        }
                        ++count;
                    }
        }
        std::cout << "Private original tail equivalence: " << count << " notes / " << events
                  << " event messages; FNV64 " << hash << '\n';
        CHECK(files.size() == 39 && count == 7030 && events == 28120 && hash == 7927161452939007316ull);
        return true;
    }
} // namespace
int main(int argc, char** argv)
{
    if (argc < 2)
        return 2;
    const std::string_view test = argv[1];
    return (test == "behavior"               ? behavior()
            : test == "bounds"               ? bounds()
            : test == "private" && argc == 3 ? private_data(argv[2])
                                             : false)
               ? 0
               : 1;
}
