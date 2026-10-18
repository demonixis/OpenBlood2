#include "engine/ima_program.h"
#include "game/music_catalog.h"
#include <algorithm>
#include <cmath>
#include <filesystem>
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
    struct Fixture
    {
        std::shared_ptr<ImaSection> section = std::make_shared<ImaSection>();
        std::shared_ptr<ImaStyle> style = std::make_shared<ImaStyle>();
        std::shared_ptr<ImaTimeline> timeline = std::make_shared<ImaTimeline>();
        std::shared_ptr<ImaHarmony> harmony = std::make_shared<ImaHarmony>();
        ImaBand band;
        ImaCurves curves;
        ImaCursorSeed seed;
        bool prepare(std::string& error)
        {
            section->tempo = 120;
            section->measures = 1;
            section->ticks_per_measure = 768;
            section->ticks_per_beat = 192;
            section->chords.resize(1);
            section->chords[0].chord_pattern = 0x91;
            style->beats_per_measure = 4;
            style->beat_unit = 4;
            ImaPattern p;
            p.flags = 0x204;
            p.length = 192;
            p.timing = {192, 4, 192, 1};
            p.offset = 12;
            p.grids.resize(1);
            p.grids[0].offset = 32;
            ImaNote n;
            n.type = 9;
            n.play_mode = 0x80;
            n.midi_note = 60;
            n.velocity = 100;
            n.duration = 1000;
            n.time_offset = -5;
            n.variations = 0xffff;
            p.grids[0].notes.push_back(n);
            style->patterns.push_back(p);
            style->source.chunks = {{"RIFF", "AAPT", 12, 100, 1}, {"LIST", "AACL", 32, 80, 2}, {"note", "", 44, 20, 3}};
            seed.variations.fill(1);
            for (auto& ch : band.channels)
            {
                ch.volume = 100;
                ch.pan = 64;
            }
            ImaPersonality personality;
            personality.scale_mask = 0xab5ab5;
            return prepare_ima_timeline(*section, *style, *timeline, error) &&
                   prepare_ima_harmony(*section, *timeline, personality, *harmony, error);
        }
        bool compile(ImaSectionProgram& p, std::string& error)
        {
            return prepare_ima_program(section, style, timeline, harmony, band, curves, seed, 1, p, error);
        }
    };
    std::shared_ptr<DlsBank> bank()
    {
        auto b = std::make_shared<DlsBank>();
        b->waves.push_back({0, 48000, {.5f, 1, -.5f, -1}, {}});
        DlsRegion r;
        r.key_high = r.velocity_high = 127;
        r.channel = 1;
        r.options = 1;
        r.settings.unity_note = 60;
        r.settings.loop = DlsLoop{0, 4};
        DlsInstrument i;
        i.regions.push_back(r);
        b->instruments.push_back(i);
        return b;
    }
    bool prepare()
    {
        std::string error;
        Fixture f;
        CHECK(f.prepare(error));
        ImaSectionProgram p;
        CHECK(f.compile(p, error));
        CHECK(p.grids == 4 && p.notes == 4 && p.terminal_notes == 4 && p.events.size() == 88 && p.preroll_ticks == 5 &&
              p.last_tick == 1576);
        const auto exact_work = p.work_units;
        CHECK(exact_work > 0);
        CHECK(!prepare_ima_program(f.section, f.style, f.timeline, f.harmony, f.band, f.curves, f.seed, 1, p, error,
                                   exact_work - 1) &&
              p.events.empty());
        CHECK(prepare_ima_program(f.section, f.style, f.timeline, f.harmony, f.band, f.curves, f.seed, 1, p, error,
                                  exact_work) &&
              p.work_units == exact_work);
        // Retained partition reverses each grid. Terminal flush preserves that list order, not chronological order.
        CHECK(p.events[80].time == 0 && p.events[80].status == 0x94);
        std::vector<int> stops;
        for (std::size_t i = 84; i < 88; ++i)
        {
            CHECK(p.events[i].status == 0x84);
            stops.push_back(p.events[i].time);
        }
        CHECK(stops == std::vector<int>({1384, 1000, 1192, 1576}));
        ImaSectionProgram again;
        CHECK(f.compile(again, error) && p.events == again.events && p.random_state == again.random_state);
        f.style->patterns[0].grids[0].notes[0].time_range = 11;
        f.style->patterns[0].grids[0].notes[0].velocity_range = 7;
        CHECK(f.compile(p, error) && f.compile(again, error) && p.events == again.events && p.draws == 8);
        std::uint32_t expected = 1;
        for (unsigned i = 0; i < 8; ++i)
            expected = expected * 214013U + 2531011U;
        CHECK(p.random_state == expected);
        f.section->opaque_note_chunks = 1;
        CHECK(!f.compile(p, error) && p.events.empty());
        f.section->opaque_note_chunks = 0;
        f.section->tempo = 0;
        CHECK(!f.compile(p, error) && p.events.empty());
        f.section->tempo = 120;
        f.style->source.chunks[2].size = 38;
        CHECK(!f.compile(p, error) && p.events.empty());
        Fixture large;
        CHECK(large.prepare(error));
        large.section->measures = 100;
        ImaPersonality personality;
        personality.scale_mask = 0xab5ab5;
        CHECK(prepare_ima_timeline(*large.section, *large.style, *large.timeline, error));
        CHECK(prepare_ima_harmony(*large.section, *large.timeline, personality, *large.harmony, error));
        auto repeated = large.style->patterns[0].grids[0].notes.front();
        repeated.duration = 1;
        repeated.time_offset = 0;
        large.style->patterns[0].grids[0].notes.assign(512, repeated);
        large.style->source.chunks[2].size = 2 + 512 * 18;
        CHECK(!large.compile(p, error) && p.events.empty() && error.find("message budget") != error.npos);
        Fixture work;
        CHECK(work.prepare(error));
        work.section->measures = 1000;
        CHECK(prepare_ima_timeline(*work.section, *work.style, *work.timeline, error));
        CHECK(prepare_ima_harmony(*work.section, *work.timeline, personality, *work.harmony, error));
        work.style->source.chunks.resize(8192, {"opaque", "", 100, 0, 3});
        CHECK(!work.compile(p, error) && p.events.empty() && error.find("grid/work budget") != error.npos);
        CHECK(!prepare_ima_program({}, f.style, f.timeline, f.harmony, f.band, f.curves, f.seed, 1, p, error));
        return true;
    }
    bool playback()
    {
        std::string error;
        Fixture f;
        CHECK(f.prepare(error));
        ImaSectionProgram p;
        CHECK(f.compile(p, error));
        auto b = bank();
        DlsSampler full, split;
        ImaClock clock;
        CHECK(clock.prepare(std::span(&p.tempo, 1), 0, error));
        CHECK(full.enqueue_ima(b, clock, p.events, .2, error) && split.enqueue_ima(b, clock, p.events, .2, error));
        // Every output sample matches across arbitrary block boundaries; no UI involved.
        std::uint64_t end;
        CHECK(clock.sample(p.last_tick, end, error));
        std::uint64_t left = end + 100;
        while (left)
        {
            auto n = std::min<std::uint64_t>(left, 500);
            std::vector<float> a(n * 2), c(n * 2);
            CHECK(full.advance(n, a));
            for (std::uint64_t i = 0; i < n; i += std::min<std::uint64_t>(17, n - i))
            {
                auto part = std::min<std::uint64_t>(17, n - i);
                CHECK(split.advance(part, std::span(c).subspan(i * 2, part * 2)));
            }
            CHECK(a == c);
            left -= n;
        }
        CHECK(full.voices().empty() && full.pending_midi().empty() && !full.dispatch_failures &&
              full.midi_released == 4);
        // Cancellation is instrument-only; the monotonic sample clock and diagnostics survive.
        CHECK(clock.prepare(std::span(&p.tempo, 1), full.sample_clock(), error));
        CHECK(full.enqueue_ima(b, clock, p.events, .2, error));
        CHECK(full.advance(1, {}) && !full.voices().empty());
        auto time = full.sample_clock(), messages = full.queued_messages;
        std::weak_ptr<const DlsBank> weak = b;
        b.reset();
        CHECK(!weak.expired());
        full.silence();
        CHECK(weak.expired());
        CHECK(full.sample_clock() == time && full.queued_messages == messages && full.voices().empty() &&
              full.pending_midi().empty());
        CHECK(full.advance(1000, {}) && full.voices().empty());
        full.clear();
        CHECK(full.sample_clock() == 0);
        for (unsigned slot = 0; slot < 16; ++slot)
        {
            CHECK(full.receive({}, std::uint8_t(0xb0 | slot), 11, 0, 1, error));
            CHECK(full.midi_channels()[slot].controllers.expression == 0);
        }
        full.clear();
        for (unsigned slot = 0; slot < 16; ++slot)
        {
            const auto& c = full.midi_channels()[slot];
            CHECK(c.controllers.volume == 100 && c.controllers.expression == 127 && c.controllers.pan == 64 &&
                  c.controllers.pitch_bend == 8192 && c.bank_msb == 0 && c.bank_lsb == 0 && c.program == 0 &&
                  c.percussion == (slot == 9));
        }
        return true;
    }
    bool private_data(const std::filesystem::path& root)
    {
        std::string error;
        MusicCatalog catalog;
        std::string sections_list;
        std::size_t expected_sections = 0;
        for (const auto& file : std::filesystem::directory_iterator(root / "Music/Cabal"))
            if (file.path().extension() == ".sec")
            {
                sections_list += file.path().filename().string() + " ";
                ++expected_sections;
            }
        catalog.inspect(root, {{"MusicDirectory", "Music/Cabal"},
                               {"InstrumentFiles", "Cabal.dls cabinit.sty"},
                               {"AmbientList", sections_list},
                               {"CruisingList", ""},
                               {"HarddrivingList", ""}});
        CHECK(catalog.error.empty());
        std::size_t sections = 0, events = 0, notes = 0, tails = 0;
        std::uint64_t rendered = 0, significant = 0;
        double energy = 0;
        for (const auto& a : catalog.assets)
            if (a.section)
            {
                const auto old_energy = energy;
                DlsSampler sampler;
                for (unsigned slot = 0; slot < 16; ++slot)
                {
                    const auto& c = sampler.midi_channels()[slot];
                    CHECK(c.controllers.volume == 100 && c.controllers.expression == 127 && c.controllers.pan == 64 &&
                          c.controllers.pitch_bend == 8192 && c.bank_msb == 0 && c.bank_lsb == 0 && c.program == 0 &&
                          c.percussion == (slot == 9));
                }

                CHECK(catalog.play_section(a.filename, sampler));
                events += catalog.audition.events.size();
                notes += catalog.audition.notes;
                tails += catalog.audition.terminal_notes;
                CHECK(!catalog.play_section(a.filename, sampler)); // Existing queue cannot be replaced accidentally.
                while (sampler.sample_clock() <= catalog.audition_end + 4 * DlsSampler::rate)
                {
                    std::vector<float> pcm(960 * 2);
                    CHECK(sampler.advance(960, pcm));
                    for (auto sample : pcm)
                    {
                        CHECK(std::isfinite(sample));
                        if (sample != 0)
                            ++rendered;
                        if (std::abs(sample) > 1e-7f)
                            ++significant;
                        energy += double(sample) * sample;
                    }
                }
                for (const auto& v : sampler.voices())
                    CHECK(v.gate != UINT64_MAX && v.amplitude.release <= 20);
                CHECK(sampler.advance(21 * DlsSampler::rate, {}));
                CHECK(sampler.pending_midi().empty() && sampler.voices().empty() && !sampler.dispatch_failures &&
                      sampler.started == catalog.audition.notes);
                CHECK(!catalog.audition.notes || energy > old_energy);
                ++sections;
            }
        std::cout << "Cabal complete sections " << sections << " / events " << events << " / notes " << notes
                  << " / terminal stops " << tails << " / nonzero PCM " << rendered << " / significant PCM "
                  << significant << " / energy " << energy << '\n';
        CHECK(sections == expected_sections && notes > 0 && rendered > 0 && significant > 0 && energy > 0);
        return true;
    }
} // namespace
int main(int argc, char** argv)
{
    std::string mode = argc > 1 ? argv[1] : "";
    return (mode == "prepare"                ? prepare()
            : mode == "playback"             ? playback()
            : mode == "private" && argc == 3 ? private_data(argv[2])
                                             : false)
               ? 0
               : 1;
}
