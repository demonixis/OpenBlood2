#include "engine/ima_note_event.h"
#include "engine/ima_note_pitch.h"
#include <algorithm>
#include <bit>
#include <filesystem>
#include <fstream>
#include <iostream>
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
    struct NoteInput
    {
        std::uint16_t music;
        std::uint8_t modifier, raw, mode, channel;
    };
    struct ChannelInput
    {
        std::uint8_t mode;
        bool limited;
        std::int8_t lower, upper;
    };
    struct Fixture
    {
        NoteInput note;
        ChannelInput channel;
        ImaHarmonicContext harmony;
        bool ok;
        std::int16_t pitch;
    };
    ImaNote note(NoteInput input)
    {
        ImaNote n;
        n.type = 9;
        n.music_value = input.music;
        n.pitch_modifier = input.modifier;
        n.midi_note = input.raw;
        n.play_mode = input.mode;
        n.channel = input.channel;
        n.velocity = 100;
        n.duration = 192;
        return n;
    }
    ImaPattern pattern(ChannelInput input, unsigned channel)
    {
        ImaPattern p;
        p.play_modes[channel] = input.mode;
        p.lower_pitch[channel] = input.lower;
        p.upper_pitch[channel] = input.upper;
        p.pitch_range_channels = input.limited ? (1u << channel) : 0;
        return p;
    }
    void hash_result(std::uint64_t& hash, bool ok, std::int16_t pitch)
    {
        for (auto b : {std::uint8_t(ok), static_cast<std::uint8_t>(pitch),
                       static_cast<std::uint8_t>(static_cast<std::uint16_t>(pitch) >> 8)})
        {
            hash ^= b;
            hash *= 1099511628211ull;
        }
    }
    bool behavior()
    {
        std::string error;
        // Isolated executable observations, including two nonterminating original walks.
        const Fixture fixtures[]{
            {{16681, 37, 60, 128, 0}, {3, false, 0, 127}, {true, 145, 2741, 0, 0, 0}, true, 36},
            {{16681, 37, 60, 129, 0}, {3, false, 0, 127}, {true, 145, 2741, 0, 0, 0}, true, 48},
            {{16681, 37, 60, 130, 0}, {3, false, 0, 127}, {true, 145, 2741, 0, 0, 0}, true, 36},
            {{16681, 37, 60, 131, 0}, {3, false, 0, 127}, {true, 145, 2741, 0, 0, 0}, true, 36},
            {{16681, 37, 60, 132, 0}, {3, false, 0, 127}, {true, 145, 2741, 0, 0, 0}, true, 57},
            {{16681, 37, 60, 133, 0}, {3, false, 0, 127}, {true, 145, 2741, 0, 0, 0}, true, 36},
            {{16681, 37, 60, 134, 0}, {3, false, 0, 127}, {true, 145, 2741, 0, 0, 0}, true, 36},
            {{16681, 37, 60, 135, 0}, {3, false, 0, 127}, {true, 145, 2741, 0, 0, 0}, true, 36},
            {{16681, 37, 60, 136, 0}, {3, false, 0, 127}, {true, 145, 2741, 0, 0, 0}, true, 36},
            {{16681, 37, 60, 137, 0}, {3, false, 0, 127}, {true, 145, 2741, 0, 0, 0}, true, 36},
            {{16681, 37, 60, 138, 0}, {3, false, 0, 127}, {true, 145, 2741, 0, 0, 0}, true, 36},
            {{16681, 37, 60, 139, 0}, {3, false, 0, 127}, {true, 145, 2741, 0, 0, 0}, true, 36},
            {{16681, 37, 60, 140, 0}, {3, false, 0, 127}, {true, 145, 2741, 0, 0, 0}, true, 36},
            {{16681, 37, 60, 141, 0}, {3, false, 0, 127}, {true, 145, 2741, 0, 0, 0}, true, 36},
            {{16681, 37, 60, 142, 0}, {3, false, 0, 127}, {true, 145, 2741, 0, 0, 0}, true, 36},
            {{16681, 37, 60, 143, 0}, {3, false, 0, 127}, {true, 145, 2741, 0, 0, 0}, true, 36},
            {{16920, 159, 60, 131, 0}, {3, true, 48, 72}, {true, 145, 2741, -7, 11, 16}, true, 59},
            {{16920, 159, 60, 131, 0}, {3, true, 48, 72}, {true, 145, 2741, -7, 11, 39}, true, 62},
            {{16920, 159, 60, 131, 0}, {3, true, 48, 72}, {true, 145, 2741, -7, 11, 71}, true, 50},
            {{16920, 159, 60, 131, 0}, {3, true, 48, 72}, {true, 145, 2741, -7, 11, 87}, true, 50},
            {{16920, 159, 60, 131, 0}, {3, true, 48, 72}, {true, 145, 2741, -7, 11, 119}, true, 50},
            {{16640, 0, 60, 131, 0}, {3, false, 0, 127}, {true, 1, 2741, 0, 0, 0}, true, 0},
            {{16640, 159, 60, 131, 0}, {3, false, 0, 127}, {true, 145, 2741, 0, 0, 0}, true, 40},
            {{16640, 159, 60, 131, 0}, {3, false, 0, 127}, {true, 1, 2147483648u, -128, 127, 0}, true, 202},
            {{16384, 128, 60, 132, 0}, {3, false, 0, 127}, {true, 1, 2147483648u, 0, 0, 0}, true, 70},
            {{16384, 248, 60, 132, 0}, {3, false, 0, 127}, {true, 1, 4294967295u, 0, 0, 0}, true, 55},
            {{16384, 16, 60, 132, 0}, {3, false, 0, 127}, {true, 1, 0, 0, 0, 0}, false, 0},
            {{16384, 128, 60, 132, 0}, {3, false, 0, 127}, {true, 145, 2741, 0, 0, 0}, false, 0},
            {{0, 0, 255, 131, 0}, {3, false, 0, 127}, {true, 145, 2741, 0, 0, 0}, true, -1},
            {{16384, 0, 255, 131, 0}, {3, false, 0, 127}, {false, 0, 0, 0, 0, 0}, true, -1},
            {{16384, 0, 12, 129, 0}, {3, false, 0, 127}, {true, 145, 2741, 0, 0, 0}, true, 0},
            {{16384, 0, 128, 129, 0}, {3, false, 0, 127}, {true, 145, 2741, 0, 0, 0}, true, -140},
            {{16384, 0, 90, 129, 0}, {1, true, 100, 20}, {true, 0, 0, 0, 0, 16}, true, 102},
        };
        for (const auto& f : fixtures)
        {
            const auto n = note(f.note);
            const auto p = pattern(f.channel, n.channel);
            ImaNotePitch out{123, 255, true};
            const bool ok = resolve_ima_note_pitch(n, p, f.harmony, out, error);
            CHECK(ok == f.ok && out.pitch == f.pitch);
            CHECK(ok ? error.empty() : !error.empty());
            CHECK(out.converted == (ok && f.harmony.enabled && n.music_value != 0));
            if (!out.converted)
                CHECK(out.mode == 0);
            if (!ok)
                continue;
            ImaNoteEvents events;
            CHECK(prepare_ima_note_events(n, {1000, out.pitch, -12, {}}, {}, events, error));
            CHECK(events.emitted == (out.pitch != 0));
            if (events.emitted)
                CHECK(events.messages[0].key >= 1 && events.messages[0].key <= 127 &&
                      events.messages[1].time - events.messages[0].time == 192);
        }
        return true;
    }
    bool generated()
    {
        std::string error;
        std::uint32_t seed = 0x6d2b79f5;
        const auto draw = [&]
        {
            seed = seed * 1664525u + 1013904223u;
            return seed;
        };
        const auto signed_byte = [](std::uint32_t v)
        { return std::bit_cast<std::int8_t>(static_cast<std::uint8_t>(v)); };
        const std::uint32_t masks[]{0, 1, 0x91, 0xab5, 0xfff, 0x80000000, 0xffffffff, 0x80100001};
        std::uint64_t hash = 14695981039346656037ull;
        unsigned refused = 0;
        for (unsigned i = 0; i < 2048; ++i)
        {
            const auto ch = static_cast<std::uint8_t>(i % 16);
            const auto music = static_cast<std::uint16_t>(draw());
            const auto modifier = static_cast<std::uint8_t>(draw() & 127);
            const auto raw = static_cast<std::uint8_t>(draw());
            const auto mode = static_cast<std::uint8_t>((i % 16) | ((i & 1) ? 0x80 : 0));
            auto pm = static_cast<std::uint8_t>((i / 16) % 16);
            if (!pm)
                pm = ch == 5 ? 1 : ch == 4 ? 2 : 3;
            const auto lower = signed_byte(draw() >> 24), upper = signed_byte(draw() >> 24);
            const auto cr = signed_byte(draw() >> 24), sr = signed_byte(draw() >> 24);
            const auto flags = static_cast<std::uint8_t>(draw() >> 24);
            const auto n = note({music, modifier, raw, mode, ch});
            const auto p = pattern({pm, bool(i & 2), lower, upper}, ch);
            ImaNotePitch out{123, 255, true};
            const bool ok = resolve_ima_note_pitch(
                n, p, {bool(i % 11), masks[(i / 8) % 8], masks[(i / 64) % 8], cr, sr, flags}, out, error);
            if (!ok)
            {
                ++refused;
                CHECK(out.pitch == 0 && out.mode == 0 && !out.converted && !error.empty());
            }
            hash_result(hash, ok, out.pitch);
        }
        std::cout << "Generated original pitch observations: FNV64 " << hash << "; refused " << refused << '\n';
        CHECK(hash == 15763711143549854140ull && refused == 25);
        return true;
    }
    bool bounds()
    {
        std::string error;
        auto n = note({0x4000, 0x10, 60, 0x84, 0});
        auto p = pattern({3, false, 0, 127}, 0);
        ImaNotePitch out{60, 3, true};
        const ImaHarmonicContext context{true, 1, 0, 0, 0, 0};
        CHECK(!resolve_ima_note_pitch(n, p, context, out, error) && out.pitch == 0 && !out.converted && out.mode == 0);
        n.music_value = 0;
        CHECK(resolve_ima_note_pitch(n, p, context, out, error) && out.pitch == 60 && !out.converted && error.empty());
        for (auto bad : {0, 7, 10, 255})
        {
            n.type = static_cast<std::uint8_t>(bad);
            CHECK(!resolve_ima_note_pitch(n, p, context, out, error) && out.pitch == 0);
        }
        for (auto ch : {16, 255})
        {
            n.type = 9;
            n.channel = static_cast<std::uint8_t>(ch);
            CHECK(!resolve_ima_note_pitch(n, p, context, out, error) && out.pitch == 0);
        }
        // Loader defaults, explicit low-nibble 7 inheritance, and raw suppression.
        n = note({0x4000, 0, 90, 0x87, 5});
        p = {};
        CHECK(resolve_ima_note_pitch(n, p, {true, 0x91, 0xab5, 0, 0, 0}, out, error) && out.pitch == 78 &&
              out.mode == 1);
        n.channel = 4;
        CHECK(resolve_ima_note_pitch(n, p, {true, 0x91, 0xab5, 0, 0, 0}, out, error) && out.mode == 2);
        n.channel = 3;
        n.type = 8;
        CHECK(resolve_ima_note_pitch(n, p, {true, 0x91, 0xab5, 0, 0, 0}, out, error) && out.mode == 3);
        n.midi_note = 0;
        CHECK(resolve_ima_note_pitch(n, p, {}, out, error) && !out.converted && out.pitch == 0);
        return true;
    }
    bool private_data(const std::filesystem::path& root)
    {
        std::string error;
        std::vector<std::filesystem::path> files;
        for (const auto& f : std::filesystem::recursive_directory_iterator(root / "Music"))
            if (f.is_regular_file() && f.path().extension() == ".sty")
                files.push_back(f.path());
        std::sort(files.begin(), files.end());
        std::uint64_t hash = 14695981039346656037ull;
        std::size_t count = 0, emitted = 0;
        const ImaHarmonicContext contexts[]{{true, 0x91, 0xab5, 0, 0, 0}, {true, 0x889, 0xad5, 5, -7, 0x57}, {}};
        for (const auto& f : files)
        {
            std::ifstream in(f, std::ios::binary);
            std::vector<std::uint8_t> data{std::istreambuf_iterator<char>(in), {}};
            ImaStyle style;
            CHECK(read_ima_style(data, style, error));
            for (const auto& p : style.patterns)
                for (const auto& g : p.grids)
                    for (const auto& n : g.notes)
                    {
                        for (const auto& context : contexts)
                        {
                            ImaNotePitch out;
                            CHECK(resolve_ima_note_pitch(n, p, context, out, error));
                            hash_result(hash, true, out.pitch);
                            std::array<std::uint16_t, 3> draws{};
                            const auto required = out.pitch
                                                      ? unsigned(n.time_range != 0) + unsigned(n.duration_range != 0) +
                                                            unsigned(n.velocity_range != 0)
                                                      : 0;
                            ImaNoteEvents events;
                            CHECK(prepare_ima_note_events(n, {10000, out.pitch, 0, {}},
                                                          std::span(draws).first(required), events, error));
                            CHECK(events.emitted == (out.pitch != 0));
                            emitted += events.emitted;
                        }
                        ++count;
                    }
        }
        std::cout << "Private explicit harmony: " << count << " notes; FNV64 " << hash << "; pairs " << emitted << '\n';
        CHECK(files.size() == 39 && count == 7030 && hash == 809768487747410926ull);
        return true;
    }
} // namespace
int main(int argc, char** argv)
{
    if (argc < 2)
        return 2;
    const std::string_view t = argv[1];
    return (t == "behavior"               ? behavior()
            : t == "generated"            ? generated()
            : t == "bounds"               ? bounds()
            : t == "private" && argc == 3 ? private_data(argv[2])
                                          : false)
               ? 0
               : 1;
}
