#include "dls_scalar_reference.h"
#include "engine/audio.h"
#include "engine/dls_sampler.h"
#include "engine/config.h"
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
    bool compare_scalar(DlsSampler& sampler, std::uint64_t frames, std::vector<float>& output)
    {
        std::string error;
        auto expected = sampler.voices();
        auto pcm = output;
        const auto completed = sampler.completed + ScalarReference::advance(expected, frames, pcm);
        const auto clock = sampler.sample_clock();
        CHECK(sampler.advance(frames, output));
        CHECK(output == pcm && sampler.completed == completed && sampler.sample_clock() == clock + frames);
        CHECK(sampler.voices().size() == expected.size());
        for (std::size_t i = 0; i < expected.size(); ++i)
        {
            const auto& a = sampler.voices()[i];
            const auto& b = expected[i];
            CHECK(a.id == b.id && a.cursor == b.cursor && a.loops == b.loops && a.frames == b.frames &&
                  a.gate == b.gate);
        }
        return true;
    }
    bool scalar_equivalence()
    {
        std::string error;
        for (unsigned mask = 0; mask < 8; ++mask)
            for (bool loop : {false, true})
            {
                auto bank = fixture();
                bank->waves[0].samples.resize(8192);
                for (unsigned i = 0; i < 8192; ++i)
                    bank->waves[0].samples[i] = float(std::sin(i * .013));
                auto& region = bank->instruments[0].regions[0];
                if (!loop)
                    region.settings.loop.reset();
                else
                    region.settings.loop = DlsLoop{101, 991};
                connection(*bank, 0, 0x206, -12000);
                connection(*bank, 0, 0x207, -2400);
                connection(*bank, 0, 0x209, -2400);
                connection(*bank, 0, 0x20a, 42);
                if (mask & 1)
                    connection(*bank, 1, 3, 71);
                if (mask & 2)
                    connection(*bank, 1, 1, -93);
                if (mask & 4)
                {
                    connection(*bank, 5, 3, -333);
                    connection(*bank, 0, 0x30a, -1200);
                }
                DlsSampler sampler;
                CHECK(sampler.receive(bank, 0x90, 61, 99, .5, error));
                const auto id = sampler.voices()[0].id;
                for (const auto frames : {1u, 478u, 1u, 1u, 67u, 4096u, 800u})
                {
                    std::vector<float> pcm(frames * 2, .125f);
                    CHECK(compare_scalar(sampler, frames, pcm));
                }
                if (!sampler.voices().empty())
                    CHECK(sampler.set_controllers(id, {0, 111, 42, 16383}, error));
                std::vector<float> hidden;
                CHECK(compare_scalar(sampler, 799, hidden));
                CHECK(sampler.receive(bank, 0x90, 61, 81, .5, error)); // Original fast choke of old key.
                std::vector<float> prefix(258, .25f);
                CHECK(compare_scalar(sampler, 4096, prefix));
                CHECK(sampler.receive(bank, 0x80, 61, 0, .5, error));
                CHECK(compare_scalar(sampler, 48000, hidden) && sampler.voices().empty());
            }
        std::cout
            << "Frozen scalar reference: exact PCM/cursor/frame/loop/completion equality; all 8 modulation "
               "combinations, one-shots/loops, delay seam, controllers, muted prefixes, choke and release verified\n";
        return true;
    }
    bool midi_receiver()
    {
        std::string error;
        auto bank = fixture();
        connection(*bank, 0, 0x209, -1200); // half-second release
        DlsSampler sampler;
        auto send = [&](unsigned status, unsigned first, unsigned second)
        { return sampler.receive(bank, std::uint8_t(status), std::uint8_t(first), std::uint8_t(second), 1, error); };
        CHECK(sampler.midi_channels()[9].percussion && !sampler.midi_channels()[4].percussion);
        CHECK(send(0xb4, 7, 91) && send(0xb4, 10, 0) && send(0xb4, 11, 82));
        CHECK(send(0x94, 60, 100) && send(0x95, 60, 100));
        CHECK(sampler.play(bank, 0, 60, 100, 1, 1, error)); // Independent diagnostic voice.
        const auto first_id = sampler.voices()[0].id;
        const auto note_on = *sampler.voices()[0].channel;
        CHECK(sampler.voices()[0].midi_channel == 4 && sampler.voices()[0].controllers.volume == 91);
        CHECK(sampler.advance(480, {}));
        const auto other = sampler.voices()[1], diagnostic = sampler.voices()[2];
        CHECK(send(0xb4, 7, 0) && send(0xe4, 255, 255));
        CHECK(sampler.voices()[0].gain == 0 && sampler.voices()[0].controllers.pitch_bend == 32767);
        CHECK(sampler.voices()[0].channel->controllers == note_on.controllers);
        CHECK(sampler.voices()[1].controllers == other.controllers &&
              sampler.voices()[2].controllers == diagnostic.controllers);
        CHECK(send(0x84, 60, 100) && sampler.voices()[0].gate == 480 && sampler.midi_released == 1);
        CHECK(send(0xb4, 10, 127) && sampler.voices()[0].controllers.pan == 127); // Release tails follow channels.
        CHECK(send(0x84, 60, 1) && sampler.midi_unmatched_stops == 1);
        CHECK(send(0xc4, 99, 0) && sampler.voices()[0].channel->program == 0);
        const auto count = sampler.voices().size();
        CHECK(!send(0x94, 62, 100) && sampler.voices().size() == count); // Missing program.
        CHECK(!send(0xb4, 64, 127) && !send(0xb4, 7, 128) && !send(0x74, 60, 100));
        CHECK(sampler.midi_channels()[4].controllers.volume == 0);
        CHECK(sampler.advance(48000 * 11, {}));
        CHECK(sampler.voices().size() == 1 && sampler.voices()[0].midi_channel == 5);
        CHECK(send(0x95, 60, 0) && sampler.advance(24001, {}) && sampler.voices().empty());
        sampler.clear();
        CHECK(sampler.midi_received == 0 && sampler.last_midi_error.empty());
        CHECK(sampler.midi_channels()[4].controllers == DlsControllers{});
        CHECK(send(0x94, 60, 100) && sampler.voices()[0].id > first_id);
        // Note-off on the start boundary preserves the first sample, like the original clamp.
        CHECK(send(0x84, 60, 0) && sampler.voices()[0].gate == 1);
        sampler.clear();
        bank->instruments[0].regions[0].options = 1; // SELFNONEXCLUSIVE: overlap, newest-held stop only.
        CHECK(send(0x94, 60, 100) && send(0x94, 60, 90) && sampler.advance(480, {}));
        CHECK(sampler.midi_choked == 0 && send(0x84, 60, 100));
        CHECK(sampler.voices()[0].gate == std::numeric_limits<std::uint64_t>::max());
        CHECK(sampler.voices()[1].gate == 480 && send(0x94, 60, 0) && sampler.voices()[0].gate == 480);
        sampler.clear();
        bank->instruments[0].regions[0].options = 0;
        CHECK(send(0x94, 60, 100) && sampler.advance(480, {}));
        CHECK(send(0x84, 60, 100) && sampler.advance(480, {}));
        const auto released = sampler.voices()[0];
        CHECK(send(0x94, 60, 90) && sampler.midi_choked == 1);
        CHECK(sampler.voices()[0].choke_frame == released.frames);
        CHECK(sampler.voices()[0].choke_seconds <= 1.0 / 70 && sampler.voices()[0].cursor == released.cursor);
        std::vector<float> out(1400);
        CHECK(sampler.advance(700, out) && sampler.voices().size() == 1);
        CHECK(std::all_of(out.begin(), out.end(), [](float value) { return std::isfinite(value); }));
        CHECK(sampler.midi_released == 1 && send(0x84, 60, 100));
        CHECK(sampler.advance(24001, {}) && sampler.voices().empty());
        // A same-key note in another patch still replaces old channel/key voices.
        bank->instruments.push_back(bank->instruments[0]);
        bank->instruments[1].program = 1;
        CHECK(send(0x94, 60, 100) && sampler.advance(480, {}));
        CHECK(send(0xc4, 1, 0) && send(0x94, 60, 100));
        CHECK(sampler.voices().back().instrument == 1 && sampler.voices().front().choke_frame);
        return true;
    }
    // DLS Level 1: an existing, unambiguous instrument with no region for the key sounds nothing.
    // Received notes are counted once per patch/key; auditions and every other lookup fail as before.
    bool uncovered_key()
    {
        std::string error;
        auto bank = fixture();
        auto& regions = bank->instruments[0].regions;
        regions[0].key_low = 0;
        regions[0].key_high = 23;
        regions.push_back(regions[0]);
        regions[1].key_low = 60;
        regions[1].key_high = 127; // Keys 24-59 are a gap.
        std::optional<std::size_t> out;
        bool uncovered = false;
        CHECK(!find_dls_instrument(*bank, {}, 48, out, error, &uncovered) && uncovered && !out);
        CHECK(error == "No DLS region covers this MIDI key");
        CHECK(find_dls_instrument(*bank, {}, 60, out, error, &uncovered) && !uncovered && out == 0);
        DlsMidiChannel missing{0, 0, 13, false, {}};
        CHECK(!find_dls_instrument(*bank, missing, 48, out, error, &uncovered) && !uncovered);
        DlsSampler sampler;
        auto send = [&](unsigned status, unsigned first, unsigned second)
        { return sampler.receive(bank, std::uint8_t(status), std::uint8_t(first), std::uint8_t(second), 1, error); };
        CHECK(send(0x90, 48, 100) && sampler.voices().empty() && sampler.midi_uncovered_notes == 1);
        CHECK(sampler.uncovered_notes.size() == 1 && sampler.uncovered_notes[0].key == 48 &&
              sampler.uncovered_notes[0].program == 0 && sampler.uncovered_notes[0].frame == 0);
        CHECK(sampler.advance(100, {}) && send(0x90, 48, 90) && sampler.midi_uncovered_notes == 2);
        CHECK(sampler.uncovered_notes.size() == 1 && send(0x91, 30, 100) && sampler.uncovered_notes.size() == 2);
        CHECK(sampler.uncovered_notes[1].frame == 100 && sampler.midi_rejected == 0 && sampler.last_midi_error.empty());
        CHECK(send(0x80, 48, 0) && sampler.midi_unmatched_stops == 1); // Its stop finds no voice.
        CHECK(send(0x90, 60, 100) && sampler.voices().size() == 1);   // Covered keys still sound.
        // A missing patch, an ambiguous patch and auditions stay explicit failures.
        CHECK(send(0xc2, 13, 0) && !send(0x92, 48, 100) && sampler.midi_rejected == 1);
        CHECK(error == "No DLS instrument for this MIDI patch/key");
        bank->instruments.push_back(bank->instruments[0]);
        CHECK(!send(0x93, 48, 100) && error == "Ambiguous DLS packed patch or invalid instrument");
        bank->instruments.pop_back();
        CHECK(!sampler.play_channel(bank, {}, 48, 100, 1, 1, error) && error == "No DLS region covers this MIDI key");
        CHECK(sampler.midi_uncovered_notes == 3 && sampler.voices().size() == 1);
        sampler.clear();
        CHECK(sampler.midi_uncovered_notes == 0 && sampler.uncovered_notes.empty());
        return true;
    }
    // Regression: Track1's program 13 has regions 0-11, 12-23 and 60-127, and the IMA music of
    // Slaughter House and the Ancient One plays its key 48 about four seconds in. Both worlds must run
    // past that note with no failure, and the note must be recorded as uncovered.
    bool uncovered_private(const std::filesystem::path& root)
    {
        std::string error;
        for (const char* world : {"11_slaughter_house", "25_ancientone"})
        {
            auto game = std::make_unique<GameSession>();
            Config config;
            config.data_dir = root;
            CHECK(game->initialize() && game->resources.mount_game(config, error));
            CHECK(GameSession::start_new_game(game, world, 1, 0, error));
            game->server_shell.Player(game->local_client)->m_damage.m_bGodMode = true;
            for (unsigned step = 0; step < 200; ++step)
                if (!game->step(.05))
                {
                    for (const auto& line : game->step_errors())
                        std::cerr << line << '\n';
                    std::cerr << world << ": step " << step << " failed\n";
                    return false;
                }
            const auto& sampler = game->server.audio.instruments;
            CHECK(sampler.midi_rejected == 0 && sampler.dispatch_failures == 0 && sampler.midi_received > 0);
            CHECK(std::any_of(sampler.uncovered_notes.begin(), sampler.uncovered_notes.end(),
                              [](const auto& n) { return n.program == 13 && n.key == 48 && !n.percussion; }));
            std::cout << world << ": 10 s with " << sampler.midi_received << " MIDI messages, "
                      << sampler.midi_uncovered_notes << " silent uncovered-key notes:";
            for (const auto& n : sampler.uncovered_notes)
                std::cout << " program " << n.program << " key " << n.key;
            std::cout << '\n';
        }
        return true;
    }
    bool midi_limits()
    {
        std::string error;
        auto bank = fixture();
        bank->instruments[0].regions[0].settings.unity_note = 0;
        DlsSampler sampler;
        auto send = [&](unsigned status, unsigned first, unsigned second)
        { return sampler.receive(bank, std::uint8_t(status), std::uint8_t(first), std::uint8_t(second), 1, error); };
        CHECK(send(0x90, 60, 100) && send(0x90, 94, 100) && sampler.advance(10, {}));
        auto before = sampler.voices();
        CHECK(!send(0xe0, 255, 255) && sampler.midi_channels()[0].controllers.pitch_bend == 8192);
        CHECK(sampler.controls_updated == 0 && sampler.voices()[0].controllers == before[0].controllers &&
              sampler.voices()[1].controllers == before[1].controllers);
        CHECK(sampler.voices()[0].cursor == before[0].cursor && sampler.voices()[1].gate == before[1].gate);
        CHECK(send(0xb0, 0, 3) && send(0xb0, 32, 4));
        CHECK(sampler.midi_channels()[0].bank_msb == 3 && sampler.midi_channels()[0].bank_lsb == 4);
        CHECK(!send(0x90, 60, 100) && !sampler.voices()[0].choke_frame);
        sampler.clear();
        bank->instruments[0].regions[0].settings.unity_note = 60;
        for (unsigned i = 0; i < DlsSampler::max_voices; ++i)
            CHECK(send(0x90, i, 100));
        const auto id = sampler.voices()[0].id;
        CHECK(!send(0x90, 0, 100) && sampler.voices().size() == DlsSampler::max_voices);
        CHECK(sampler.voices()[0].id == id && !sampler.voices()[0].choke_frame);
        CHECK(sampler.midi_rejected == 1);
        sampler.clear();
        before.clear(); // Owned inspection snapshots also retain their immutable bank.
        std::weak_ptr<const DlsBank> weak = bank;
        CHECK(send(0x90, 60, 100));
        bank.reset();
        CHECK(!weak.expired() && send(0x80, 60, 0)); // Stop needs no bank.
        CHECK(sampler.advance(2, {}) && weak.expired());
        CHECK(send(0x80, 60, 0) && sampler.midi_unmatched_stops == 1);
        return true;
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
        // An MSynth25 effective loop may end on the guard frame past the wave: it plays the
        // loop start sample, then wraps after frames + 1 positions (Crudux program 2).
        auto guard = fixture();
        guard->instruments[0].regions[0].settings.loop = DlsLoop{0, 5};
        CHECK(sampler.play(guard, 0, 60, 127, 1, 1, error));
        out.assign(22, 0);
        auto reference = out;
        CHECK(compare_scalar(sampler, 11, out));
        const double guarded[] = {0, 1, -1, .5, 0, 0, 1, -1, .5, 0, 0};
        for (unsigned f = 0; f < 11; ++f)
            CHECK(near(out[2 * f], guarded[f] * gain));
        CHECK(sampler.voices()[0].loops == 2 && near(sampler.voices()[0].cursor, 1));
        sampler.clear();
        guard->instruments[0].regions[0].settings.loop = DlsLoop{0, 6};
        CHECK(!sampler.play(guard, 0, 60, 127, 1, 1, error)); // Nothing past the guard frame.
        std::cout << "PCM interpolation, loop seam/tail exclusion, guard-frame loops, tuning, velocity, gain, pan and "
                     "one-shot completion verified\n";
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
    bool velocity_default()
    {
        std::string error;
        auto implicit = fixture(), explicit_default = fixture();
        explicit_default->instruments[0].articulation = {true, {{2, 0, 1, 1, -960 * 65536}}};
        for (unsigned velocity : {1, 32, 64, 100, 127})
        {
            DlsSampler a, b;
            CHECK(a.play(implicit, 0, 60, velocity, 1, 1, error));
            CHECK(b.play(explicit_default, 0, 60, velocity, 1, 1, error));
            std::vector<float> pa(960), pb(960);
            CHECK(a.advance(480, pa) && b.advance(480, pb) && pa == pb);
            CHECK(near(b.voices()[0].nominal_gain, std::pow(velocity / 127.0, 2)));
        }
        DlsSampler sampler;
        auto bad = fixture();
        for (auto edge : {DlsConnection{2, 0, 1, 1, -959 * 65536}, DlsConnection{2, 1, 1, 1, -960 * 65536},
                          DlsConnection{2, 0, 1, 2, -960 * 65536}})
        {
            bad->instruments[0].articulation = {true, {edge}};
            CHECK(!sampler.play(bad, 0, 60, 100, 1, 1, error) && sampler.voices().empty());
        }
        bad->instruments[0].articulation = {true, {{2, 0, 1, 1, -960 * 65536}, {2, 0, 1, 1, -960 * 65536}}};
        CHECK(!sampler.play(bad, 0, 60, 100, 1, 1, error));
        auto long_release = fixture();
        connection(*long_release, 0, 0x209, 6387); // Authored Track1 release, about 40 s.
        CHECK(sampler.play(long_release, 0, 60, 127, 1, 1, error));
        CHECK(sampler.voices()[0].amplitude.release > 39 && sampler.voices()[0].amplitude.release < 41);
        CHECK(sampler.advance(48000 * 25, {}) && sampler.voices().size() == 1);
        CHECK(sampler.advance(48000 * 20, {}) && sampler.voices().empty());
        long_release->instruments[0].articulation.connections.clear();
        connection(*long_release, 0, 0x209, 7200); // 64 s exceeds bound.
        CHECK(!sampler.play(long_release, 0, 60, 127, 1, 1, error));
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
    bool live_controls()
    {
        std::string error;
        auto bank = fixture();
        bank->waves[0].samples.assign(10000, 1);
        bank->instruments[0].regions[0].settings.loop = DlsLoop{0, 10000};
        connection(*bank, 0, 0x206, -1200); // Half-second attack; controller edits must not restart it.
        connection(*bank, 0, 0x209, -1200);
        DlsSampler sampler;
        DlsMidiChannel channel;
        channel.controllers = {127, 127, 0};
        const auto id = sampler.play_channel(bank, channel, 60, 127, 1, 1, error);
        const auto other = sampler.play(bank, 0, 60, 127, 1, .1, error);
        CHECK(id && other && sampler.advance(12000, {}));
        const auto before = sampler.voices()[0];
        auto control = channel.controllers;
        CHECK(apply_dls_controller(control, 0xb4, 7, 0, error));
        CHECK(apply_dls_controller(control, 0xe4, 0, 0, error));
        CHECK(sampler.set_controllers(id, control, error));
        CHECK(sampler.voices()[0].frames == before.frames && sampler.voices()[0].cursor == before.cursor &&
              sampler.voices()[0].gate == before.gate &&
              sampler.voices()[0].channel->controllers == channel.controllers);
        CHECK(sampler.voices()[0].gain == 0 && sampler.voices()[1].controllers == DlsControllers{});
        CHECK(sampler.release(other) && sampler.advance(24001, {}));
        CHECK(sampler.voices().size() == 1);
        std::vector<float> out(2);
        CHECK(sampler.advance(1, out) && out == std::vector<float>(2));
        CHECK(apply_dls_controller(control, 0xbf, 7, 127, error) &&
              apply_dls_controller(control, 0xbf, 11, 64, error) &&
              apply_dls_controller(control, 0xbf, 10, 127, error) && apply_dls_controller(control, 0xef, 0, 64, error));
        CHECK(sampler.set_controllers(id, control, error));
        CHECK(sampler.advance(1, out) && out[0] == 0 && near(out[1], std::pow(64.0 / 127, 2)));
        const auto cursor = sampler.voices()[0].cursor;
        CHECK(apply_dls_controller(control, 0xe0, 127, 127, error));
        CHECK(sampler.set_controllers(id, control, error) && sampler.advance(1, {}));
        CHECK(near(sampler.voices()[0].cursor - cursor, std::exp2(199.0 / 1200)));
        // Restore gain from its immutable base, including after repeated silent updates.
        for (unsigned i = 0; i < 100; ++i)
        {
            CHECK(sampler.set_controllers(id, {0, 0, 0}, error));
            CHECK(sampler.set_controllers(id, {127, 127, 0}, error));
        }
        CHECK(sampler.voices()[0].gain == 1 && sampler.voices()[0].bend_step == 1);
        auto saved = sampler.voices()[0];
        for (auto bad : {DlsControllers{128, 127, 0}, DlsControllers{127, 128, 0}, DlsControllers{127, 127, 128},
                         DlsControllers{127, 127, 0, 32768}})
            CHECK(!sampler.set_controllers(id, bad, error));
        CHECK(!sampler.set_controllers(0, {}, error) && sampler.controls_rejected == 5 && sampler.rejected == 0);
        CHECK(sampler.voices()[0].controllers == saved.controllers && sampler.voices()[0].frames == saved.frames &&
              sampler.voices()[0].cursor == saved.cursor && sampler.voices()[0].gain == saved.gain);
        for (auto message :
             {std::array<unsigned, 3>{0xb0, 1, 127}, {0xb0, 7, 128}, {0xd0, 0, 20}, {0xa0, 60, 50}, {0x90, 60, 100}})
        {
            const auto prior = control;
            CHECK(!apply_dls_controller(control, message[0], message[1], message[2], error) && control == prior);
        }
        // Original raw byte combination, not seven-bit masking or a fabricated center.
        CHECK(apply_dls_controller(control, 0xe0, 255, 255, error) && control.pitch_bend == 32767);
        CHECK(sampler.set_controllers(id, control, error) && dls_pitch_bend_cents(control.pitch_bend) == 599);
        CHECK(sampler.release(id));
        const auto release_frame = sampler.voices()[0].gate;
        CHECK(sampler.advance(2400, {}) && sampler.set_controllers(id, {127, 127, 0}, error));
        CHECK(sampler.voices()[0].gate == release_frame); // Updating release never reopens a gate.
        CHECK(sampler.advance(24000, {}) && sampler.voices().empty());
        CHECK(!sampler.set_controllers(id, {}, error));
        const auto next = sampler.play(bank, 0, 60, 100, 1, 1, error);
        CHECK(next > other && !sampler.set_controllers(id, {}, error));
        sampler.clear();
        CHECK(!sampler.controls_updated && !sampler.controls_rejected && sampler.last_control_error.empty());
        // A valid controller can exceed this particular voice's total pitch budget.
        auto extreme = fixture();
        extreme->instruments[0].regions[0].settings.unity_note = 0;
        CHECK(sampler.play(extreme, 0, 95, 100, 1, 1, error));
        const auto edge = sampler.voices()[0].id;
        CHECK(!sampler.set_controllers(edge, {127, 127, 0, 0}, error) && sampler.voices()[0].bend_step == 1);
        CHECK(!sampler.play(extreme, 0, 95, 100, 1, 1, error, {127, 127, 0, 16383}));
        // Inaudible sample attenuation cannot be undone by volume edits.
        extreme = fixture();
        extreme->instruments[0].regions[0].settings.attenuation = std::numeric_limits<std::int32_t>::min();
        const auto silent = sampler.play(extreme, 0, 60, 100, 1, 1, error);
        CHECK(silent && sampler.set_controllers(silent, {127, 127, 0}, error) && sampler.voices().back().gain == 0);
        std::cout << "Live controls preserve clocks, note-on snapshots, release and peer voices; zero-gain recovery, "
                     "raw bend, atomic bounds and stale identities verified\n";
        return true;
    }
    bool bend_reference()
    {
        std::string error;
        std::uint64_t hash = 14695981039346656037ull;
        for (unsigned bend = 0; bend < 32768; ++bend)
        {
            const auto value = std::uint32_t(dls_pitch_bend_cents(bend));
            for (unsigned byte = 0; byte < 4; ++byte)
                hash = (hash ^ ((value >> (byte * 8)) & 255)) * 1099511628211ull;
        }
        CHECK(dls_pitch_bend_cents(8191) == -1 && dls_pitch_bend_cents(8192) == 0 && dls_pitch_bend_cents(0) == -200 &&
              dls_pitch_bend_cents(16383) == 199);
        std::cout << "Original 32768 pitch values FNV64 " << hash << '\n';
        CHECK(hash == 10930475445258237893ull);
        return true;
    }
    bool midi_private(const std::filesystem::path& root)
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
                DlsSampler visible, hidden;
                const unsigned slot = instrument.bank & 0x80000000U ? 9 : 4;
                const auto key = std::uint8_t((region.key_low + region.key_high) / 2);
                auto both = [&](unsigned kind, unsigned first, unsigned second)
                {
                    return visible.receive(bank, std::uint8_t(kind | slot), std::uint8_t(first), std::uint8_t(second),
                                           .2, error) &&
                           hidden.receive(bank, std::uint8_t(kind | slot), std::uint8_t(first), std::uint8_t(second),
                                          .2, error);
                };
                CHECK(both(0xb0, 0, (instrument.bank >> 8) & 127) && both(0xb0, 32, instrument.bank & 127));
                CHECK(both(0xc0, instrument.program, 0) && both(0x90, key, 100));
                bool nonzero = false;
                for (unsigned frame = 0; frame < 12; ++frame)
                {
                    if (frame == 2)
                        CHECK(both(0xb0, 7, 0));
                    if (frame == 3)
                        CHECK(both(0xb0, 7, 100));
                    if (frame == 4)
                        CHECK(both(0xe0, 127, 127));
                    if (frame == 8)
                        CHECK(both(0x80, key, 100));
                    std::vector<float> pcm(1600);
                    CHECK(visible.advance(800, pcm) && hidden.advance(800, {}));
                    CHECK(visible.voices().size() == hidden.voices().size());
                    for (std::size_t i = 0; i < visible.voices().size(); ++i)
                        CHECK(visible.voices()[i].cursor == hidden.voices()[i].cursor &&
                              visible.voices()[i].frames == hidden.voices()[i].frames);
                    for (auto value : pcm)
                    {
                        CHECK(std::isfinite(value));
                        nonzero = nonzero || std::abs(value) > 1e-8;
                    }
                }
                CHECK(nonzero && visible.midi_rejected == 0 && visible.midi_received == 8);
                CHECK(visible.advance(48000 * 30, {}) && hidden.advance(48000 * 30, {}));
                CHECK(visible.voices().empty() && hidden.voices().empty());
                ++regions;
            }
        CHECK(regions == 38);
        std::cout
            << "38 Cabal regions: channel note/control/stop, finite PCM, hidden clocks, release cleanup verified\n";
        return true;
    }
    bool private_data(const std::filesystem::path& root, bool live = false)
    {
        std::string error;
        unsigned accepted = 0, audible = 0, legacy = 0, control_updates = 0;
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
                    CHECK(s.voices().size() == hidden.voices().size());
                    if (live && frame < 12 && !s.voices().empty())
                    {
                        DlsControllers c{frame == 2 ? 0u : 100u, frame == 3 ? 0u : 127u, (frame * 17) % 128,
                                         frame % 2 ? 16383u : 8191u};
                        const auto before = s.voices()[0];
                        CHECK(s.set_controllers(id, c, error) &&
                              hidden.set_controllers(hidden.voices()[0].id, c, error));
                        CHECK(s.voices()[0].frames == before.frames && s.voices()[0].cursor == before.cursor);
                        ++control_updates;
                    }
                    std::vector<float> pcm(1600);
                    CHECK(compare_scalar(s, 800, pcm) && hidden.advance(800, {}));
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
        CHECK(!live || control_updates >= 38);
        if (live)
            std::cout << "Private live controller updates " << control_updates << '\n';
        std::cout << "Cabal region notes: " << accepted << " synthesized/nonzero, " << legacy
                  << " with legacy pan; no refused regions; hidden clocks and release cleanup verified\n";
        return true;
    }
} // namespace
int main(int argc, char** argv)
{
    std::string test = argc > 1 ? argv[1] : "";
    const bool ok = test == "scalar_equivalence"          ? scalar_equivalence()
                    : test == "midi_receiver"             ? midi_receiver()
                    : test == "midi_private" && argc == 3 ? midi_private(argv[2])
                    : test == "midi_limits"               ? midi_limits()
                    : test == "uncovered_key"             ? uncovered_key()
                    : test == "uncovered_private" && argc == 3 ? uncovered_private(argv[2])
                    : test == "pcm"                       ? pcm()
                    : test == "velocity_default"          ? velocity_default()
                    : test == "envelopes"                 ? envelopes()
                    : test == "lifetime"                  ? lifetime()
                    : test == "legacy_pan"                ? legacy_pan()
                    : test == "routing"                   ? routing()
                    : test == "live_controls"             ? live_controls()
                    : test == "bend_reference"            ? bend_reference()
                    : test == "live_private" && argc == 3 ? private_data(argv[2], true)
                    : test == "controllers"               ? controllers()
                    : test == "private" && argc == 3      ? private_data(argv[2])
                                                          : false;
    return ok ? 0 : 1;
}
