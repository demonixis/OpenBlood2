#include "engine/ima_timeline.h"
#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iostream>
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
    ImaSection section()
    {
        ImaSection s;
        s.measures = 2;
        s.ticks_per_measure = 768;
        s.ticks_per_beat = 192;
        return s;
    }
    ImaStyle style()
    {
        ImaStyle s;
        s.beats_per_measure = 6;
        s.beat_unit = 8;
        return s;
    }
    bool timing()
    {
        std::string error;
        auto s = section();
        auto st = style();
        ImaTimeline t;
        for (auto tick : {-769, -767, 1536, 768 * 32768})
            s.commands.push_back({tick, {1, 2, 3, 4}});
        for (auto [measure, beat] : {std::pair{-2, -1}, {7, 5}, {0, 6}, {0, 127}, {0, -128}})
        {
            ImaSectionChord c;
            c.measure = measure;
            c.beat = beat;
            s.chords.push_back(c);
        }
        CHECK(prepare_ima_timeline(s, st, t, error));
        CHECK(t.ticks_per_beat == 96 && t.ticks_per_measure == 576 && t.duration == 1152 && !t.default_beat_unit);
        CHECK(t.commands.size() == 4 && t.commands[0].time == -576 && t.commands[1].time == 0 &&
              t.commands[2].time == 1152 && t.commands[3].time == -18874368);
        CHECK(t.chords.size() == 3 && t.chords[0].time == -1248 && t.chords[1].time == 4512 &&
              t.chords[2].time == -12288);
        CHECK(t.chords[2].source == 4 && (t.discarded_chords == std::vector<std::size_t>{2, 3}));
        CHECK(s.commands[0].time == -769 && s.chords.size() == 5 && s.ticks_per_measure == 768); // Raw input unchanged.
        st.beats_per_measure = 4;
        st.beat_unit = 0;
        CHECK(prepare_ima_timeline(s, st, t, error) && t.default_beat_unit && t.ticks_per_beat == 192 &&
              st.beat_unit == 0);
        st.beats_per_measure = 3;
        st.beat_unit = 7;
        CHECK(prepare_ima_timeline(s, st, t, error) && t.ticks_per_beat == 109 && t.ticks_per_measure == 327 &&
              t.duration == 654);
        s.ticks_per_measure = 1;
        s.commands = {{INT32_MIN, {}}, {INT32_MAX, {}}};
        CHECK(prepare_ima_timeline(s, st, t, error) && t.commands[0].time == 0 && t.commands[1].time == -327);
        s = {};
        st = {};
        CHECK(t.commands[1].time == -327 && t.chords.back().source == 4); // Owned, no borrowed inputs.
        std::cout << "Timeline: original style override/default, truncation, signed/wrapped command measures, unsorted "
                     "chord times and discarded beats\n";
        return true;
    }
    bool mutes()
    {
        std::string error;
        auto s = section();
        auto st = style();
        ImaTimeline t;
        s.mutes = {{500, 7, 99}, {200, 9, 98}, {576, 11, 97}, {576, 13, 96}, {999999, 15, 95}};
        CHECK(prepare_ima_timeline(s, st, t, error) && t.mutes.size() == 12);
        for (std::size_t i = 0; i < t.mutes.size(); ++i)
        {
            CHECK(t.mutes[i].time == int(i * 96));
            CHECK(t.mutes[i].value == (i < 6 ? 7 : 13) && t.mutes[i].source == (i < 6 ? 0 : 3));
        }
        CHECK(s.mutes[0].word6 == 99 && s.mutes[1].time == 200);
        s.mutes = {{-1, 1, 0}, {0, 2, 0}, {1, 3, 0}, {96, 4, 0}, {1152, 5, 0}};
        CHECK(prepare_ima_timeline(s, st, t, error) && t.mutes[0].value == 2 && t.mutes[0].source == 1 &&
              t.mutes[1].value == 4 && t.mutes.back().value == 4);
        s.mutes.clear();
        CHECK(prepare_ima_timeline(s, st, t, error) && t.mutes.size() == 12);
        for (const auto& m : t.mutes)
            CHECK(m.value == 0 && m.source == std::numeric_limits<std::size_t>::max());
        std::cout << "Timeline mutes: initial future value, authored unsorted/duplicate traversal, beat boundaries, "
                     "strict end and empty input\n";
        return true;
    }
    bool bounds()
    {
        std::string error;
        auto s = section();
        auto st = style();
        ImaTimeline t;
        for (auto v : {-32768, -1, 0})
        {
            s.measures = v;
            CHECK(!prepare_ima_timeline(s, st, t, error) && !error.empty());
            s = section();
            s.ticks_per_measure = v;
            CHECK(!prepare_ima_timeline(s, st, t, error));
            s = section();
            st.beats_per_measure = v;
            CHECK(!prepare_ima_timeline(s, st, t, error));
            st = style();
        }
        for (auto v : {-32768, -1, 769, 32767})
        {
            st.beat_unit = v;
            CHECK(!prepare_ima_timeline(s, st, t, error));
        }
        st = style();
        st.beats_per_measure = 32767;
        st.beat_unit = 1;
        CHECK(!prepare_ima_timeline(s, st, t, error));
        st = style();
        CHECK(prepare_ima_timeline(s, st, t, error, 12));
        CHECK(!prepare_ima_timeline(s, st, t, error, 11) && t.records() == 0 && t.duration == 0);
        s.commands.resize(1);
        CHECK(!prepare_ima_timeline(s, st, t, error, 12));
        s.chords.resize(1);
        CHECK(!prepare_ima_timeline(s, st, t, error, 13));
        CHECK(prepare_ima_timeline(s, st, t, error, 14));
        s.chords[0].beat = 6;
        CHECK(prepare_ima_timeline(s, st, t, error, 13) && t.discarded_chords.size() == 1);
        s.commands.resize(4096);
        CHECK(!prepare_ima_timeline(s, st, t, error));
        s = section();
        s.measures = 32767;
        CHECK(!prepare_ima_timeline(s, st, t, error, SIZE_MAX) && !error.empty() && t.records() == 0 &&
              t.discarded_chords.empty());
        st.beats_per_measure = 1;
        st.beat_unit = 1;
        s.measures = 1;
        CHECK(prepare_ima_timeline(s, st, t, error, 1) && t.duration == 768);
        std::cout
            << "Timeline bounds: unsafe divisors/meters/lengths, overflow, raw and expanded budgets, atomic clearing\n";
        return true;
    }
    void word(Bytes& b, std::uint32_t v, unsigned n = 4)
    {
        for (unsigned i = 0; i < n; ++i)
            b.push_back(static_cast<std::uint8_t>(v >> (8 * i)));
    }
    Bytes read(const std::filesystem::path& f)
    {
        std::ifstream in(f, std::ios::binary);
        return Bytes(std::istreambuf_iterator<char>(in), {});
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
        std::uint64_t hash = 14695981039346656037ull;
        std::size_t bound = 0, missing = 0, commands = 0, mutes = 0, chords = 0;
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
            CHECK(read_ima_style(read(matches[0]), st, error) && st.identifier == s.style_id);
            ImaTimeline t;
            CHECK(prepare_ima_timeline(s, st, t, error));
            ++bound;
            CHECK(t.discarded_chords.empty());
            commands += t.commands.size();
            mutes += t.mutes.size();
            chords += t.chords.size();
            Bytes canonical;
            word(canonical, t.ticks_per_beat, 2);
            word(canonical, t.ticks_per_measure, 2);
            word(canonical, t.duration);
            word(canonical, t.commands.size());
            for (const auto& c : t.commands)
                word(canonical, c.time);
            word(canonical, t.mutes.size());
            for (const auto& m : t.mutes)
            {
                word(canonical, m.time);
                word(canonical, m.value, 2);
            }
            word(canonical, t.chords.size());
            for (const auto& c : t.chords)
                word(canonical, c.time);
            for (auto b : canonical)
            {
                hash ^= b;
                hash *= 1099511628211ull;
            }
        }
        std::cout << "Private timelines: " << bound << " bound / " << missing << " missing style / " << commands
                  << " commands / " << mutes << " mutes / " << chords << " chords / original FNV64 " << hash << '\n';
        CHECK(files.size() == 156 && bound == 132 && missing == 24 && commands == 132 && mutes == 3030 &&
              chords == 132 && hash == 4011438205292846879ull);
        return true;
    }
} // namespace
int main(int argc, char** argv)
{
    if (argc < 2)
        return 2;
    const std::string_view test = argv[1];
    return (test == "timing"                 ? timing()
            : test == "mutes"                ? mutes()
            : test == "bounds"               ? bounds()
            : test == "private" && argc == 3 ? private_data(argv[2])
                                             : false)
               ? 0
               : 1;
}
