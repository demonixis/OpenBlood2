#include "engine/dls_sampler.h"
#include "engine/ima_section.h"
#include <algorithm>
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
    bool timing()
    {
        std::string error;
        ImaClock clock;
        std::uint64_t frame = 99;
        CHECK(!clock.sample(0, frame, error) && frame == 0);
        const ImaTempo base[] = {{0, 120, 0}};
        CHECK(clock.prepare(base, 17, error));
        CHECK(clock.sample(192, frame, error) && frame == 24017);
        CHECK(clock.sample(1, frame, error) && frame == 161); // 2.604ms -> 3ms, not 125 samples.
        const ImaTempo half[] = {{0, 125, 0}};
        CHECK(clock.prepare(half, 0, error));
        CHECK(clock.sample(1, frame, error) && frame == 144); // 2.5ms -> 3ms.
        const ImaTempo quantized[] = {{0, 120, 4095}};
        CHECK(clock.prepare(quantized, 0, error) && clock.boundaries()[0].tempo16 == 1920);
        const ImaTempo changed[] = {{0, 120, 4096}};
        CHECK(clock.prepare(changed, 0, error) && clock.boundaries()[0].tempo16 == 1921);
        // Boundary time carries fractions in 100ns, rather than rounding each segment.
        const ImaTempo points[] = {{0, 120, 0}, {1, 125, 0}, {2, 349, 65535}, {2, 120, 0}, {3, 120, 0}};
        CHECK(clock.prepare(points, 0, error) && clock.boundaries().size() == 5);
        CHECK(clock.boundaries()[1].time100ns == 26041 && clock.boundaries()[2].time100ns == 51041);
        CHECK(clock.boundaries()[4].time100ns == 77082);
        CHECK(clock.sample(1, frame, error) && frame == 96);  // A change starts at truncated 2ms.
        CHECK(clock.sample(2, frame, error) && frame == 240); // Last equal-tick tempo.
        CHECK(clock.sample(4, frame, error) && frame == 480);
        for (const auto t :
             {ImaTempo{0, 1, 65535}, ImaTempo{0, 9, 65535}, ImaTempo{0, 350, 1}, ImaTempo{0, 32767, 65535}})
        {
            CHECK(clock.prepare(std::span(&t, 1), 0, error));
            CHECK(clock.boundaries()[0].tempo16 == (t.whole < 10 ? 160U : 5600U));
        }
        return true;
    }
    bool limits()
    {
        std::string error;
        ImaClock clock;
        std::uint64_t frame = 0;
        const ImaTempo good[] = {{0, 120, 0}};
        CHECK(clock.prepare(good, 17, error));
        for (auto bad :
             {std::vector<ImaTempo>{}, std::vector<ImaTempo>{{1, 120, 0}}, std::vector<ImaTempo>{{0, 0, 0}},
              std::vector<ImaTempo>{{0, -1, 0}}, std::vector<ImaTempo>{{0, 120, 0}, {4, 120, 0}, {3, 120, 0}},
              std::vector<ImaTempo>{{0, 120, 0}, {-1, 120, 0}}, std::vector<ImaTempo>{{0, 10, 0}, {INT32_MAX, 120, 0}},
              std::vector<ImaTempo>(ImaClock::max_tempos + 1)})
        {
            CHECK(!clock.prepare(bad, 999, error));
            CHECK(clock.origin_frame() == 17 && clock.boundaries().size() == 1);
            CHECK(clock.sample(192, frame, error) && frame == 24017);
        }
        CHECK(!clock.sample(-1, frame, error) && frame == 0);
        CHECK(!clock.sample(INT32_MAX, frame, error) && frame == 0);
        CHECK(clock.prepare(good, UINT64_MAX - 1, error));
        CHECK(clock.sample(0, frame, error) && frame == UINT64_MAX - 1);
        CHECK(!clock.sample(1, frame, error) && frame == 0);
        const ImaTempo overflow[] = {{0, 120, 0}, {1, 120, 0}};
        CHECK(!clock.prepare(overflow, UINT64_MAX - 1, error));
        return true;
    }
    bool dispatch()
    {
        std::string error;
        ImaClock clock;
        const ImaTempo tempo[] = {{0, 125, 0}};
        CHECK(clock.prepare(tempo, 13, error));
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
        const ImaTimedMidiMessage messages[] = {
            {8, 0x84, 60, 100}, {2, 0x94, 60, 100}, {4, 0xb4, 7, 0}, {6, 0xb4, 7, 100}};
        DlsSampler full, split;
        CHECK(full.enqueue_ima(bank, clock, messages, 1, error) && split.enqueue_ima(bank, clock, messages, 1, error));
        CHECK(full.pending_midi()[0].message.frame == 253 && full.pending_midi().back().message.frame == 973);
        std::vector<float> whole(2048), fragments(2048);
        CHECK(full.advance(1024, whole));
        for (unsigned i = 0; i < 1024; ++i)
            CHECK(split.advance(1, std::span(fragments).subspan(i * 2, 2)));
        CHECK(whole == fragments && full.voices().empty() && full.dispatched_messages == 4);
        for (unsigned i = 0; i < 1024; ++i)
        {
            const bool silent = i < 253 || (i >= 493 && i < 733) || i >= 973;
            CHECK(silent ? whole[i * 2] == 0 && whole[i * 2 + 1] == 0
                         : std::abs(whole[i * 2]) + std::abs(whole[i * 2 + 1]) > 1e-6);
        }
        // Conversion failure, including a late invalid record, cannot latch even a preceding bank select.
        const ImaTimedMidiMessage invalid[] = {{0, 0xb4, 0, 12}, {-1, 0x94, 60, 100}};
        CHECK(!full.enqueue_ima(bank, clock, invalid, 1, error) && full.pending_banks()[4][0] == 0);
        CHECK(full.pending_midi().empty() && full.queued_messages == 4 && full.queue_rejected == 1);
        CHECK(!full.enqueue_ima(bank, clock, messages, 1, error)); // Converted past time.
        CHECK(!full.enqueue_ima(bank, ImaClock{}, {}, 1, error));
        std::vector<ImaTimedMidiMessage> excess(DlsSampler::max_pending_midi + 1);
        CHECK(!full.enqueue_ima(bank, clock, excess, 1, error));
        CHECK(clock.prepare(tempo, full.sample_clock() + 48000, error));
        CHECK(full.enqueue_ima(bank, clock, messages, 1, error));
        std::weak_ptr<const DlsBank> lifetime = bank;
        bank.reset();
        CHECK(!lifetime.expired());
        full.clear();
        split.clear();
        CHECK(lifetime.expired() && full.sample_clock() == 0 && full.pending_midi().empty());
        return true;
    }
    bool reference()
    {
        std::string error;
        std::uint64_t hash = 14695981039346656037ull;
        auto check = [&](std::span<const ImaTempo> tempos, std::span<const int> ticks)
        {
            ImaClock clock;
            CHECK(clock.prepare(tempos, 0, error));
            for (auto tick : ticks)
            {
                std::uint64_t frame = 0;
                CHECK(clock.sample(tick, frame, error));
                for (auto v : {std::uint32_t(tick), std::uint32_t(frame / 48)})
                    for (unsigned i = 0; i < 4; ++i)
                    {
                        hash ^= std::uint8_t(v >> (i * 8));
                        hash *= 1099511628211ull;
                    }
            }
            return true;
        };
        std::vector<int> ticks;
        for (int i = 0; i < 2000; ++i)
            ticks.push_back(i);
        for (auto whole : {1, 9, 10, 120, 121, 349, 350, 32767})
            for (auto fraction : {0, 1023, 4095, 4096, 32768, 65535})
            {
                const ImaTempo tempo{0, std::int16_t(whole), std::uint16_t(fraction)};
                CHECK(check(std::span(&tempo, 1), ticks));
            }
        for (int i = 0; i < 120; ++i)
        {
            const ImaTempo tempos[] = {
                {0, 121, 32768}, {1 + i, 125, 0}, {192 + i, 349, 65535}, {192 + i, 120, 4095}, {1000 + i, 9, 1234}};
            const int query[] = {0, 1, i + 1, 191 + i, 192 + i, 193 + i, 999 + i, 1000 + i, 2000 + i, 10000 + i};
            CHECK(check(tempos, query));
        }
        std::cout << "Original IMRT25 synthetic FNV64 " << hash << '\n';
        CHECK(hash == 624056526007582026ull);
        return true;
    }
    bool private_data(const std::filesystem::path& root)
    {
        std::string error;
        std::error_code ec;
        std::vector<std::filesystem::path> files;
        for (const auto& f : std::filesystem::recursive_directory_iterator(root / "Music", ec))
            if (f.path().extension() == ".sec")
                files.push_back(f.path());
        CHECK(!ec);
        std::sort(files.begin(), files.end());
        std::uint64_t hash = 14695981039346656037ull;
        for (const auto& f : files)
        {
            std::ifstream in(f, std::ios::binary);
            std::vector<std::uint8_t> bytes(std::istreambuf_iterator<char>{in}, {});
            ImaSection section;
            CHECK(read_ima_section(bytes, section, error));
            ImaClock clock;
            const ImaTempo tempo{0, section.tempo, section.tempo_fraction};
            CHECK(clock.prepare(std::span(&tempo, 1), 0, error));
            for (auto tick : {0, 1, 191, 192, 193, int(section.measures) * section.ticks_per_measure})
            {
                std::uint64_t frame = 0;
                CHECK(clock.sample(tick, frame, error));
                for (auto v : {std::uint32_t(tick), std::uint32_t(frame / 48)})
                    for (unsigned i = 0; i < 4; ++i)
                    {
                        hash ^= std::uint8_t(v >> (i * 8));
                        hash *= 1099511628211ull;
                    }
            }
        }
        std::cout << files.size() << " original section clocks / FNV64 " << hash << '\n';
        CHECK(files.size() == 156);
        // Filled from independent original runtime probe, never from this implementation.
        CHECK(hash == 217478193828128853ull);
        return true;
    }
} // namespace
int main(int argc, char** argv)
{
    const std::string name = argc > 1 ? argv[1] : "";
    return (name == "timing"                 ? timing()
            : name == "limits"               ? limits()
            : name == "reference"            ? reference()
            : name == "dispatch"             ? dispatch()
            : name == "private" && argc == 3 ? private_data(argv[2])
                                             : false)
               ? 0
               : 1;
}
