#include "engine/audio.h"
#include "game/music_catalog.h"
#include <algorithm>
#include <cmath>
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
    ImaSectionProgram section(int duration, int preroll, int stop, int pitch, int tempo, unsigned fraction)
    {
        ImaSectionProgram s;
        s.duration_ticks = duration;
        s.preroll_ticks = preroll;
        s.last_tick = stop;
        s.tempo = {0, std::int16_t(tempo), std::uint16_t(fraction)};
        s.notes = 1;
        ImaBand band;
        for (auto& c : band.channels)
        {
            c.volume = 100;
            c.pan = 64;
        }
        std::array<ImaMidiSetup, 80> setup;
        std::string error;
        if (!ima_band_setup(band, setup, error))
            return {};
        for (const auto& m : setup)
            s.events.push_back({0, m.status, m.data1, m.data2});
        s.events.push_back({0, 0x94, std::uint8_t(pitch), 100});
        s.events.push_back({stop, 0x84, std::uint8_t(pitch), 100});
        return s;
    }
    auto sections()
    {
        return std::array{section(193, 5, 750, 60, 137, 39000), section(129, 20, 450, 61, 53, 30000),
                          section(167, 13, 260, 62, 211, 6000)};
    }
    bool timing()
    {
        std::string error;
        const auto parts = sections();
        DlsSampler sampler;
        CHECK(sampler.advance(17, {}));
        ImaLoopTransport loop;
        CHECK(loop.start(parts, 1, bank(), sampler, .2, error));
        std::vector<ImaSectionProgram> unrolled{parts[0]};
        for (unsigned i = 0; i < 20; ++i)
        {
            unrolled.push_back(parts[1]);
            unrolled.push_back(parts[2]);
        }
        ImaPlaylistProgram reference;
        CHECK(prepare_ima_playlist(unrolled, reference, error));
        ImaClock clock;
        CHECK(clock.prepare(reference.tempos, 17, error));
        std::uint64_t a, b;
        for (int tick = 0; tick < reference.duration_ticks; ++tick)
        {
            CHECK(loop.sample(tick, a, error) && clock.sample(tick, b, error) && a == b);
        }
        CHECK(!loop.sample(-1, a, error) && a == 0);
        CHECK(!loop.sample(INT64_MAX, a, error) && a == 0);
        // Preserve fractional boundary time, not rounded samples, across repetitions.
        CHECK(loop.sample(loop.first_end_tick(), a, error));
        CHECK(loop.sample(loop.first_end_tick() + 19 * loop.loop_ticks(), b, error));
        std::cout << "Repeat clock matched " << reference.duration_ticks << " ticks / 20 cycles; frames " << a << ".."
                  << b << '\n';
        return true;
    }
    bool playback()
    {
        std::string error;
        const auto parts = sections();
        AudioMixer whole, split;
        whole.inspect_pcm = split.inspect_pcm = true;
        auto b = bank();
        CHECK(whole.music_loop.start(parts, 1, b, whole.instruments, .2, error));
        CHECK(split.music_loop.start(parts, 1, b, split.instruments, .2, error));
        std::vector<ImaSectionProgram> unrolled{parts[0]};
        for (unsigned i = 0; i < 10; ++i)
        {
            unrolled.push_back(parts[1]);
            unrolled.push_back(parts[2]);
        }
        ImaPlaylistProgram reference;
        CHECK(prepare_ima_playlist(unrolled, reference, error));
        ImaClock clock;
        CHECK(clock.prepare(reference.tempos, 0, error));
        DlsSampler oracle;
        CHECK(oracle.enqueue_ima(b, clock, reference.events, .2, error));
        std::uint64_t end;
        CHECK(
            whole.music_loop.sample(whole.music_loop.first_end_tick() + 7 * whole.music_loop.loop_ticks(), end, error));
        std::size_t peak_queue = 0;
        std::uint64_t nonzero = 0;
        while (whole.instruments.sample_clock() < end)
        {
            const auto frames = unsigned(std::min<std::uint64_t>(499, end - whole.instruments.sample_clock()));
            CHECK(whole.advance(double(frames) / 48000, {}, {}));
            std::vector<float> expected(frames * 2), fragments;
            CHECK(oracle.advance(frames, expected));
            CHECK(whole.mixed == expected);
            for (unsigned pos = 0; pos < frames;)
            {
                const auto count = std::min(17u, frames - pos);
                CHECK(split.advance(double(count) / 48000, {}, {}));
                fragments.insert(fragments.end(), split.mixed.begin(), split.mixed.end());
                pos += count;
            }
            CHECK(fragments == expected);
            for (auto v : expected)
            {
                CHECK(std::isfinite(v));
                nonzero += v != 0;
            }
            peak_queue = std::max(peak_queue, whole.instruments.pending_midi().size());
        }
        CHECK(nonzero && whole.music_loop.passes_submitted() >= 8 && peak_queue < 250);
        CHECK(whole.instruments.started == oracle.started && !whole.instruments.dispatch_failures);
        // Exact-deadline and zero steps do not replay a submitted cycle.
        const auto passes = whole.music_loop.passes_submitted();
        CHECK(whole.advance(0, {}, {}) && whole.music_loop.passes_submitted() == passes);
        const auto frame = whole.instruments.sample_clock();
        whole.music_loop.stop(whole.instruments);
        CHECK(!whole.music_loop.active() && whole.instruments.pending_midi().empty() &&
              whole.instruments.voices().empty());
        CHECK(whole.instruments.sample_clock() == frame && whole.advance(.02, {}, {}));
        CHECK(whole.music_loop.passes_submitted() == passes);
        std::cout << "Repeated PCM equals unrolled reference and 17-frame fragments; nonzero " << nonzero
                  << " / peak queue " << peak_queue << '\n';
        return true;
    }
    bool limits()
    {
        std::string error;
        auto parts = sections();
        AudioMixer audio;
        auto b = bank();
        CHECK(!audio.music_loop.start(parts, 3, b, audio.instruments, .2, error));
        CHECK(!audio.music_loop.start(parts, 0, {}, audio.instruments, .2, error));
        CHECK(!audio.music_loop.start(parts, 0, b, audio.instruments, NAN, error));
        parts[2].tempo.whole = 0;
        CHECK(!audio.music_loop.start(parts, 1, b, audio.instruments, .2, error));
        CHECK(!audio.music_loop.active() && audio.instruments.pending_midi().empty());
        parts = sections();
        CHECK(audio.music_loop.start(parts, 1, b, audio.instruments, .2, error));
        const auto count = audio.instruments.pending_midi().size(), passes = audio.music_loop.passes_submitted();
        CHECK(!audio.music_loop.start(parts, 0, b, audio.instruments, .2, error));
        CHECK(!audio.music_loop.pump(audio.instruments, UINT64_MAX, error));
        CHECK(audio.instruments.pending_midi().size() == count && audio.music_loop.passes_submitted() == passes);
        CHECK(audio.advance(.1, {}, {}));
        CHECK(!audio.music_loop.pump(audio.instruments, 0, error));
        audio.clear();
        CHECK(!audio.music_loop.active() && !audio.music_loop.template_events() && !audio.instruments.sample_clock());
        // Bounded scheduling refuses an unrealistically short/high-density repeat;
        // the mixer stops it once, advances time, and retains an inspectable failure.
        std::array tiny{section(1, 0, 1, 60, 350, 0)};
        CHECK(audio.music_loop.start(tiny, 0, b, audio.instruments, .2, error));
        CHECK(!audio.advance(60, {}, {}));
        CHECK(!audio.music_loop.active() && !audio.music_error.empty() &&
              audio.instruments.sample_clock() == 60 * 48000);
        CHECK(audio.instruments.pending_midi().empty() && audio.instruments.voices().empty());
        const auto failure = audio.music_error;
        CHECK(audio.advance(.02, {}, {}) && audio.music_error == failure);
        audio.clear();
        // A real receiver failure is not successful playback or an indefinitely retried event.
        auto missing = std::make_shared<DlsBank>();
        CHECK(audio.music_loop.start(parts, 1, missing, audio.instruments, .2, error));
        CHECK(!audio.advance(.02, {}, {}));
        CHECK(!audio.music_loop.active() && !audio.music_error.empty() && audio.instruments.dispatch_failures);
        CHECK(audio.instruments.pending_midi().empty() && audio.instruments.voices().empty());
        return true;
    }
    bool lifetime()
    {
        std::string error;
        auto parts = sections();
        AudioMixer audio;
        auto b = bank();
        std::weak_ptr<const DlsBank> weak = b;
        CHECK(audio.music_loop.start(parts, 1, b, audio.instruments, .2, error));
        b.reset();
        CHECK(!weak.expired() && audio.advance(.1, {}, {}));
        audio.music_loop.stop(audio.instruments);
        CHECK(weak.expired());
        audio.clear();
        b = bank();
        weak = b;
        CHECK(audio.music_loop.start(parts, 1, b, audio.instruments, .2, error));
        b.reset();
        audio.clear();
        CHECK(weak.expired() && !audio.music_loop.passes_submitted() && audio.music_error.empty());
        return true;
    }
    bool private_data(const std::filesystem::path& root)
    {
        std::string error;
        MusicCatalog catalog;
        catalog.inspect(root, {{"MusicDirectory", "Music/Cabal"},
                               {"InstrumentFiles", "Cabal.dls cabinit.sty"},
                               {"AmbientList", "a2.sec A2.SEC a3.sec"},
                               {"CruisingList", "c2.sec c1.sec"},
                               {"HarddrivingList", "h1.sec h2.sec"}});
        CHECK(catalog.error.empty());
        std::uint64_t started = 0, nonzero = 0;
        std::size_t peak = 0;
        for (unsigned level = 0; level < 4; ++level)
        {
            AudioMixer audio;
            audio.inspect_pcm = level == 3;
            CHECK(catalog.play_level_loop(0, level, audio));
            CHECK(!catalog.play_level_loop(0, level, audio));
            CHECK(catalog.playlist_files[level ? 1 : 0] == (level == 0   ? "s.sec"
                                                            : level == 1 ? "a2.sec"
                                                            : level == 2 ? "c2.sec"
                                                                         : "h1.sec"));
            const auto notes_intro = level ? catalog.playlist_audition.parts.front().notes : 0;
            std::uint64_t notes_loop = 0;
            for (std::size_t i = level ? 1 : 0; i < catalog.playlist_audition.parts.size(); ++i)
                notes_loop += catalog.playlist_audition.parts[i].notes;
            std::uint64_t end;
            CHECK(audio.music_loop.sample(audio.music_loop.first_end_tick() + 2 * audio.music_loop.loop_ticks(), end,
                                          error));
            while (audio.instruments.sample_clock() < end)
            {
                const auto frames = std::min<std::uint64_t>(960, end - audio.instruments.sample_clock());
                CHECK(audio.advance(double(frames) / 48000, {}, {}));
                peak = std::max(peak, audio.instruments.pending_midi().size());
                for (auto value : audio.mixed)
                {
                    CHECK(std::isfinite(value));
                    nonzero += value != 0;
                }
            }
            CHECK(audio.music_loop.active() && audio.music_loop.passes_submitted() >= 3 &&
                  !audio.instruments.dispatch_failures);
            CHECK(audio.instruments.started >= notes_intro + 3 * notes_loop);
            started += audio.instruments.started;
            const auto templates = audio.music_loop.template_events();
            // Long-duration bounded-state stress uses the authored silent list;
            // voiced lists already render/advance three complete passes above.
            if (level == 0)
                for (unsigned i = 0; i < 30; ++i)
                    CHECK(audio.advance(60, {}, {}));
            CHECK(audio.music_loop.template_events() == templates && audio.instruments.pending_midi().size() < 65536);
            const auto frame = audio.instruments.sample_clock();
            audio.music_loop.stop(audio.instruments);
            CHECK(audio.instruments.sample_clock() == frame && audio.instruments.pending_midi().empty() &&
                  audio.instruments.voices().empty());
            audio.clear();
            CHECK(audio.music_loop.template_events() == 0);
        }
        CHECK(nonzero);
        std::cout << "Cabal 4 looping lists / three passes plus 30 minutes of SilenceList; initial notes " << started
                  << " / nonzero PCM " << nonzero << " / peak queue " << peak << '\n';
        // The running transport, not the catalogue/UI, owns all future bank access.
        AudioMixer detached;
        CHECK(catalog.play_level_loop(0, 3, detached));
        catalog = {};
        CHECK(detached.advance(60, {}, {}) && detached.music_loop.active());
        detached.clear();
        return true;
    }
} // namespace
int main(int argc, char** argv)
{
    if (argc < 2)
        return 2;
    std::string_view mode = argv[1];
    bool ok = mode == "timing"                 ? timing()
              : mode == "playback"             ? playback()
              : mode == "limits"               ? limits()
              : mode == "lifetime"             ? lifetime()
              : mode == "private" && argc == 3 ? private_data(argv[2])
                                               : false;
    return ok ? 0 : 1;
}
