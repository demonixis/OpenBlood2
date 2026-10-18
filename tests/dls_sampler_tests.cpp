#include "engine/audio.h"
#include "engine/dls_sampler.h"
#include "game/server_adapter.h"
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <numbers>
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
    bool near(double a, double b, double epsilon = 1e-6)
    {
        return std::abs(a - b) < epsilon;
    }
    std::shared_ptr<DlsBank> fixture()
    {
        auto b = std::make_shared<DlsBank>();
        b->waves.push_back({0, 48000, {0, 1, -1, .5f}, {}});
        DlsRegion r;
        r.key_high = r.velocity_high = 127;
        r.channel = 1;
        r.settings.unity_note = 60;
        r.settings.loop = DlsLoop{1, 2};
        DlsInstrument i;
        i.regions.push_back(r);
        b->instruments.push_back(i);
        return b;
    }
    void connection(DlsBank& b, unsigned source, unsigned destination, double value)
    {
        auto& a = b.instruments[0].articulation;
        a.present = true;
        a.connections.push_back(
            {std::uint16_t(source), 0, std::uint16_t(destination), 0, std::int32_t(std::llround(value * 65536))});
    }
    bool pcm()
    {
        std::string error;
        auto bank = fixture();
        DlsSampler sampler;
        const double gain = std::pow(100.0 / 127, 2) * std::sqrt(63.0 / 127);
        CHECK(sampler.play(bank, 0, 60, 127, 1, 1, error));
        std::vector<float> out(12);
        CHECK(sampler.advance(6, out));
        for (unsigned f = 0; f < 6; ++f)
        {
            const double expected = (f == 0 ? 0 : f % 2 ? 1 : -1) * gain;
            CHECK(near(out[2 * f], expected) && near(out[2 * f + 1], expected * std::sqrt(64.0 / 63)));
        }
        CHECK(sampler.voices()[0].loops == 2 && near(sampler.voices()[0].cursor, 2));
        sampler.clear();
        CHECK(sampler.play(bank, 0, 48, 127, 1, 1, error));
        out.assign(12, 0);
        CHECK(sampler.advance(6, out));
        const double half[] = {0, .5, 1, 0, -1, 0}; // Last blend wraps to loop start, never the post-loop tail.
        for (unsigned f = 0; f < 6; ++f)
            CHECK(near(out[2 * f], half[f] * gain));
        sampler.clear();
        auto tuned = fixture();
        tuned->instruments[0].regions[0].settings.fine_tune = 1200;
        tuned->instruments[0].regions[0].settings.attenuation = -200 * 65536; // -20 dB.
        connection(*tuned, 0, 4, -500);
        CHECK(sampler.play(tuned, 0, 60, 64, 1, .5, error));
        out.assign(4, 0);
        CHECK(sampler.advance(2, out));
        const double pan = 64.0 / 127 - .5;
        CHECK(near(out[2], -.1 * .5 * std::pow(64.0 / 127, 2) * std::pow(100.0 / 127, 2) * std::sqrt(1 - pan)));
        CHECK(near(out[3], out[2] * std::sqrt(pan / (1 - pan))));
        sampler.clear();
        tuned->instruments[0].regions[0].settings.attenuation = std::numeric_limits<std::int32_t>::min();
        CHECK(sampler.play(tuned, 0, 60, 127, 1, 1, error));
        out.assign(4, 0);
        CHECK(sampler.advance(2, out));
        CHECK(out == std::vector<float>(4));
        sampler.clear();
        bank->instruments[0].regions[0].settings.loop.reset();
        CHECK(sampler.play(bank, 0, 60, 127, 1, 1, error));
        out.assign(12, 0);
        CHECK(sampler.advance(6, out));
        CHECK(near(out[6], .5 * gain) && out[8] == 0 && sampler.voices().empty() && sampler.completed == 1);
        std::cout << "PCM interpolation, loop seam/tail exclusion, tuning, velocity, gain, pan and one-shot completion "
                     "verified\n";
        return true;
    }
    bool envelopes()
    {
        std::string error;
        auto bank = fixture();
        bank->waves[0].samples.assign(4, 1);
        connection(*bank, 0, 0x206, -1200); // attack .5 s
        connection(*bank, 0, 0x207, 0);     // full-scale decay 1 s
        connection(*bank, 0, 0x20a, 500);   // -48 dB sustain, not half amplitude
        connection(*bank, 0, 0x209, 0);     // full-scale release 1 s
        DlsSampler sampler;
        CHECK(sampler.play(bank, 0, 60, 127, 2, 1, error));
        std::vector<float> one(2);
        const double gain = std::pow(100.0 / 127, 2) * std::sqrt(63.0 / 127);
        CHECK(sampler.advance(12000, {}));
        CHECK(sampler.advance(1, one));
        CHECK(near(one[0], gain * .5));
        CHECK(sampler.advance(35999, {}));
        one.assign(2, 0);
        CHECK(sampler.advance(1, one));
        CHECK(near(one[0], gain * std::pow(10.0, -48.0 / 20)));
        const auto id = sampler.voices()[0].id;
        CHECK(sampler.release(id));
        CHECK(sampler.advance(12000, {}));
        one.assign(2, 0);
        CHECK(sampler.advance(1, one));
        CHECK(near(one[0], gain * std::pow(10.0, -72.0 / 20), 1e-8));
        CHECK(sampler.advance(12000, {}));
        CHECK(sampler.voices().empty() && !sampler.release(id));
        // Release during linear attack starts at its actual gain, not full scale.
        CHECK(sampler.play(bank, 0, 60, 127, 2, 1, error));
        CHECK(sampler.advance(12000, {}));
        CHECK(sampler.release(sampler.voices()[0].id));
        one.assign(2, 0);
        CHECK(sampler.advance(1, one));
        CHECK(near(one[0], gain * .5));
        CHECK(sampler.advance(48000, {}));
        CHECK(sampler.voices().empty());
        auto mod = fixture();
        connection(*mod, 2, 0x206, 1200); // Default absolute zero absorbs velocity scaling.
        connection(*mod, 0, 0x30a, 0);
        connection(*mod, 2, 0x30a, 1200);
        connection(*mod, 0, 0x30b, 0);
        connection(*mod, 3, 0x30b, -1200);
        connection(*mod, 5, 3, 1200);
        CHECK(sampler.play(mod, 0, 64, 64, 2, 1, error));
        const auto& voice = sampler.voices()[0];
        CHECK(voice.amplitude.attack == 0 && near(voice.pitch.attack, std::sqrt(2.0)) &&
              near(voice.pitch.decay, std::pow(2.0, -.5)));
        CHECK(sampler.advance(1000, {}));
        double expected = 0;
        for (unsigned f = 0; f < 1000; ++f)
            expected += std::exp2(400.0 / 1200) * std::exp2((double(f) / 48000) / std::sqrt(2.0));
        CHECK(near(sampler.voices()[0].cursor, 1 + std::fmod(expected - 1, 2), 1e-8));
        sampler.clear();
        auto lfo = fixture();
        connection(*lfo, 1, 3, 100);
        CHECK(sampler.play(lfo, 0, 60, 127, 1, 1, error));
        CHECK(sampler.advance(2000, {}));
        expected = 0;
        for (unsigned f = 0; f < 2000; ++f)
        {
            double t = double(f) / 48000;
            expected += std::exp2((t >= .01 ? 100 * std::sin(2 * std::numbers::pi * 5 * (t - .01)) : 0) / 1200);
        }
        CHECK(near(sampler.voices()[0].cursor, 1 + std::fmod(expected - 1, 2), 1e-8));
        sampler.clear();
        auto transient = fixture();
        transient->waves[0].samples.assign(4, 1);
        connection(*transient, 0, 0x20a, 0);
        CHECK(sampler.play(transient, 0, 60, 127, 1, 1, error));
        std::vector<float> impulse(4);
        CHECK(sampler.advance(2, impulse));
        CHECK(near(impulse[0], gain) && impulse[2] == 0 && sampler.voices().empty());
        sampler.clear();
        auto tremolo = fixture();
        tremolo->waves[0].samples.assign(4, 1);
        connection(*tremolo, 1, 1, -120);
        CHECK(sampler.play(tremolo, 0, 60, 127, 1, 1, error));
        CHECK(sampler.advance(480, {}));
        one.assign(2, 0);
        CHECK(sampler.advance(1, one));
        CHECK(near(one[0], gain)); // The delayed start ramps from zero bias.
        CHECK(sampler.advance(9599, {}));
        one.assign(2, 0);
        CHECK(sampler.advance(1, one));
        CHECK(near(one[0], gain * std::pow(10.0, -.3)));
        std::cout << "Linear amplitude attack, logarithmic decay/release, early note-off, absorbing zero, EG2/LFO "
                     "pitch and /128 scaling verified\n";
        return true;
    }
    bool lifetime()
    {
        std::string error;
        auto bank = fixture();
        DlsSampler a, b;
        auto first = a.play(bank, 0, 60, 127, 1, 1, error);
        CHECK(first && b.play(bank, 0, 60, 127, 1, 1, error));
        std::vector<float> whole(2000), split(1000), merged;
        CHECK(a.advance(1000, whole));
        CHECK(b.advance(500, split));
        merged = split;
        split.assign(1000, 0);
        CHECK(b.advance(500, split));
        merged.insert(merged.end(), split.begin(), split.end());
        CHECK(whole == merged && a.voices()[0].cursor == b.voices()[0].cursor);
        CHECK(!a.advance(1, whole) && a.voices()[0].frames == 1000);
        CHECK(!a.advance(48000 * 61, {}) && !a.advance(1, std::span<float>(whole).first(1)));
        a.clear();
        CHECK(!a.release(first));
        auto second = a.play(bank, 0, 60, 127, 1, 1, error);
        CHECK(second > first);
        for (unsigned i = 1; i < DlsSampler::max_voices; ++i)
            CHECK(a.play(bank, 0, 60, 127, 1, 1, error));
        CHECK(!a.play(bank, 0, 60, 127, 1, 1, error) && a.voices().size() == 32);
        CHECK(a.advance(48000, {}) && a.voices().empty() && a.completed == 32);
        auto malformed = fixture();
        connection(*malformed, 0, 0x208, 6); // A different legacy code still has no supported semantics.
        CHECK(!a.play(malformed, 0, 60, 127, 1, 1, error) && error.find("destination 520") != std::string::npos);
        malformed = fixture();
        connection(*malformed, 0, 4, 0);
        connection(*malformed, 0, 4, 0);
        CHECK(!a.play(malformed, 0, 60, 127, 1, 1, error));
        malformed = fixture();
        malformed->instruments[0].regions.push_back(malformed->instruments[0].regions[0]);
        CHECK(!a.play(malformed, 0, 60, 127, 1, 1, error));
        malformed = fixture();
        malformed->instruments[0].regions[0].key_group = 1;
        CHECK(!a.play(malformed, 0, 60, 127, 1, 1, error));
        CHECK(!a.play(bank, 0, 60, 0, 1, 1, error) && !a.play(bank, 0, 128, 127, 1, 1, error));
        CHECK(!a.play(bank, 0, 60, 127, std::nan(""), 1, error) && !a.play(bank, 0, 60, 127, 1, 2, error));
        auto scoped = fixture();
        connection(*scoped, 0, 0x206, -1200);
        auto id = a.play(scoped, 0, 60, 127, 1, 1, error);
        CHECK(id && near(a.voices()[0].amplitude.attack, .5));
        a.clear();
        scoped->instruments[0].regions[0].articulation.present = true;
        CHECK(a.play(scoped, 0, 60, 127, 1, 1, error));
        CHECK(a.voices()[0].amplitude.attack == 0); // An explicit empty local graph selects defaults.
        std::vector<float> additive(4, .25f);
        CHECK(a.advance(2, additive));
        CHECK(additive[0] == .25f && near(additive[2], .25 + std::pow(100.0 / 127, 2) * std::sqrt(63.0 / 127)));
        a.clear();
        // World ownership releases PCM even if a note was still held. Output transfer never transfers notes.
        GameSession world;
        auto& mixer = world.server.audio;
        AudioMixer destination;
        std::weak_ptr<const DlsBank> weak = bank;
        b.clear();
        CHECK(mixer.instruments.play(bank, 0, 60, 127, 1, 1, error));
        bank.reset();
        CHECK(!weak.expired());
        mixer.inspect_pcm = true;
        CHECK(mixer.advance(.02, {}, {}));
        CHECK(mixer.peak > 0 && mixer.mixed.size() == 1920);
        CHECK(mixer.transfer_output_to(destination, error));
        CHECK(destination.instruments.voices().empty() && mixer.instruments.voices().size() == 1);
        CHECK(world.unload_world());
        CHECK(weak.expired() && mixer.instruments.voices().empty());
        std::cout << "Block partition, muted advance, bounded refusal, stale IDs, shared PCM, output transfer and "
                     "world cleanup verified\n";
        return true;
    }
    bool legacy_pan()
    {
        std::string error;
        const double gain = std::pow(100.0 / 127, 2);
        // Independently check output gains, not just successful graph loading or field equality.
        auto check_pan = [&](std::vector<DlsConnection> connections, double expected, unsigned destination, bool legacy)
        {
            auto bank = fixture();
            bank->waves[0].samples.assign(4, 1);
            bank->instruments[0].articulation = {true, std::move(connections)};
            DlsSampler sampler;
            CHECK(sampler.play(bank, 0, 60, 127, 1, 1, error));
            const auto& voice = sampler.voices()[0];
            CHECK(voice.pan == expected && voice.pan_destination == destination && voice.legacy_pan == legacy);
            std::vector<float> out(2);
            CHECK(sampler.advance(1, out));
            const double p = std::clamp(64.0 / 127 + expected / 1000, 0.0, 1.0);
            CHECK(near(out[0], gain * std::sqrt(1 - p)) && near(out[1], gain * std::sqrt(p)));
            return true;
        };
        for (int pan : {-500, -406, -400, -300, -200, -6, 0, 6, 200, 306, 354, 400, 500})
        {
            const DlsConnection legacy{0, 0, 2, 0, pan}, standard{0, 0, 4, 0, pan * 65536};
            CHECK(check_pan({legacy}, pan, 2, true));
            CHECK(check_pan({standard}, pan, 4, false));
            CHECK(check_pan({legacy, standard}, pan, 4, true)); // Aliases overwrite, never sum.
            CHECK(check_pan({standard, legacy}, pan, 2, true));
        }
        CHECK(check_pan({{0, 0, 2, 0, -500}, {0, 0, 4, 0, 500 * 65536}}, 500, 4, true));
        CHECK(check_pan({{0, 0, 4, 0, 500 * 65536}, {0, 0, 2, 0, -500}}, -500, 2, true));
        CHECK(check_pan({{0, 0, 4, 0, 32768}}, .5, 4, false)); // Existing fractional standard pan remains.
        DlsSampler sampler;
        auto good = fixture();
        const auto first = sampler.play(good, 0, 60, 127, 1, 1, error);
        CHECK(first && sampler.advance(3, {}));
        const double cursor = sampler.voices()[0].cursor;
        for (const DlsConnection bad : {DlsConnection{0, 0, 2, 0, -501},
                                        {0, 0, 2, 0, 501},
                                        {0, 0, 2, 0, 6 * 65536},
                                        {0, 0, 2, 0, std::numeric_limits<std::int32_t>::min()},
                                        {0, 0, 2, 0, std::numeric_limits<std::int32_t>::max()},
                                        {1, 0, 2, 0, 6},
                                        {0, 1, 2, 0, 6},
                                        {0, 0, 2, 1, 6},
                                        {0, 0, 127, 0, 6}})
        {
            auto bank = fixture();
            // A later valid standard pan must not conceal an invalid legacy record.
            bank->instruments[0].articulation = {true, {bad, {0, 0, 4, 0, 0}}};
            CHECK(!sampler.play(bank, 0, 60, 127, 1, 1, error));
            CHECK(sampler.voices().size() == 1 && sampler.voices()[0].id == first &&
                  sampler.voices()[0].cursor == cursor);
        }
        auto bank = fixture();
        bank->instruments[0].articulation = {true, {{0, 0, 2, 0, 6}, {0, 0, 2, 0, 6}}};
        CHECK(!sampler.play(bank, 0, 60, 127, 1, 1, error)); // Identical-edge duplication still fails.
        bank->instruments[0].articulation = {true, {{0, 0, 2, 0, 6}}};
        sampler.clear();
        CHECK(sampler.play(bank, 0, 60, 127, 1, 1, error) && sampler.voices()[0].legacy_pan);
        sampler.clear();
        bank->instruments[0].regions[0].articulation = {true, {}};
        CHECK(sampler.play(bank, 0, 60, 127, 1, 1, error));
        CHECK(!sampler.voices()[0].legacy_pan && sampler.voices()[0].pan_destination == 0);
        sampler.clear();
        bank->instruments[0].regions[0].articulation = {true, {{0, 0, 2, 0, -300}}};
        CHECK(sampler.play(bank, 0, 60, 127, 1, 1, error));
        CHECK(sampler.voices()[0].legacy_pan && sampler.voices()[0].pan == -300);
        std::cout << "Legacy integer/standard fixed pan, signed PCM gains, ordered aliases, invalid graphs, "
                     "failure atomicity and local precedence verified\n";
        return true;
    }
    bool routing()
    {
        std::string error;
        auto bank = fixture();
        bank->instruments[0].bank = 0x1234;
        bank->instruments[0].program = 57;
        CHECK(dls_instrument_patch(bank->instruments[0]) == (0x12U << 14 | 0x34U << 7 | 57U));
        auto drum = bank->instruments[0];
        drum.bank = 0x80000000;
        drum.program = 0;
        drum.regions[0].key_low = 40;
        drum.regions[0].key_high = 50;
        bank->instruments.push_back(drum);
        drum.bank = 0x80000102;
        drum.program = 3;
        drum.regions[0].key_low = 60;
        drum.regions[0].key_high = 70;
        bank->instruments.push_back(drum);
        std::optional<std::size_t> out;
        DlsMidiChannel melodic{0x12, 0x34, 57, false, {90, 80, 50}};
        CHECK(find_dls_instrument(*bank, melodic, 60, out, error) && out == 0);
        DlsMidiChannel drums{1, 2, 3, true, {}};
        CHECK(find_dls_instrument(*bank, drums, 60, out, error) && out == 2);
        CHECK(find_dls_instrument(*bank, drums, 40, out, error) && out == 1);
        CHECK(!find_dls_instrument(*bank, drums, 51, out, error) && !out && !error.empty());
        CHECK(find_dls_instrument(*bank, drums, 128, out, error) && out == 2);
        drums.program = 99;
        CHECK(find_dls_instrument(*bank, drums, 40, out, error) && out == 1);
        drums.percussion = false;
        CHECK(!find_dls_instrument(*bank, drums, 40, out, error) && !out);
        drums.bank_msb = 128;
        CHECK(!find_dls_instrument(*bank, drums, 40, out, error) && !out);
        DlsSampler sampler;
        const auto id = sampler.play_channel(bank, melodic, 60, 100, 1, 1, error);
        CHECK(id && sampler.voices()[0].channel && sampler.voices()[0].instrument == 0);
        melodic.program = 99;
        CHECK(!sampler.play_channel(bank, melodic, 60, 100, 1, 1, error));
        CHECK(sampler.rejected == 1 && sampler.voices().size() == 1 && sampler.voices()[0].id == id);
        CHECK(sampler.voices()[0].channel->program == 57); // Owned, not borrowed mutable channel state.
        CHECK(!sampler.play_channel(bank, {}, 128, 100, 1, 1, error));
        CHECK(!sampler.play_channel({}, {}, 60, 100, 1, 1, error));
        melodic.program = 57;
        sampler.clear();
        CHECK(sampler.play_channel(bank, melodic, 60, 100, 1, 1, error));
        // The supplied channel may belong to a voice in the vector that play() grows.
        for (unsigned i = 0; i < 16; ++i)
        {
            CHECK(sampler.play_channel(bank, *sampler.voices().front().channel, 60, 100, 1, 1, error));
            CHECK(sampler.voices().back().channel->program == 57 &&
                  sampler.voices().back().controllers == melodic.controllers);
        }
        sampler.clear();
        auto ambiguous = bank->instruments[0];
        ambiguous.bank |= 0x80; // Raw locale differs but legacy synth masks this reserved bit.
        bank->instruments.push_back(ambiguous);
        CHECK(!find_dls_instrument(*bank, melodic, 60, out, error) && !out);
        bank->instruments.pop_back();
        // Hash matches the bounded original DLL packing/lookup probe, including ignored bank bits.
        std::uint64_t hash = 14695981039346656037ULL;
        auto mix = [&](std::uint32_t value)
        {
            for (unsigned byte = 0; byte < 4; ++byte)
            {
                hash ^= std::uint8_t(value >> (8 * byte));
                hash *= 1099511628211ULL;
            }
        };
        for (std::uint32_t i = 0; i < 512; ++i)
        {
            DlsInstrument instrument;
            instrument.bank = i * 0x1020305U;
            instrument.program = (i * 37) & 127;
            mix(dls_instrument_patch(instrument));
        }
        for (unsigned percussion = 0; percussion < 2; ++percussion)
            for (unsigned program : {0U, 3U, 99U})
                for (unsigned key : {0U, 39U, 40U, 50U, 51U, 59U, 60U, 70U, 71U, 127U, 128U})
                {
                    DlsMidiChannel c{1, 2, program, bool(percussion), {}};
                    const bool found = find_dls_instrument(*bank, c, key, out, error);
                    CHECK(found == bool(out));
                    mix(out ? std::uint32_t(*out + 1) : 0);
                }
        std::cout << "Original routing fingerprint: " << hash << '\n';
        CHECK(hash == 6386313400906083943ULL);
        return true;
    }
    bool controllers()
    {
        std::string error;
        auto bank = fixture();
        bank->waves[0].samples.assign(4, 1);
        DlsSampler sampler;
        DlsControllers control{127, 127, 0};
        CHECK(sampler.play(bank, 0, 60, 127, 1, 1, error, control));
        std::vector<float> out(2);
        CHECK(sampler.advance(1, out) && out[0] == 1 && out[1] == 0);
        sampler.clear();
        control = {64, 32, 127};
        CHECK(sampler.play(bank, 0, 60, 127, 1, 1, error, control));
        out.assign(2, 0);
        CHECK(sampler.advance(1, out) && out[0] == 0 &&
              near(out[1], std::pow(64.0 / 127, 2) * std::pow(32.0 / 127, 2)));
        CHECK(sampler.voices()[0].controllers == control);
        for (auto invalid : {DlsControllers{128, 127, 64}, DlsControllers{100, 128, 64}, DlsControllers{100, 127, 128}})
            CHECK(!sampler.play(bank, 0, 60, 127, 1, 1, error, invalid));
        CHECK(sampler.voices().size() == 1 && sampler.rejected == 3);
        sampler.clear();
        connection(*bank, 0, 4, 500);
        control = {127, 127, 127};
        CHECK(sampler.play(bank, 0, 60, 127, 1, 1, error, control));
        CHECK(sampler.voices()[0].right == 1 && sampler.voices()[0].left == 0);
        sampler.clear();
        control.volume = 0;
        const auto id = sampler.play(bank, 0, 60, 127, 1, 1, error, control);
        CHECK(id);
        out.assign(2, 0);
        CHECK(sampler.advance(1, out) && out == std::vector<float>(2) && sampler.voices()[0].frames == 1);
        const std::weak_ptr<const DlsBank> weak = bank;
        bank.reset();
        CHECK(!weak.expired() && sampler.release(id));
        CHECK(sampler.advance(48000 * 21, {}) && sampler.voices().empty() && weak.expired());
        std::cout << "Controller PCM endpoints, squared volume/expression, clamping, silence clock, failure atomicity "
                     "and lifetime verified\n";
        return true;
    }
    bool private_data(const std::filesystem::path& root)
    {
        std::string error;
        unsigned accepted = 0, audible = 0, legacy = 0;
        std::ifstream in(root / "Music/Cabal/Cabal.dls", std::ios::binary);
        CHECK(in.good());
        std::vector<std::uint8_t> bytes((std::istreambuf_iterator<char>(in)), {});
        auto bank = std::make_shared<DlsBank>();
        CHECK(read_dls_bank(bytes, *bank, error));
        for (std::size_t i = 0; i < bank->instruments.size(); ++i)
            for (const auto& r : bank->instruments[i].regions)
            {
                DlsSampler s, hidden;
                const unsigned key = (r.key_low + r.key_high) / 2;
                const auto id = s.play(bank, i, key, 100, 1, .2, error);
                CHECK(id);
                ++accepted;
                if (s.voices()[0].legacy_pan)
                {
                    ++legacy;
                    CHECK(s.voices()[0].pan_destination == 4);
                }
                CHECK(hidden.play(bank, i, key, 100, 1, .2, error));
                bool nonzero = false;
                for (unsigned frame = 0; frame < 60; ++frame)
                {
                    std::vector<float> pcm(1600);
                    CHECK(s.advance(800, pcm) && hidden.advance(800, {}));
                    for (float x : pcm)
                    {
                        CHECK(std::isfinite(x) && std::abs(x) <= 1);
                        nonzero = nonzero || std::abs(x) > 1e-8;
                    }
                }
                CHECK(s.voices().size() == hidden.voices().size());
                if (!s.voices().empty())
                    CHECK(s.voices()[0].cursor == hidden.voices()[0].cursor);
                if (nonzero)
                    ++audible;
                CHECK(s.advance(48000 * 30, {}) && s.voices().empty());
            }
        CHECK(accepted == 38 && accepted == audible && legacy == 5);
        std::cout << "Cabal region notes: " << accepted << " synthesized/nonzero, " << legacy
                  << " with legacy pan; no refused regions; hidden clocks and release cleanup verified\n";
        return true;
    }
} // namespace
int main(int argc, char** argv)
{
    std::string test = argc > 1 ? argv[1] : "";
    const bool ok = test == "pcm"                    ? pcm()
                    : test == "envelopes"            ? envelopes()
                    : test == "lifetime"             ? lifetime()
                    : test == "legacy_pan"           ? legacy_pan()
                    : test == "routing"              ? routing()
                    : test == "controllers"          ? controllers()
                    : test == "private" && argc == 3 ? private_data(argv[2])
                                                     : false;
    return ok ? 0 : 1;
}
