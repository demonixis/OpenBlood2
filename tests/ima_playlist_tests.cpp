#include "engine/ima_playlist.h"
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
    bool selection()
    {
        std::string error;
        std::uint64_t hash = 14695981039346656037ull;
        unsigned requests = 0;
        auto byte = [&](unsigned b)
        {
            hash ^= b;
            hash *= 1099511628211ull;
        };
        auto text = [&](std::string_view s)
        {
            for (unsigned char c : s)
                byte(c);
            byte(0);
        };
        for (unsigned from = 0; from < 256; ++from)
            for (unsigned to = 0; to < 256; ++to)
            {
                MusicLevelRequest r;
                bool ok = CMusic::SelectLevel(from, to, r);
                byte(from);
                byte(to);
                byte(ok);
                if (ok)
                {
                    ++requests;
                    CHECK(r.from == from && r.to == to);
                    text(r.playlist);
                    text(r.transition);
                    byte(r.immediate);
                    byte(r.loop);
                }
                else
                    CHECK(r.playlist.empty() && r.transition.empty());
            }
        CHECK(requests == 13 && hash == 17078631686421749893ull);
        std::cout << "Original level selection: 65536 pairs / " << requests << " requests / FNV " << hash << '\n';
        return true;
    }
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
    ImaSectionProgram section(int duration, int preroll, int stop, int pitch, int tempo)
    {
        ImaSectionProgram s;
        s.duration_ticks = duration;
        s.preroll_ticks = preroll;
        s.last_tick = stop;
        s.tempo = {0, std::int16_t(tempo), 0};
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
    bool composition()
    {
        std::string error;
        std::array sections{section(192, 5, 305, 60, 120), section(96, 20, 70, 61, 60)};
        ImaPlaylistProgram p;
        CHECK(prepare_ima_playlist(sections, p, error));
        CHECK(p.preroll_ticks == 5 && p.duration_ticks == 288 && p.last_tick == 305 && p.events.size() == 164);
        CHECK(p.parts[0].start_tick == 5 && p.parts[1].start_tick == 197 && p.tempos[1].tick == 197);
        CHECK(p.events[80].time == 0 && p.events[81].time == 305 && p.events[82].time == 177 &&
              p.events[162].time == 177 && p.events[163].time == 247);
        ImaClock clock;
        CHECK(clock.prepare(p.tempos, 13, error));
        std::uint64_t end;
        CHECK(clock.sample(p.last_tick, end, error) && end == 51661);
        CHECK(clock.boundaries()[1].time100ns == 5130208);
        auto b = bank();
        DlsSampler full, split;
        CHECK(full.enqueue_ima(b, clock, p.events, .2, error) && split.enqueue_ima(b, clock, p.events, .2, error));
        while (full.sample_clock() <= end + 100)
        {
            std::vector<float> a(1000), c(1000);
            CHECK(full.advance(500, a));
            for (unsigned i = 0; i < 500; i += std::min(17u, 500 - i))
                CHECK(split.advance(std::min(17u, 500 - i), std::span(c).subspan(i * 2, std::min(17u, 500 - i) * 2)));
            CHECK(a == c);
            for (auto sample : a)
                CHECK(std::isfinite(sample));
        }
        CHECK(full.voices().empty() && full.pending_midi().empty() && full.started == 2 && !full.dispatch_failures);
        CHECK(!prepare_ima_playlist({}, p, error) && p.events.empty());
        auto invalid = sections;
        invalid[1].duration_ticks = 0;
        CHECK(!prepare_ima_playlist(invalid, p, error) && p.events.empty());
        invalid = sections;
        invalid[1].tempo.whole = 0;
        CHECK(!prepare_ima_playlist(invalid, p, error) && p.events.empty());
        invalid = sections;
        invalid[1].events.back().time = INT32_MAX;
        CHECK(!prepare_ima_playlist(invalid, p, error));
        invalid = sections;
        invalid[0].duration_ticks = INT32_MAX;
        CHECK(!prepare_ima_playlist(invalid, p, error));
        std::vector<ImaSectionProgram> too_many(65, sections[0]);
        CHECK(!prepare_ima_playlist(too_many, p, error));
        invalid = sections;
        invalid[1].events.resize(DlsSampler::max_pending_midi);
        CHECK(!prepare_ima_playlist(invalid, p, error));
        return true;
    }
    bool private_data(const std::filesystem::path& root)
    {
        std::string error;
        MusicCatalog catalog;
        // Deliberate authored duplicate plus case variant: playlist order is not the deduplicated asset list.
        catalog.inspect(root, {{"MusicDirectory", "Music/Cabal"},
                               {"InstrumentFiles", "Cabal.dls cabinit.sty"},
                               {"AmbientList", "a2.sec A2.SEC a3.sec"},
                               {"CruisingList", "c2.sec c1.sec"},
                               {"HarddrivingList", "h1.sec h2.sec"}});
        CHECK(catalog.error.empty());
        unsigned played = 0;
        std::size_t notes = 0, events = 0;
        std::uint64_t rendered = 0;
        for (unsigned from = 0; from < 4; ++from)
            for (unsigned to = 0; to < 4; ++to)
            {
                MusicLevelRequest r;
                DlsSampler sampler;
                if (!CMusic::SelectLevel(from, to, r))
                {
                    CHECK(!catalog.play_level_once(from, to, sampler));
                    continue;
                }
                if (!catalog.play_level_once(from, to, sampler))
                {
                    error = std::to_string(from) + " -> " + std::to_string(to) + ": " + catalog.audition_error;
                    CHECK(false);
                }
                CHECK(catalog.playlist_request && catalog.playlist_request->playlist == r.playlist);
                const auto& files = catalog.playlist_files;
                const auto start = r.transition.empty() ? 0u : 1u;
                if (start)
                    CHECK(files.front() == r.transition);
                if (to == 1)
                    CHECK(files[start] == "a2.sec" && files[start + 1] == "A2.SEC" && files[start + 2] == "a3.sec");
                CHECK(!catalog.play_level_once(0, 1, sampler));
                std::size_t expected = 0;
                for (const auto& p : catalog.playlist_audition.parts)
                    expected += p.notes;
                while (sampler.sample_clock() <= catalog.audition_end + 4 * DlsSampler::rate)
                {
                    std::vector<float> pcm(from == 0 && to == 3 ? 1920 : 0);
                    CHECK(sampler.advance(960, pcm));
                    for (auto value : pcm)
                    {
                        CHECK(std::isfinite(value));
                        if (value != 0)
                            ++rendered;
                    }
                }
                for (const auto& v : sampler.voices())
                    CHECK(v.gate != UINT64_MAX && v.amplitude.release <= 20);
                CHECK(sampler.advance(21 * DlsSampler::rate, {}));
                CHECK(sampler.pending_midi().empty() && sampler.voices().empty() && !sampler.dispatch_failures &&
                      sampler.started == expected);
                notes += expected;
                events += catalog.playlist_audition.events.size();
                ++played;
            }
        CHECK(played == 13 && notes > 0 && rendered > 0);
        // Late missing section: no partially queued transition, patch latch or report overwrite.
        DlsSampler sampler;
        CHECK(catalog.play_level_once(0, 1, sampler));
        const auto report = catalog.playlist_files;
        sampler.silence();
        auto clock = sampler.sample_clock();
        auto admitted = sampler.queued_messages;
        catalog.plan.requests.push_back({"AmbientList", "missing.sec", MusicAssetKind::Section});
        CHECK(!catalog.play_level_once(0, 1, sampler));
        CHECK(catalog.playlist_files == report && sampler.pending_midi().empty() &&
              sampler.queued_messages == admitted && sampler.sample_clock() == clock);
        catalog.plan.requests.pop_back();
        // No finite pass for an empty requested list, even when the transition itself exists.
        std::erase_if(catalog.plan.requests, [](const auto& e) { return e.role == "AmbientList"; });
        CHECK(!catalog.play_level_once(0, 1, sampler) && sampler.pending_midi().empty());
        // Queue retains its bank after the catalog is destroyed, then cancellation releases it.
        CHECK(catalog.play_level_once(0, 3, sampler));
        std::weak_ptr<const DlsBank> lifetime;
        for (const auto& q : sampler.pending_midi())
            if (q.bank)
            {
                lifetime = q.bank;
                break;
            }
        catalog = {};
        CHECK(!lifetime.expired());
        sampler.silence();
        CHECK(lifetime.expired());
        CHECK(!catalog.playlist_request && catalog.playlist_files.empty() && catalog.playlist_audition.events.empty());
        std::cout << "Finite level auditions " << played << " / notes " << notes << " / events " << events
                  << " / nonzero PCM " << rendered << '\n';
        return true;
    }
} // namespace
int main(int argc, char** argv)
{
    std::string mode = argc > 1 ? argv[1] : "";
    return (mode == "selection"              ? selection()
            : mode == "composition"          ? composition()
            : mode == "private" && argc == 3 ? private_data(argv[2])
                                             : false)
               ? 0
               : 1;
}
