#include "engine/ima_selection.h"
#include <algorithm>
#include <bit>
#include <cctype>
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
    ImaSection section()
    {
        ImaSection s;
        s.measures = 16;
        s.ticks_per_measure = 768;
        s.ticks_per_beat = 192;
        s.chords.resize(1);
        return s;
    }
    ImaStyle style()
    {
        ImaStyle s;
        s.beats_per_measure = 4;
        s.beat_unit = 4;
        return s;
    }
    void command(ImaSection& s, std::int32_t time, std::uint32_t flags)
    {
        ImaSectionCommand c;
        c.time = time;
        for (unsigned i = 0; i < 4; ++i)
            c.value[i] = std::uint8_t(flags >> (8 * i));
        s.commands.push_back(c);
    }
    void chord(ImaSection& s, int measure, int beat)
    {
        ImaSectionChord c;
        c.measure = static_cast<std::int16_t>(measure);
        c.beat = static_cast<std::int8_t>(beat);
        s.chords.push_back(c);
    }
    void pattern(ImaStyle& s, std::uint32_t flags, int measures)
    {
        ImaPattern p;
        p.flags = flags;
        p.timing[3] = static_cast<std::int16_t>(measures);
        s.patterns.push_back(p);
    }
    bool sample(const ImaSection& s, std::int32_t tick, ImaSectionSelection& out, std::string& error)
    {
        ImaTimeline t;
        return prepare_ima_timeline(s, style(), t, error) && select_ima_section(s, t, tick, out, error);
    }
    bool behavior()
    {
        std::string error;
        auto s = section();
        ImaSectionSelection out;
        CHECK(sample(s, 0, out, error) && out.measure == 0 && out.chord == 0 && out.mute == 0 && out.mute_mask == 0 &&
              out.request.measures == 12 && out.request.flags == 0x204);
        command(s, 0, 0x422);
        command(s, 0, 0x801);
        command(s, 768 * 4, 0x10000);
        CHECK(sample(s, 0, out, error) && out.request.flags == 0x426 &&
              out.request.measures == 0); // First equal, next duplicate boundary.
        CHECK(sample(s, 1, out, error) && out.request.flags == 0x804 && out.request.measures == 4);
        CHECK(sample(s, 768 * 4, out, error) && out.request.flags == 0x10004 && out.request.measures == 12);
        s.commands.clear();
        command(s, 768 * 20, 0xffffffff);
        CHECK(sample(s, 0, out, error) && out.request.flags == 0x204 && out.request.measures == 12);
        CHECK(sample(s, 768 * 17, out, error) &&
              out.request.measures == 3); // Future command replaces even negative remaining length.
        s.commands.clear();
        CHECK(sample(s, 768 * 17, out, error) && out.request.measures == -1);
        s.chords[0].measure = 5;
        chord(s, 6, 0);
        chord(s, 0, 0);
        CHECK(sample(s, 0, out, error) && out.chord == 0 &&
              out.request.measures == 5); // First future chord, not sorted.
        CHECK(sample(s, 6 * 768, out, error) && out.chord == 2);
        s = section();
        s.chords[0].beat = 1;
        CHECK(sample(s, 0, out, error) && out.request.measures == 1 && out.request.flags == 0x210);
        s.chords[0].beat = 2;
        CHECK(sample(s, 0, out, error) && out.request.measures == 12 && out.request.flags == 0x208);
        chord(s, 2, 0);
        CHECK(sample(s, 0, out, error) && out.request.measures == 1 && out.request.flags == 0x208);
        s = section();
        s.chords[0].measure = 2;
        s.chords[0].beat = 1;
        CHECK(sample(s, 0, out, error) && out.request.measures == 2 && out.request.flags == 0x204);
        s = section();
        s.mutes = {{400, 0x8001, 0}, {0, 7, 0}};
        CHECK(sample(s, -1, out, error) && out.measure == 0 && out.mute_mask == 0x8001);
        CHECK(sample(s, 576, out, error) && out.mute_mask == 7);
        CHECK(sample(s, INT32_MAX, out, error) &&
              out.measure == std::bit_cast<std::int16_t>(std::uint16_t(INT32_MAX / 768)));
        auto st = style();
        ImaPatternSelection p;
        CHECK(select_ima_pattern(st, {4, 0x204}, p, error) && !p.pattern && p.match == ImaPatternMatch::Empty);
        pattern(st, 0x224, 4);
        pattern(st, 0x204, 2);
        pattern(st, 0x204, 2);
        pattern(st, 0x204, 5);
        CHECK(select_ima_pattern(st, {4, 0x204}, p, error) && p.pattern == 1 && p.match == ImaPatternMatch::Length);
        CHECK(select_ima_pattern(st, {1, 0x204}, p, error) && p.pattern == 3 &&
              p.match == ImaPatternMatch::LastEligible);
        CHECK(select_ima_pattern(st, {0, 0x204}, p, error) && p.pattern == 0 &&
              p.match == ImaPatternMatch::HeadFallback);
        CHECK(select_ima_pattern(st, {12, 0x10404}, p, error) && p.pattern == 0 &&
              p.match == ImaPatternMatch::HeadFallback);
        CHECK(select_ima_pattern(st, {32767, 0x204}, p, error) && p.pattern == 3 && p.match == ImaPatternMatch::Length);
        st = {};
        s = {};
        CHECK(p.pattern == 3 && out.mute_mask == 7); // Results own values, no borrowed handles.
        return true;
    }
    bool bounds()
    {
        std::string error;
        auto s = section();
        ImaTimeline t;
        ImaSectionSelection out;
        CHECK(prepare_ima_timeline(s, style(), t, error));
        auto bad = t;
        bad.chords[0].source = 999;
        CHECK(!select_ima_section(s, bad, 0, out, error) && !error.empty() && !out.mute && out.request.flags == 0);
        bad = t;
        bad.chords[0].time++;
        CHECK(!select_ima_section(s, bad, 0, out, error));
        bad = t;
        bad.ticks_per_measure = 0;
        CHECK(!select_ima_section(s, bad, 0, out, error));
        bad = t;
        bad.mutes[0].source = 999;
        CHECK(!select_ima_section(s, bad, 0, out, error));
        bad = t;
        bad.mutes[1].time = -1;
        CHECK(!select_ima_section(s, bad, 0, out, error));
        command(s, 0, 0x204);
        CHECK(prepare_ima_timeline(s, style(), t, error));
        bad = t;
        bad.commands[0].source = 1;
        CHECK(!select_ima_section(s, bad, 0, out, error));
        bad = t;
        bad.commands[0].time = 1;
        CHECK(!select_ima_section(s, bad, 0, out, error));
        bad = t;
        bad.mutes.resize(8193);
        CHECK(!select_ima_section(s, bad, 0, out, error));
        s.chords.clear();
        CHECK(prepare_ima_timeline(s, style(), t, error));
        CHECK(!select_ima_section(s, t, 0, out, error) && error.find("fallback") != std::string::npos);
        s.chords.resize(4097);
        CHECK(!select_ima_section(s, t, 0, out, error));
        auto st = style();
        st.patterns.resize(4097);
        ImaPatternSelection p;
        p.pattern = 7;
        CHECK(!select_ima_pattern(st, {1, 0}, p, error) && !p.pattern && !error.empty());
        return true;
    }
    void word(std::uint64_t& hash, std::uint32_t value, int bytes)
    {
        for (int i = 0; i < bytes; ++i)
        {
            hash ^= std::uint8_t(value >> (8 * i));
            hash *= 1099511628211ull;
        }
    }
    bool observe(const ImaSection& s, const ImaStyle& st, const ImaTimeline& t, std::int32_t tick,
                 ImaPatternRequest extra, std::uint64_t& hash, std::string& error)
    {
        ImaSectionSelection a;
        ImaPatternSelection p, q;
        CHECK(select_ima_section(s, t, tick, a, error) && select_ima_pattern(st, a.request, p, error) &&
              select_ima_pattern(st, extra, q, error));
        word(hash, a.measure, 2);
        word(hash, a.chord, 2);
        word(hash, a.mute.value_or(65535), 2);
        word(hash, a.mute_mask, 2);
        word(hash, a.request.measures, 2);
        word(hash, a.request.flags, 4);
        word(hash, p.pattern ? static_cast<std::uint32_t>(*p.pattern) : 0xffffffff, 4);
        word(hash, q.pattern ? static_cast<std::uint32_t>(*q.pattern) : 0xffffffff, 4);
        return true;
    }
    bool generated()
    {
        std::string error;
        std::uint32_t seed = 0x198b2;
        const auto draw = [&]
        {
            seed = seed * 1664525u + 1013904223u;
            return seed;
        };
        const std::uint32_t flags[]{0, 4, 8, 16, 0x204, 0x208, 0x210, 0x224, 0x404, 0x10404, 0x13fff, 0xffffffff};
        std::uint64_t hash = 14695981039346656037ull;
        for (unsigned i = 0; i < 4096; ++i)
        {
            auto s = section();
            s.measures = static_cast<std::int16_t>(1 + draw() % 16);
            s.chords.clear();
            auto st = style();
            for (unsigned j = 0, n = draw() % 6; j < n; ++j)
            {
                const auto measure = int(draw() % 26) - 5;
                const auto offset = int(draw() % 2) * 17;
                const auto time = measure * 768 + offset;
                const auto f = flags[draw() % 12];
                command(s, time, f);
            }
            for (unsigned j = 0, n = 1 + draw() % 8; j < n; ++j)
            {
                const auto m = int(draw() % 23) - 4;
                const auto b = int(draw() % 6) - 1;
                chord(s, m, j ? b : 0);
            }
            for (unsigned j = 0, n = draw() % 6; j < n; ++j)
            {
                const auto time = int(draw() % 14000) - 2000;
                const auto value = std::uint16_t(draw());
                s.mutes.push_back({time, value, 0});
            }
            auto tick = int(draw() % 16000) - 2000;
            if (i % 17 == 0)
                tick = INT32_MIN;
            if (i % 19 == 0)
                tick = INT32_MAX;
            if (i % 23 == 0)
                tick = 0;
            for (unsigned j = 0, n = draw() % 12; j < n; ++j)
            {
                const auto f = flags[draw() % 12];
                const auto m = int(draw() % 18) - 1;
                pattern(st, f, m);
            }
            const auto raw = std::bit_cast<std::int16_t>(std::uint16_t(draw()));
            const auto f = draw();
            const ImaPatternRequest extra{static_cast<std::int16_t>((raw % 17 + 17) % 17 - 2), f};
            ImaTimeline t;
            CHECK(prepare_ima_timeline(s, st, t, error));
            CHECK(observe(s, st, t, tick, extra, hash, error));
        }
        std::cout << "Original section/pattern cases: 4096; FNV64 " << hash << '\n';
        CHECK(hash == 12281621980575432131ull);
        return true;
    }
    std::vector<std::uint8_t> read(const std::filesystem::path& p)
    {
        std::ifstream in(p, std::ios::binary);
        return {std::istreambuf_iterator<char>(in), {}};
    }
    std::string lower(std::string s)
    {
        for (auto& c : s)
            c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        return s;
    }
    bool private_data(const std::filesystem::path& root)
    {
        std::string error;
        std::vector<std::filesystem::path> files;
        for (const auto& f : std::filesystem::recursive_directory_iterator(root / "Music"))
            if (f.path().extension() == ".sec")
                files.push_back(f.path());
        std::sort(files.begin(), files.end());
        std::size_t bound = 0, missing = 0, cases = 0;
        std::uint64_t hash = 14695981039346656037ull;
        for (const auto& file : files)
        {
            ImaSection s;
            CHECK(read_ima_section(read(file), s, error));
            std::vector<std::filesystem::path> matches;
            for (const auto& f : std::filesystem::directory_iterator(file.parent_path()))
                if (lower(f.path().filename().string()) == lower(s.source.style_reference + ".sty"))
                    matches.push_back(f.path());
            if (matches.empty())
            {
                CHECK(file.parent_path().filename() == "Track1");
                ++missing;
                continue;
            }
            CHECK(matches.size() == 1);
            ImaStyle st;
            ImaTimeline t;
            CHECK(read_ima_style(read(matches[0]), st, error) && st.identifier == s.style_id &&
                  prepare_ima_timeline(s, st, t, error));
            for (auto tick : {-1, 0, 1, int(t.ticks_per_beat) - 1, int(t.ticks_per_beat), int(t.ticks_per_measure) - 1,
                              int(t.ticks_per_measure), t.duration - 1, t.duration})
            {
                CHECK(observe(s, st, t, tick, {12, 0x204}, hash, error));
                ++cases;
            }
            ++bound;
        }
        std::cout << "Private selections " << cases << "; bound/missing " << bound << '/' << missing << "; FNV64 "
                  << hash << '\n';
        CHECK(bound == 132 && missing == 24 && cases == 1188 && hash == 2704444894253380274ull);
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
