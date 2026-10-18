#include "engine/ima_harmony.h"
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
    void be(std::vector<std::uint8_t>& d, std::size_t pos, std::uint32_t value)
    {
        for (int i = 0; i < 4; ++i)
            d[pos + i] = static_cast<std::uint8_t>(value >> (24 - i * 8));
    }
    std::vector<std::uint8_t> personality()
    {
        std::vector<std::uint8_t> d(12 + 1214);
        std::copy_n("REPs", 4, d.begin());
        be(d, 4, d.size() - 8);
        be(d, 8, 1214);
        d[28] = 0xb5;
        d[29] = 0x5a;
        d[30] = 0xab;
        std::copy_n("Silence", 7, d.begin() + 32);
        return d;
    }
    bool reader()
    {
        std::string error;
        auto d = personality();
        ImaPersonality p;
        CHECK(read_ima_personality(d, p, error) && p.scale_mask == 0xab5ab5 && p.name == "Silence" &&
              p.header_bytes == 1214 && p.opaque_chunks.empty());
        for (std::size_t n = 0; n < d.size(); ++n)
        {
            CHECK(!read_ima_personality(std::span(d).first(n), p, error));
            CHECK(p.name.empty() && p.scale_mask == 0 && p.opaque_chunks.empty());
        }
        auto bad = d;
        bad[0] = 'X';
        CHECK(!read_ima_personality(bad, p, error));
        for (auto n : {0u, 1163u, 65537u, 0xffffffffu})
        {
            bad = d;
            be(bad, 8, n);
            CHECK(!read_ima_personality(bad, p, error));
        }
        for (auto offset : {32, 52, 132})
        {
            bad = d;
            bad[offset] = 255;
            CHECK(!read_ima_personality(bad, p, error));
            bad = d;
            std::fill_n(bad.begin() + offset, offset == 52 ? 80 : 20, 'A');
            CHECK(!read_ima_personality(bad, p, error));
        }
        d.insert(d.end(), {'N', 'E', 'C', 's', 0, 0, 0, 1, 7});
        be(d, 4, d.size() - 8);
        CHECK(read_ima_personality(d, p, error) && p.opaque_chunks.size() == 1 && p.opaque_chunks[0].size == 1);
        auto owned = p;
        d.back() = 99;
        CHECK(owned.name == "Silence");
        for (int i = 0; i < 255; ++i)
            d.insert(d.end(), {'X', 'X', 'X', 'X', 0, 0, 0, 0});
        be(d, 4, d.size() - 8);
        CHECK(read_ima_personality(d, p, error) && p.opaque_chunks.size() == 256);
        d.push_back(0);
        be(d, 4, d.size() - 8);
        CHECK(!read_ima_personality(d, p, error) && p.opaque_chunks.empty());
        d.resize(1024 * 1024 + 1);
        CHECK(!read_ima_personality(d, p, error));
        return true;
    }
    ImaSection section()
    {
        ImaSection s;
        s.measures = 1;
        s.ticks_per_measure = 768;
        s.default_chord = 128;
        s.chords.resize(1);
        s.chords[0].chord_pattern = 0x91;
        return s;
    }
    ImaStyle style()
    {
        ImaStyle s;
        s.beat_unit = 4;
        s.beats_per_measure = 4;
        return s;
    }
    bool prepare(const ImaSection& s, std::uint32_t mask, ImaHarmony& h, std::string& error)
    {
        ImaTimeline t;
        ImaPersonality p;
        p.scale_mask = mask;
        return prepare_ima_timeline(s, style(), t, error) && prepare_ima_harmony(s, t, p, h, error);
    }
    void hash(std::uint64_t& value, const ImaHarmony& h)
    {
        const auto& c = h.chords.front();
        for (auto b : {h.base.chord_flags, c.harmony.chord_flags, std::uint8_t(c.variation_flags),
                       std::uint8_t(c.variation_flags >> 8)})
        {
            value ^= b;
            value *= 1099511628211ull;
        }
    }
    bool behavior()
    {
        std::string error;
        auto s = section();
        ImaHarmony h;
        CHECK(prepare(s, 0xab5ab5, h, error));
        CHECK(h.base_flag && h.base.scale_mask == 0xab5ab5 && !h.base.enabled && h.chords.size() == 1);
        CHECK(h.base.chord_flags == 0x7e && h.chords[0].harmony.chord_flags == 0x13 &&
              h.chords[0].variation_flags == 0x2101);
        s.chords[0].scale_pattern = 1;
        s.chords[0].flags = 8;
        s.chords[0].root = 1;
        s.default_chord = 135;
        CHECK(prepare(s, 0xab5ab5, h, error) && h.base.scale_root == 7 && h.base_flag);
        CHECK(h.chords[0].harmony.scale_mask == 1 && h.chords[0].harmony.chord_flags == 0x93 &&
              h.chords[0].harmony.chord_root == 1 && h.chords[0].harmony.scale_root == 7);
        // Retained order controls next-beat classification; discarded source chords are not the successor.
        s = section();
        s.chords.resize(3);
        s.chords[1].beat = 4;
        s.chords[1].root = 5;
        s.chords[2].root = 7;
        s.chords[2].beat = 0;
        s.chords[2].chord_pattern = 0x89;
        CHECK(prepare(s, 0xab5ab5, h, error) && h.chords.size() == 2 && h.chords[1].source == 2 &&
              h.chords[0].variation_flags == 0x3101);
        s.chords[1].beat = -1;
        s.chords[1].measure = -1;
        CHECK(prepare(s, 0xab5ab5, h, error) && h.chords.size() == 3 && h.chords[1].time == -960 &&
              h.chords[0].variation_flags == 0x2101);
        ImaTimeline t;
        ImaPersonality p;
        CHECK(prepare_ima_timeline(s, style(), t, error));
        auto bad = t;
        bad.chords[0].source = 500;
        CHECK(!prepare_ima_harmony(s, bad, p, h, error) && h.chords.empty() && h.base.scale_mask == 0);
        bad = t;
        bad.chords[0].time += 1;
        CHECK(!prepare_ima_harmony(s, bad, p, h, error));
        bad = t;
        bad.chords.pop_back();
        CHECK(!prepare_ima_harmony(s, bad, p, h, error));
        bad = t;
        bad.chords[1].source = 0;
        CHECK(!prepare_ima_harmony(s, bad, p, h, error));
        bad = t;
        bad.ticks_per_beat = 0;
        CHECK(!prepare_ima_harmony(s, bad, p, h, error));
        s.chords.resize(4097);
        CHECK(!prepare_ima_harmony(s, t, p, h, error));
        s.chords.clear();
        t.chords.clear();
        CHECK(prepare_ima_harmony(s, t, p, h, error) && h.chords.empty());
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
        const auto sb = [](std::uint32_t v) { return std::bit_cast<std::int8_t>(std::uint8_t(v)); };
        const std::uint32_t masks[]{0, 1, 0x91, 0x89, 0xab5, 0xab5ab5, 0x3c0000, 0xffffffff};
        std::uint64_t digest = 14695981039346656037ull;
        for (unsigned i = 0; i < 4096; ++i)
        {
            auto s = section();
            auto& c = s.chords[0];
            c.chord_pattern = i & 1 ? draw() : masks[(i / 2) % 8];
            c.scale_pattern = i & 2 ? draw() : 0;
            c.root = sb(draw() >> 24);
            c.flags = std::uint8_t(draw() >> 24);
            const auto bm = i & 4 ? draw() : masks[(i / 8) % 8];
            s.default_chord = std::uint8_t(draw() >> 24);
            const auto nr = sb(draw() >> 24);
            const auto nb = i & 8 ? 0 : sb(draw() >> 24);
            if (i & 16)
            {
                s.chords.emplace_back();
                s.chords[1].root = nr;
                // Classifier only distinguishes zero/nonzero. Keep the synthetic successor within the meter.
                s.chords[1].beat = nb == 0 ? 0 : -1;
            }
            ImaHarmony h;
            CHECK(prepare(s, bm, h, error));
            hash(digest, h);
            CHECK(!h.chords[0].harmony.enabled && h.base_flag == bool(s.default_chord & 128));
        }
        std::cout << "Original chord cases: 4096; FNV64 " << digest << '\n';
        CHECK(digest == 12106312165928785022ull);
        return true;
    }
    std::vector<std::uint8_t> read(const std::filesystem::path& p)
    {
        std::ifstream in(p, std::ios::binary);
        return {std::istreambuf_iterator<char>(in), {}};
    }
    bool private_data(const std::filesystem::path& root)
    {
        std::string error;
        std::vector<std::filesystem::path> files;
        for (const auto& f : std::filesystem::recursive_directory_iterator(root / "Music"))
            if (f.path().extension() == ".sec")
                files.push_back(f.path());
        std::sort(files.begin(), files.end());
        std::uint64_t digest = 14695981039346656037ull;
        for (const auto& f : files)
        {
            ImaSection s;
            ImaPersonality p;
            ImaTimeline t;
            ImaHarmony h;
            CHECK(read_ima_section(read(f), s, error) &&
                  read_ima_personality(read(f.parent_path() / "SILENCE.PER"), p, error));
            CHECK(p.name == "Silence" && p.scale_mask == 0xab5ab5 && p.header_bytes == 1214 &&
                  p.opaque_chunks.size() == 1);
            CHECK(prepare_ima_timeline(s, style(), t, error) && prepare_ima_harmony(s, t, p, h, error));
            CHECK(h.chords.size() == 1 && h.chords.front().source == 0);
            hash(digest, h);
            auto context = h.chords.front().harmony;
            context.enabled = true;
            ImaNote n;
            n.type = 9;
            n.music_value = 0x4000;
            n.play_mode = 0x83;
            ImaNotePitch pitch;
            CHECK(resolve_ima_note_pitch(n, {}, context, pitch, error) && pitch.converted);
        }
        std::cout << "Private chord sections " << files.size() << "; FNV64 " << digest << '\n';
        CHECK(files.size() == 156 && digest == 11061850450855354925ull);
        return true;
    }
} // namespace
int main(int argc, char** argv)
{
    if (argc < 2)
        return 2;
    const std::string_view t = argv[1];
    return (t == "reader"                 ? reader()
            : t == "behavior"             ? behavior()
            : t == "generated"            ? generated()
            : t == "private" && argc == 3 ? private_data(argv[2])
                                          : false)
               ? 0
               : 1;
}
