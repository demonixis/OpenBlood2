#include "engine/audio.h"
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
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
    std::shared_ptr<DlsBank> fixture()
    {
        auto bank = std::make_shared<DlsBank>();
        bank->waves.push_back({0, 48000, {.5f, 1, -.5f, -1}, {}});
        DlsRegion region;
        region.key_high = region.velocity_high = 127;
        region.channel = 1;
        region.options = 1;
        region.settings.unity_note = 60;
        region.settings.loop = DlsLoop{0, 4};
        DlsInstrument instrument;
        instrument.regions.push_back(region);
        bank->instruments.push_back(instrument);
        return bank;
    }
    bool ordering()
    {
        std::string error;
        auto bank = fixture();
        auto alternative = bank->instruments[0];
        alternative.bank = 2 << 8;
        alternative.program = 3;
        bank->instruments.push_back(alternative);
        DlsSampler sampler;
        // Note submitted before same-time program/controllers still sees their last value.
        // Bank selects are submission latches, not timestamped patch changes.
        const DlsTimedMidi messages[] = {{9, 0x94, 60, 100},  {20, 0xb4, 0, 2},   {9, 0xc4, 3, 255},
                                         {9, 0xb4, 7, 0},     {9, 0xb4, 7, 127},  {20, 0xb4, 0, 0},
                                         {15, 0xc4, 0, 0},    {10, 0x94, 60, 80}, {10, 0x84, 60, 100},
                                         {16, 0x94, 61, 100}, {19, 0x84, 60, 0},  {19, 0x84, 61, 0}};
        CHECK(sampler.enqueue(bank, messages, 1, error));
        CHECK(sampler.midi_received == 0 && sampler.voices().empty() && sampler.pending_midi().size() == 10);
        CHECK(sampler.pending_banks()[4][0] == 0);
        CHECK(sampler.advance(8, {}) && sampler.voices().empty());
        CHECK(sampler.advance(1, {}) && sampler.voices().size() == 1);
        CHECK(sampler.voices()[0].channel->program == 3 && sampler.voices()[0].channel->bank_msb == 2);
        CHECK(sampler.voices()[0].controllers.volume == 127 && sampler.voices()[0].frames == 0);
        CHECK(sampler.advance(1, {}) && sampler.voices().size() == 2);
        CHECK(sampler.voices()[0].gate == std::numeric_limits<std::uint64_t>::max());
        CHECK(sampler.voices()[1].gate == 1 && sampler.voices()[1].frames == 0);
        CHECK(sampler.advance(6, {}) && sampler.voices().size() == 2);
        CHECK(sampler.voices().back().key == 61 && sampler.voices().back().channel->program == 0);
        CHECK(sampler.voices()[0].channel->program == 3); // Old immutable snapshot.
        CHECK(sampler.advance(4, {}) && sampler.voices().empty());
        CHECK(sampler.sample_clock() == 20 && sampler.pending_midi().empty() && sampler.coalesced_messages == 1);
        CHECK(sampler.queued_messages == 12 && sampler.dispatched_messages == 9 && sampler.midi_released == 3);
        CHECK(!sampler.dispatch_failures);
        // Stable equal-time ordering survives separate batches. Off then on leaves new note held.
        const DlsTimedMidi first[] = {{20, 0x84, 60, 0}, {25, 0xb4, 10, 1}};
        const DlsTimedMidi second[] = {{20, 0x94, 60, 100}, {25, 0xb4, 10, 127}};
        CHECK(sampler.enqueue({}, first, 1, error) && sampler.enqueue(bank, second, 1, error));
        CHECK(sampler.advance(0, {}) && sampler.midi_unmatched_stops == 1 && sampler.voices().size() == 1);
        CHECK(sampler.advance(5, {}) && sampler.voices()[0].controllers.pan == 127);
        CHECK(sampler.coalesced_messages == 2 && sampler.sample_clock() == 25);
        return true;
    }
    bool pcm()
    {
        std::string error;
        auto bank = fixture();
        const DlsTimedMidi messages[] = {{30, 0x84, 60, 0},  {10, 0x94, 60, 100},  {20, 0xb4, 7, 0},
                                         {24, 0xb4, 7, 127}, {24, 0xe4, 255, 255}, {11, 0xb4, 10, 0}};
        DlsSampler full, chunks, hidden;
        CHECK(full.enqueue(bank, messages, 1, error) && chunks.enqueue(bank, messages, 1, error) &&
              hidden.enqueue(bank, messages, 1, error));
        std::vector<float> all(80), split(80);
        CHECK(full.advance(40, all));
        for (unsigned i = 0; i < 40; ++i)
            CHECK(chunks.advance(1, std::span(split).subspan(i * 2, 2)) && hidden.advance(1, {}));
        CHECK(all == split && full.sample_clock() == hidden.sample_clock());
        CHECK(full.voices().empty() && chunks.voices().empty() && hidden.voices().empty());
        for (unsigned i = 0; i < 40; ++i)
        {
            if (i < 10 || (i >= 20 && i < 24) || i >= 30)
                CHECK(all[i * 2] == 0 && all[i * 2 + 1] == 0);
            else
                CHECK(std::abs(all[i * 2]) + std::abs(all[i * 2 + 1]) > 1e-6);
        }
        // A stall only renders the caller's prefix, while all later message boundaries still run.
        DlsSampler stall;
        const DlsTimedMidi future[] = {{20000, 0x94, 60, 100}, {21000, 0x84, 60, 0}};
        CHECK(stall.enqueue(bank, future, 1, error));
        std::vector<float> prefix(24000);
        CHECK(stall.advance(24000, prefix));
        CHECK(stall.started == 1 && stall.completed == 1 && stall.voices().empty() && stall.pending_midi().empty());
        CHECK(std::all_of(prefix.begin(), prefix.end(), [](float v) { return v == 0; }));
        // Fractional AudioMixer timing uses the same clock, independent of PCM inspection.
        AudioMixer mix;
        mix.inspect_pcm = true;
        const DlsTimedMidi near[] = {{1, 0x94, 60, 100}, {3, 0x84, 60, 0}};
        CHECK(mix.instruments.enqueue(bank, near, 1, error));
        CHECK(mix.advance(.5 / 48000, {}, {}) && mix.instruments.voices().empty());
        CHECK(mix.advance(.5 / 48000, {}, {}) && mix.instruments.voices().size() == 1);
        CHECK(mix.advance(2.0 / 48000, {}, {}) && mix.instruments.voices().empty());
        CHECK(mix.instruments.sample_clock() == 3);
        return true;
    }
    bool limits()
    {
        std::string error;
        auto bank = fixture();
        DlsSampler sampler;
        const DlsTimedMidi good[] = {{10, 0x94, 60, 100}, {20, 0x84, 60, 0}};
        CHECK(sampler.enqueue(bank, good, 1, error));
        // A bad tail must not mutate submission latches, old queue, voice counters or time.
        const DlsTimedMidi bad[] = {{10, 0xb4, 0, 7}, {10, 0xc4, 7, 0}, {10, 0xb4, 64, 127}};
        CHECK(!sampler.enqueue(bank, bad, 1, error) && sampler.pending_banks()[4][0] == 0);
        CHECK(sampler.pending_midi().size() == 2 && sampler.queued_messages == 2 && sampler.sample_clock() == 0);
        CHECK(!sampler.enqueue({}, good, 1, error) && !sampler.enqueue(bank, good, NAN, error));
        CHECK(!sampler.advance(48000 * 60 + 2, {}) && sampler.pending_midi().size() == 2);
        CHECK(sampler.advance(10, {}) && sampler.enqueue(bank, good, 1, error)); // Current boundary is allowed.
        const DlsTimedMidi late[] = {{9, 0xb4, 7, 0}};
        CHECK(!sampler.enqueue({}, late, 1, error));
        sampler.clear();
        std::vector<DlsTimedMidi> many(DlsSampler::max_pending_midi, {100, 0xb4, 7, 100});
        CHECK(sampler.enqueue({}, many, 1, error) && sampler.pending_midi().size() == many.size());
        CHECK(!sampler.enqueue(bank, good, 1, error) && sampler.pending_midi().size() == many.size());
        CHECK(sampler.advance(100, {}) && sampler.coalesced_messages == many.size() - 1 &&
              sampler.dispatched_messages == 1);
        // Runtime patch failure is not successful admission/playback. Consume it once and retain history.
        // Program 99 is ambiguous (two instruments share its packed patch); a missing one is silent.
        sampler.clear();
        for (int copy = 0; copy < 2; ++copy)
        {
            bank->instruments.push_back(bank->instruments[0]);
            bank->instruments.back().program = 99;
        }
        const DlsTimedMidi missing[] = {
            {1, 0xc4, 99, 0}, {1, 0x94, 60, 100}, {2, 0xc4, 0, 0}, {2, 0x94, 60, 100}, {4, 0x84, 60, 0}};
        CHECK(sampler.enqueue(bank, missing, 1, error));
        CHECK(!sampler.advance(5, {}) && sampler.sample_clock() == 5 && sampler.pending_midi().empty());
        CHECK(sampler.dispatch_failures == 1 && sampler.failed_dispatch_frame == 1 &&
              !sampler.last_dispatch_error.empty());
        CHECK(sampler.started == 1 && sampler.completed == 1 && sampler.advance(1, {}));
        CHECK(sampler.dispatch_failures == 1 && !sampler.last_dispatch_error.empty());
        AudioMixer mixer;
        mixer.inspect_pcm = true;
        CHECK(mixer.instruments.enqueue(bank, missing, 1, error));
        CHECK(!mixer.advance(5.0 / 48000, {}, {}) && mixer.mixed.empty() && !mixer.output_error.empty());
        return true;
    }
    bool lifetime()
    {
        std::string error;
        auto bank = fixture();
        std::weak_ptr<const DlsBank> weak = bank;
        DlsSampler sampler;
        const DlsTimedMidi events[] = {{10, 0x94, 60, 100}, {20, 0x84, 60, 0}};
        CHECK(sampler.enqueue(bank, events, 1, error));
        bank.reset();
        CHECK(!weak.expired() && sampler.advance(10, {}) && !weak.expired());
        const auto id = sampler.voices()[0].id;
        CHECK(sampler.advance(11, {}) && weak.expired());
        bank = fixture();
        weak = bank;
        const DlsTimedMidi future[] = {{999, 0x94, 60, 100}};
        CHECK(sampler.enqueue(bank, future, 1, error));
        bank.reset();
        sampler.clear();
        CHECK(weak.expired() && sampler.pending_midi().empty() && sampler.sample_clock() == 0);
        CHECK(sampler.queued_messages == 0 && sampler.last_dispatch_error.empty() &&
              sampler.pending_banks()[4][0] == 0);
        bank = fixture();
        CHECK(sampler.enqueue(bank, events, 1, error) && sampler.advance(10, {}) && sampler.voices()[0].id > id);
        return true;
    }
    bool private_data(const std::filesystem::path& root)
    {
        std::string error;
        std::ifstream in(root / "Music/Cabal/Cabal.dls", std::ios::binary);
        CHECK(in.good());
        std::vector<std::uint8_t> bytes((std::istreambuf_iterator<char>(in)), {});
        auto bank = std::make_shared<DlsBank>();
        CHECK(read_dls_bank(bytes, *bank, error));
        unsigned regions = 0;
        for (const auto& instrument : bank->instruments)
            for (const auto& region : instrument.regions)
            {
                const auto slot = std::uint8_t(instrument.bank & 0x80000000U ? 9 : 4);
                const auto key = std::uint8_t((region.key_low + region.key_high) / 2);
                const auto b = std::uint8_t(0xb0 | slot);
                const DlsTimedMidi messages[] = {{0, b, 0, std::uint8_t((instrument.bank >> 8) & 127)},
                                                 {0, b, 32, std::uint8_t(instrument.bank & 127)},
                                                 {0, std::uint8_t(0xc0 | slot), std::uint8_t(instrument.program), 0},
                                                 {8000, std::uint8_t(0x80 | slot), key, 100},
                                                 {101, std::uint8_t(0x90 | slot), key, 100},
                                                 {2000, b, 7, 0},
                                                 {2500, b, 7, 100},
                                                 {3101, std::uint8_t(0xe0 | slot), 127, 127}};
                DlsSampler whole, split, hidden;
                CHECK(whole.enqueue(bank, messages, .2, error) && split.enqueue(bank, messages, .2, error) &&
                      hidden.enqueue(bank, messages, .2, error));
                std::vector<float> pcm(20000), fragments(20000);
                CHECK(whole.advance(10000, pcm));
                for (unsigned pos = 0; pos < 10000;)
                {
                    const auto n = std::min(137U, 10000 - pos);
                    CHECK(split.advance(n, std::span(fragments).subspan(pos * 2, n * 2)) && hidden.advance(n, {}));
                    pos += n;
                }
                CHECK(pcm == fragments && whole.sample_clock() == hidden.sample_clock());
                CHECK(std::all_of(pcm.begin(), pcm.end(), [](float v) { return std::isfinite(v); }));
                CHECK(std::any_of(pcm.begin(), pcm.end(), [](float v) { return std::abs(v) > 1e-8; }));
                CHECK(whole.voices().size() == hidden.voices().size());
                for (std::size_t i = 0; i < whole.voices().size(); ++i)
                    CHECK(whole.voices()[i].frames == hidden.voices()[i].frames &&
                          whole.voices()[i].cursor == hidden.voices()[i].cursor);
                CHECK(whole.advance(48000 * 30, {}) && hidden.advance(48000 * 30, {}));
                CHECK(whole.voices().empty() && hidden.voices().empty() && whole.pending_midi().empty());
                CHECK(whole.dispatched_messages == 6 && whole.dispatch_failures == 0);
                ++regions;
            }
        CHECK(regions == 38);
        std::cout
            << "38 Cabal regions: identical PCM across block sizes, hidden clocks, timed controls/stops and cleanup\n";
        return true;
    }
} // namespace
int main(int argc, char** argv)
{
    const std::string name = argc > 1 ? argv[1] : "";
    return (name == "ordering"               ? ordering()
            : name == "pcm"                  ? pcm()
            : name == "limits"               ? limits()
            : name == "lifetime"             ? lifetime()
            : name == "private" && argc == 3 ? private_data(argv[2])
                                             : false)
               ? 0
               : 1;
}
