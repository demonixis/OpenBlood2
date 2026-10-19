#include "engine/dls_bank.h"
#include "engine/music_asset.h"
#include <algorithm>
#include <bit>
#include <cmath>
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
    using Bytes = std::vector<std::uint8_t>;
    void u16(Bytes& b, std::uint16_t v)
    {
        b.push_back(v & 255);
        b.push_back(v >> 8);
    }
    void u32(Bytes& b, std::uint32_t v)
    {
        u16(b, v & 65535);
        u16(b, v >> 16);
    }
    Bytes words(std::initializer_list<std::uint32_t> v)
    {
        Bytes b;
        for (auto n : v)
            u32(b, n);
        return b;
    }
    Bytes shorts(std::initializer_list<std::uint16_t> v)
    {
        Bytes b;
        for (auto n : v)
            u16(b, n);
        return b;
    }
    Bytes join(std::initializer_list<Bytes> lists)
    {
        Bytes b;
        for (const auto& s : lists)
            b.insert(b.end(), s.begin(), s.end());
        return b;
    }
    Bytes chunk(std::string_view tag, Bytes data)
    {
        Bytes b(tag.begin(), tag.end());
        u32(b, data.size());
        b.insert(b.end(), data.begin(), data.end());
        if (data.size() & 1)
            b.push_back(0);
        return b;
    }
    Bytes list(std::string_view form, Bytes data, std::string_view tag = "LIST")
    {
        Bytes b(form.begin(), form.end());
        b.insert(b.end(), data.begin(), data.end());
        return chunk(tag, b);
    }
    Bytes settings(std::uint16_t note, std::int16_t fine, std::int32_t attenuation, bool loop)
    {
        return chunk("wsmp", join({words({20}), shorts({note, std::bit_cast<std::uint16_t>(fine)}),
                                   words({std::bit_cast<std::uint32_t>(attenuation), 1, loop ? 1U : 0U}),
                                   loop ? words({16, 0, 1, 2}) : Bytes{}}));
    }
    Bytes graph(bool empty = false)
    {
        return list("lart",
                    chunk("art1", join({words({8, empty ? 0U : 1U}),
                                        empty ? Bytes{} : join({shorts({0x81, 2, 0x209, 1}), words({0xffff0000})})})));
    }
    Bytes wave(bool eight)
    {
        return list("wave", join({chunk("fmt ", join({shorts({1, 1}), words({22050, eight ? 22050U : 44100U}),
                                                      shorts({eight ? std::uint16_t(1) : std::uint16_t(2),
                                                              eight ? std::uint16_t(8) : std::uint16_t(16)}),
                                                      eight ? Bytes{} : shorts({18})})),
                                  chunk("data", eight ? Bytes{0, 128, 255, 64} : shorts({0x8000, 0, 0x7fff, 0x4000})),
                                  settings(eight ? 72 : 60, -58, -655360, true)}));
    }
    Bytes region(std::uint32_t cue, bool override_settings)
    {
        return list("rgn ", join({chunk("rgnh", shorts({10, 100, 20, 127, 1, 2})),
                                  chunk("wlnk", join({shorts({0, 0}), words({1, cue})})),
                                  override_settings ? settings(35, 27, 0, false) : Bytes{}, graph(true)}));
    }
    Bytes fixture()
    {
        const auto first = wave(false), second = wave(true);
        const auto inst = list("ins ", join({chunk("insh", words({2, 0, 1})),
                                             list("lrgn", join({region(0, false), region(1, true)})), graph()}));
        return list("DLS ",
                    join({chunk("colh", words({1})), list("lins", inst),
                          chunk("ptbl", words({8, 3, std::uint32_t(first.size()), 0, std::uint32_t(first.size())})),
                          list("wvpl", join({first, second})), chunk("junk", {7, 8, 9})}),
                    "RIFF");
    }
    std::size_t find(const Bytes& b, const char* tag, unsigned occurrence = 0)
    {
        MusicAssetInfo info;
        std::string e;
        if (!read_music_asset(b, MusicAssetKind::Instruments, info, e))
            return 0;
        for (const auto& c : info.chunks)
            if (c.id == tag && occurrence-- == 0)
                return c.offset + 8;
        return 0;
    }
    void patch(Bytes& b, std::size_t p, std::uint32_t v, unsigned size = 4)
    {
        for (unsigned i = 0; i < size; ++i)
            b[p + i] = v >> (i * 8);
    }
    bool decode()
    {
        std::string error;
        auto bytes = fixture();
        DlsBank bank;
        CHECK(read_dls_bank(bytes, bank, error));
        CHECK(bank.waves.size() == 2 && bank.instruments.size() == 1 &&
              bank.cues == std::vector<std::uint32_t>({1, 0, 1}));
        CHECK(bank.decoded_bytes == 32 && bank.opaque_chunks == 1);
        CHECK(bank.waves[0].samples == std::vector<float>({-1, 0, 32767.0f / 32768, .5f}));
        CHECK(bank.waves[1].samples == std::vector<float>({-1, 0, 127.0f / 128, -.5f}));
        const auto& inst = bank.instruments[0];
        const auto& a = inst.regions[0];
        const auto& b = inst.regions[1];
        CHECK(inst.program == 1 && inst.bank == 0 && inst.regions.size() == 2);
        CHECK(a.wave == 1 && a.cue == 0 && !a.settings_override && a.settings.unity_note == 72);
        CHECK(a.settings.fine_tune == -58 && a.settings.attenuation == -655360);
        // The inherited two-frame loop is authored; MSynth25 plays a loop under five/six frames once.
        CHECK(a.settings.authored_loop && a.settings.authored_loop->start == 1 &&
              a.settings.authored_loop->length == 2 && !a.settings.loop);
        CHECK(b.wave == 0 && b.cue == 1 && b.settings_override && b.settings.unity_note == 35);
        CHECK(b.settings.fine_tune == 27 && !b.settings.loop);
        CHECK(a.key_low == 10 && a.key_high == 100 && a.velocity_low == 20 && a.velocity_high == 127);
        CHECK(a.key_group == 2 && a.options == 1 && a.articulation.present && a.articulation.connections.empty());
        CHECK(inst.articulation.present && inst.articulation.connections.size() == 1);
        const auto& c = inst.articulation.connections[0];
        CHECK(c.source == 0x81 && c.control == 2 && c.destination == 0x209 && c.transform == 1 && c.scale == -65536);
        bytes.assign(bytes.size(), 0);
        CHECK(bank.waves[0].samples[3] == .5f && a.settings.authored_loop->length == 2);
        // The retail Cabal profile has an invalid wave default that all users override.
        auto overridden = fixture();
        patch(overridden, find(overridden, "wsmp", 1) + 28, 7000);
        CHECK(read_dls_bank(overridden, bank, error));
        CHECK(!bank.waves[0].settings->loop_in_bounds && bank.instruments[0].regions[1].settings.loop_in_bounds);
        auto no_default = fixture();
        auto wsmp = find(no_default, "wsmp", 1);
        std::copy_n("junk", 4, no_default.begin() + wsmp - 8);
        CHECK(read_dls_bank(no_default, bank, error)); // Every wave-0 region supplies its own settings.
        wsmp = find(no_default, "wsmp", 1);
        std::copy_n("junk", 4, no_default.begin() + wsmp - 8);
        CHECK(!read_dls_bank(no_default, bank, error)); // Wave-1 region now has no usable settings.
        auto ignored_pcm_size = fixture();
        patch(ignored_pcm_size, find(ignored_pcm_size, "fmt ") + 16, 65535, 2);
        CHECK(read_dls_bank(ignored_pcm_size, bank, error)); // PCM cbSize is ignored, never used to read more bytes.
        CHECK(read_dls_bank(fixture(), bank, error, 32));
        CHECK(!read_dls_bank(fixture(), bank, error, 31));
        CHECK(bank.waves.empty() && bank.instruments.empty() && bank.cues.empty() && bank.decoded_bytes == 0);
        std::cout << "DLS PCM endpoints, signed values, reordered/aliased cues, region override/inheritance, layered "
                     "order, raw graphs and owned lifetime verified\n";
        return true;
    }
    bool malformed()
    {
        std::string error;
        const auto valid = fixture();
        DlsBank bank;
        auto rejected = [&](const Bytes& b)
        {
            bank.waves.resize(1);
            bank.instruments.resize(1);
            bank.decoded_bytes = 4;
            return !read_dls_bank(b, bank, error) && !error.empty() && bank.waves.empty() && bank.instruments.empty() &&
                   bank.cues.empty() && !bank.decoded_bytes && !bank.opaque_chunks;
        };
        for (std::size_t n = 0; n < valid.size(); ++n)
            CHECK(rejected(Bytes(valid.begin(), valid.begin() + n)));
        struct Mutation
        {
            const char* tag;
            unsigned offset, value, size;
        };
        for (const Mutation m : {Mutation{"colh", 0, 2, 4}, {"colh", 0, 257, 4},        {"insh", 0, 4097, 4},
                                 {"insh", 8, 128, 4},       {"ptbl", 0, 12, 4},         {"ptbl", 4, 0xffffffff, 4},
                                 {"ptbl", 8, 1, 4},         {"ptbl", 8, 0xffffffff, 4}, {"rgnh", 0, 101, 2},
                                 {"rgnh", 2, 128, 2},       {"rgnh", 4, 128, 2},        {"rgnh", 8, 2, 2},
                                 {"rgnh", 10, 16, 2},       {"wlnk", 0, 2, 2},          {"wlnk", 4, 2, 4},
                                 {"wlnk", 8, 3, 4},         {"wsmp", 0, 24, 4},         {"wsmp", 4, 128, 2},
                                 {"wsmp", 12, 4, 4},        {"wsmp", 16, 2, 4},         {"fmt ", 0, 17, 2},
                                 {"fmt ", 2, 2, 2},         {"fmt ", 4, 0xffffffff, 4}, {"fmt ", 8, 1, 4},
                                 {"fmt ", 12, 1, 2},        {"fmt ", 14, 24, 2},        {"art1", 0, 12, 4},
                                 {"art1", 4, 8193, 4}})
        {
            auto bad = valid;
            patch(bad, find(valid, m.tag) + m.offset, m.value, m.size);
            CHECK(rejected(bad));
        }
        // Wave loop, distinct from the one-shot region override (the first wsmp).
        for (const auto [offset, value] : {std::pair{20U, 20U}, {24U, 1U}, {32U, 0U}})
        {
            auto bad = valid;
            patch(bad, find(valid, "wsmp", 2) + offset, value);
            CHECK(rejected(bad));
        }
        auto bad = valid;
        const auto data = find(bad, "data");
        // Same-sized duplicate format identifier must be rejected, not last-wins.
        std::copy_n("fmt ", 4, bad.begin() + data - 8);
        CHECK(rejected(bad));
        bad = valid;
        std::copy_n("rgn2", 4, bad.begin() + find(bad, "rgnh") - 12);
        CHECK(rejected(bad));
        bad = valid;
        bad.push_back(0);
        CHECK(rejected(bad));
        CHECK(!read_dls_bank(valid, bank, error, 0) && bank.waves.empty());
        std::cout << "DLS all synthetic truncations, references/counts/ranges/duplicates/loops/formats and "
                     "transactional budget failure verified\n";
        return true;
    }
    bool private_data(const std::filesystem::path& root)
    {
        std::string error;
        std::size_t files = 0, clamped = 0, waves = 0, instruments = 0, regions = 0, frames = 0, connections = 0,
                    overrides = 0;
        for (const auto& file : std::filesystem::recursive_directory_iterator(root / "Music"))
        {
            if (file.path().extension() != ".dls")
                continue;
            std::ifstream in(file.path(), std::ios::binary);
            Bytes bytes{std::istreambuf_iterator<char>(in), {}};
            DlsBank bank;
            const bool decoded = read_dls_bank(bytes, bank, error);
            CHECK(decoded);
            // Crudux program 2 (664633 for 64633 frames) and Track1 program 4 (32025 for 23025)
            // author loops past their waves; MSynth25 ends them on the guard frame.
            for (const auto& i : bank.instruments)
                for (const auto& r : i.regions)
                    if (r.settings.authored_loop && !r.settings.loop_in_bounds)
                    {
                        const auto frames = bank.waves[r.wave].samples.size();
                        CHECK(r.settings.loop && r.settings.loop->start == 0 && r.settings.loop->length == frames + 1);
                        CHECK((file.path().filename() == "crudux.dls" && i.program == 2 && frames == 64633 &&
                               r.settings.authored_loop->length == 664633) ||
                              (file.path().filename() == "track1.dls" && i.program == 4 && frames == 23025 &&
                               r.settings.authored_loop->length == 32025));
                        ++clamped;
                    }
            MusicAssetInfo info;
            CHECK(read_music_asset(bytes, MusicAssetKind::Instruments, info, error));
            std::size_t wi = 0, bank_frames = 0;
            for (const auto& ch : info.chunks)
                if (ch.id == "data")
                {
                    CHECK(wi < bank.waves.size());
                    const auto& w = bank.waves[wi++];
                    CHECK(w.rate == 22050 && w.samples.size() * 2 == ch.size);
                    bool nonzero = false;
                    for (std::size_t i = 0; i < w.samples.size(); ++i)
                    {
                        const auto p = ch.offset + 8 + i * 2;
                        const int raw = bytes[p] | (int(bytes[p + 1]) << 8);
                        CHECK(w.samples[i] == (raw >= 32768 ? raw - 65536 : raw) / 32768.0f);
                        nonzero |= w.samples[i] != 0;
                    }
                    CHECK(nonzero);
                    bank_frames += w.samples.size();
                }
            CHECK(wi == bank.waves.size() && bank.decoded_bytes == bank_frames * sizeof(float));
            std::size_t br = 0, bc = 0;
            for (const auto& i : bank.instruments)
            {
                bc += i.articulation.connections.size();
                br += i.regions.size();
                for (const auto& r : i.regions)
                {
                    CHECK(r.settings_override && r.wave < bank.waves.size() && bank.cues[r.cue] == r.wave);
                    overrides += r.settings_override;
                    bc += r.articulation.connections.size();
                    if (r.settings.loop)
                        CHECK(std::uint64_t(r.settings.loop->start) + r.settings.loop->length <=
                              bank.waves[r.wave].samples.size() + 1);
                }
            }
            std::cout << file.path().filename().string() << ": " << bank.instruments.size() << " instruments / " << br
                      << " regions / " << bank.waves.size() << " waves / " << bank_frames << " frames / " << bc
                      << " connections / " << bank.opaque_chunks << " opaque chunks\n";
            ++files;
            instruments += bank.instruments.size();
            waves += bank.waves.size();
            regions += br;
            frames += bank_frames;
            connections += bc;
        }
        CHECK(files == 5 && clamped == 2 && instruments == 54 && waves == 156 && regions == 211 && overrides == 211);
        std::cout << "Private totals: " << files << " decoded banks / " << clamped << " guard-ended loops / "
                  << instruments << " instruments / " << regions << " regions / " << waves << " waves / " << frames
                  << " frames / " << connections << " raw connections; all PCM samples independently checked\n";
        return true;
    }
    // Installed MSynth25.dll wave verification (0x354962f0), region link (0x35496190) and
    // region verification (0x354960f0), observed by static inspection.
    bool msynth_loops()
    {
        std::string error; // Reported by CHECK.
        const auto same = [](std::optional<DlsLoop> l, std::uint32_t start, std::uint32_t length)
        { return l && l->start == start && l->length == length; };
        for (bool inherited : {false, true})
        {
            // In range: unchanged, including a loop ending on the last real frame.
            CHECK(same(msynth_effective_loop({10, 50}, 100, inherited), 10, 50));
            CHECK(same(msynth_effective_loop({0, 100}, 100, inherited), 0, 100));
            // Past the wave: the end stops on the guard frame (frames + 1).
            CHECK(same(msynth_effective_loop({0, 664633}, 64633, inherited), 0, 64634));
            CHECK(same(msynth_effective_loop({90, 50}, 100, inherited), 90, 11));
            // Under six frames after clamping: played once.
            CHECK(!msynth_effective_loop({96, 50}, 100, inherited));
            CHECK(!msynth_effective_loop({1, 2}, 4, inherited));
        }
        // A region start at or past the guard restarts at zero; the stored end still clamps.
        CHECK(same(msynth_effective_loop({101, 10}, 100, false), 0, 101));
        CHECK(same(msynth_effective_loop({500, 10}, 100, false), 0, 101));
        // Inherited from the wave: its own verification first moves the start onto the end,
        // so the wave is already marked one-shot before the link could restart it.
        CHECK(!msynth_effective_loop({500, 10}, 100, true));
        // A 32-bit end wrapping to zero becomes 1: played once.
        CHECK(!msynth_effective_loop({200, 0xffffff38U}, 100, false));
        CHECK(!msynth_effective_loop({200, 0xffffff38U}, 100, true));
        // An end wrapping to 50 below a start past the guard: the region restarts its start at
        // zero (a 50-frame loop); the wave's own verification already pinned it on the end.
        CHECK(same(msynth_effective_loop({500, 0xfffffe3eU}, 100, false), 0, 50));
        CHECK(!msynth_effective_loop({500, 0xfffffe3eU}, 100, true));
        std::cout
            << "MSynth25 effective loops: guard-frame end, start restart, wrapped ends and short loops verified\n";
        return true;
    }
} // namespace
int main(int argc, char** argv)
{
    if (argc < 2)
        return 2;
    const std::string_view test = argv[1];
    return (test == "decode"                 ? decode()
            : test == "malformed"            ? malformed()
            : test == "msynth_loops"         ? msynth_loops()
            : test == "private" && argc == 3 ? private_data(argv[2])
                                             : false)
               ? 0
               : 1;
}
