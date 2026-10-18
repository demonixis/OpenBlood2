#include "engine/ima_variation.h"
#include <algorithm>
#include <bit>
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
    void word(Bytes& b, std::uint32_t v, unsigned size = 4)
    {
        for (unsigned i = 0; i < size; ++i)
            b.push_back(static_cast<std::uint8_t>(v >> (i * 8)));
    }
    Bytes tail(const ImaPattern& p)
    {
        Bytes b;
        word(b, p.chord_mask);
        word(b, p.scale_mask);
        for (auto x : p.upper_pitch)
            b.push_back(static_cast<std::uint8_t>(x));
        for (auto x : p.lower_pitch)
            b.push_back(static_cast<std::uint8_t>(x));
        word(b, p.pitch_range_channels, 2);
        for (const auto& row : p.variation_flags)
            for (auto x : row)
                word(b, x, 2);
        word(b, p.word606, 2);
        b.push_back(static_cast<std::uint8_t>(p.root));
        b.insert(b.end(), p.play_modes.begin(), p.play_modes.end());
        return b;
    }
    Bytes chunk(std::string_view id, const Bytes& p)
    {
        Bytes b(id.begin(), id.end());
        word(b, p.size());
        b.insert(b.end(), p.begin(), p.end());
        if (p.size() & 1)
            b.push_back(0);
        return b;
    }
    void append(Bytes& b, const Bytes& p)
    {
        b.insert(b.end(), p.begin(), p.end());
    }
    bool reader()
    {
        std::string error;
        Bytes p(625);
        for (unsigned i = 52; i < 625; ++i)
            p[i] = static_cast<std::uint8_t>(i * 13 + 87);
        Bytes pat{'A', 'A', 'P', 'T'};
        append(pat, chunk("patt", p));
        Bytes data{'A', 'A', 'S', 'Y'};
        append(data, chunk("styl", Bytes(94)));
        append(data, chunk("RIFF", pat));
        ImaStyle style;
        CHECK(read_ima_style(chunk("RIFF", data), style, error));
        const auto& decoded = style.patterns.front();
        CHECK(tail(decoded) == Bytes(p.begin() + 52, p.end()));
        CHECK(decoded.upper_pitch[4] == -105 && decoded.lower_pitch[0] == 51);
        const auto saved = tail(decoded);
        data.clear();
        p.clear();
        CHECK(tail(decoded) == saved);
        // Defaults are per row, based on presence anywhere in that row, never overwrite raw flags.
        ImaPattern pattern;
        pattern.variation_flags[1][7] = 0x0801;
        pattern.variation_flags[2][15] = 0x1002;
        pattern.variation_flags[3][2] = 0x1804;
        pattern.play_modes[2] = 0xff;
        const auto prepared = prepare_ima_pattern_channels(pattern);
        CHECK(prepared.variation_flags[0][0] == 0x1800 && prepared.variation_flags[0][15] == 0x1800);
        CHECK(prepared.variation_flags[1][0] == 0x1000 && prepared.variation_flags[1][7] == 0x1801);
        CHECK(prepared.variation_flags[2][0] == 0x0800 && prepared.variation_flags[2][15] == 0x1802);
        CHECK(prepared.variation_flags[3][0] == 0 && prepared.variation_flags[3][2] == 0x1804);
        CHECK(prepared.play_modes[0] == 3 && prepared.play_modes[4] == 2 && prepared.play_modes[5] == 1 &&
              prepared.play_modes[2] == 255);
        CHECK(pattern.variation_flags[0][0] == 0 && pattern.variation_flags[1][7] == 0x0801 &&
              pattern.play_modes[4] == 0);
        std::cout << "All 573 pattern-tail bytes round trip; row-local defaults and authored modes preserved\n";
        return true;
    }
    constexpr std::array<std::uint16_t, 16> current{0,      1, 2,      4,      8,      0x4000, 0x8000, 0xffff,
                                                    0x8001, 3, 0x7fff, 0x1234, 0xaaaa, 0x5555, 0xff00, 0xff};
    struct Scenario
    {
        std::optional<std::uint16_t> flags;
        std::uint16_t random, cycle;
    };
    const std::array<Scenario, 6> scenarios{{{std::nullopt, 0xffff, 0xffff},
                                             {0, 0, 0xffff},
                                             {1, 0x5555, 0xffff},
                                             {0x8000, 0xffff, 0},
                                             {0x1800, 0xffff, 0xffff},
                                             {0xffff, 0x8001, 0x7ffe}}};
    std::vector<std::uint16_t> draws(unsigned count)
    {
        std::vector<std::uint16_t> values;
        for (unsigned i = 0; i < count; ++i)
            values.push_back(static_cast<std::uint16_t>((i * 997 + 32760) & 32767));
        return values;
    }
    bool selection()
    {
        std::string error;
        ImaPatternChannels pattern;
        pattern.variation_flags[0][3] = 0x8001;
        pattern.variation_flags[0][11] = 0x8001;
        pattern.variation_flags[1][15] = 0x8001; // Single candidate still consumes a draw.
        std::array<std::uint16_t, 2> random{32767, 7};
        ImaVariationChoice out;
        CHECK(choose_ima_variations(pattern, 0x8001, current, 3, 0xffff, random, out, error));
        CHECK(out.candidates[0] == 0x808 && out.candidates[1] == 0x8000 && out.candidates[2] == 0xffff);
        CHECK(out.selected[0] == 0x800 && out.selected[1] == 0x8000 && out.draws_used == 2 &&
              out.fallback_channels == 0xfffc);
        CHECK(out.selected[5] == 0x8000 && out.selected[6] == 1 && out.selected[7] == 1 && out.selected[9] == 6);
        CHECK(choose_ima_variations(pattern, 0x8001, current, 0, 0, {}, out, error));
        CHECK(out.selected == current && out.draws_used == 0); // Hold ignores candidate eligibility.
        CHECK(choose_ima_variations(pattern, 0, current, 0, 0xffff, {}, out, error));
        CHECK(out.fallback_channels == 0 && out.selected[0] == 0 && out.candidates[0] == 0xffff);
        CHECK(choose_ima_variations(pattern, std::nullopt, current, 0, 0, {}, out, error));
        CHECK(out.fallback_channels == 0xffff && out.selected == current);
        std::cout << "Original filter/fallback, random precedence and draw order, cycle wrap/zero/multi-bit and hold\n";
        return true;
    }
    bool bounds()
    {
        std::string error;
        ImaVariationChoice out;
        ImaPatternChannels pattern;
        for (auto invalid :
             {std::vector<std::uint16_t>{}, std::vector<std::uint16_t>{1, 2}, std::vector<std::uint16_t>{32768}})
        {
            out.selected.fill(99);
            out.draws_used = 9;
            CHECK(!choose_ima_variations(pattern, 0, current, 1, 0, invalid, out, error));
            CHECK(!error.empty() && out.draws_used == 0 && out.fallback_channels == 0 &&
                  (out.selected == std::array<std::uint16_t, 16>{}));
        }
        CHECK(choose_ima_variations(pattern, 0, current, 0, 0, {}, out, error) && error.empty());
        std::cout << "Explicit RNG count/range preflight; no partial output or input mutation\n";
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
        std::uint64_t hash = 14695981039346656037ull;
        std::size_t patterns = 0, choices = 0;
        for (const auto& f : files)
        {
            std::ifstream in(f, std::ios::binary);
            Bytes data{std::istreambuf_iterator<char>(in), {}};
            ImaStyle style;
            CHECK(read_ima_style(data, style, error));
            std::size_t pi = 0;
            for (const auto& c : style.source.chunks)
                if (c.id == "patt")
                {
                    CHECK(pi < style.patterns.size());
                    const auto& p = style.patterns[pi++];
                    Bytes canonical = tail(p);
                    CHECK(canonical == Bytes(data.begin() + c.offset + 8 + 52, data.begin() + c.offset + 8 + 625));
                    const auto effective = prepare_ima_pattern_channels(p);
                    for (const auto& row : effective.variation_flags)
                        for (auto v : row)
                            word(canonical, v, 2);
                    canonical.insert(canonical.end(), effective.play_modes.begin(), effective.play_modes.end());
                    for (const auto& s : scenarios)
                    {
                        ImaVariationChoice result;
                        CHECK(choose_ima_variations(effective, s.flags, current, s.random, s.cycle,
                                                    draws(std::popcount(s.random)), result, error));
                        for (auto v : result.candidates)
                            word(canonical, v, 2);
                        for (auto v : result.selected)
                            word(canonical, v, 2);
                        word(canonical, result.fallback_channels, 2);
                        word(canonical, result.draws_used, 2);
                        ++choices;
                    }
                    for (auto b : canonical)
                    {
                        hash ^= b;
                        hash *= 1099511628211ull;
                    }
                    ++patterns;
                }
            CHECK(pi == style.patterns.size());
        }
        std::cout << "Private original equivalence: " << files.size() << " styles / " << patterns << " patterns / "
                  << choices << " choices; FNV64 " << hash << '\n';
        CHECK(files.size() == 39 && patterns == 131 && choices == 786 && hash == 7572323464032181405ull);
        return true;
    }
} // namespace
int main(int argc, char** argv)
{
    if (argc < 2)
        return 2;
    const std::string_view test = argv[1];
    return (test == "reader"                 ? reader()
            : test == "selection"            ? selection()
            : test == "bounds"               ? bounds()
            : test == "private" && argc == 3 ? private_data(argv[2])
                                             : false)
               ? 0
               : 1;
}
