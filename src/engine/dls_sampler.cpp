#include "engine/dls_sampler.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <numbers>
#include <set>
#include <tuple>

namespace OpenBlood2
{
    namespace
    {
        constexpr double fixed = 65536.0;
        constexpr auto zero_time = std::numeric_limits<std::int32_t>::min();
        struct EnvelopeSettings
        {
            std::int32_t attack = zero_time, decay = zero_time, release = zero_time;
            double sustain = 1000, velocity_attack = 0, key_decay = 0;
        };
        double time_seconds(std::int32_t nominal, double scale = 0)
        {
            // Absolute zero is absorbing, including when velocity/key scaling exists.
            return nominal == zero_time ? 0 : std::exp2((nominal / fixed + scale) / 1200);
        }
        bool envelope(const EnvelopeSettings& s, unsigned key, unsigned velocity, DlsEnvelope& out)
        {
            out = {time_seconds(s.attack, velocity / 128.0 * s.velocity_attack),
                   time_seconds(s.decay, key / 128.0 * s.key_decay), s.sustain / 1000, time_seconds(s.release)};
            return out.attack <= 20 && out.decay <= 40 && out.release <= 20 && out.sustain >= 0 && out.sustain <= 1;
        }
        bool configure(const DlsArticulation& art, DlsVoice& voice, std::string& error)
        {
            EnvelopeSettings amplitude, pitch;
            std::set<std::tuple<unsigned, unsigned, unsigned, unsigned>> seen;
            for (const auto& c : art.connections)
            {
                if (!seen.emplace(c.source, c.control, c.destination, c.transform).second)
                {
                    error = "Duplicate DLS articulation connection";
                    return false;
                }
                bool supported = c.control == 0 && c.transform == 0;
                const double value = c.scale / fixed;
                EnvelopeSettings* env = nullptr;
                unsigned field = 0;
                switch (c.destination)
                {
                    case 0x206:
                        env = &amplitude;
                        field = 0;
                        break;
                    case 0x207:
                        env = &amplitude;
                        field = 1;
                        break;
                    case 0x20a:
                        env = &amplitude;
                        field = 2;
                        break;
                    case 0x209:
                        env = &amplitude;
                        field = 3;
                        break;
                    case 0x30a:
                        env = &pitch;
                        field = 0;
                        break;
                    case 0x30b:
                        env = &pitch;
                        field = 1;
                        break;
                    case 0x30e:
                        env = &pitch;
                        field = 2;
                        break;
                    case 0x30d:
                        env = &pitch;
                        field = 3;
                        break;
                    default:
                        break;
                }
                if (env && supported)
                {
                    if (c.source == 0)
                    {
                        if (field == 0)
                            env->attack = c.scale;
                        if (field == 1)
                            env->decay = c.scale;
                        if (field == 2)
                            env->sustain = value;
                        if (field == 3)
                            env->release = c.scale;
                    }
                    else if (c.source == 2 && field == 0 && c.scale != zero_time)
                        env->velocity_attack = value;
                    else if (c.source == 3 && field == 1 && c.scale != zero_time)
                        env->key_decay = value;
                    else
                        supported = false;
                }
                else if (supported && c.source == 0 && (c.destination == 2 || c.destination == 4))
                {
                    // Installed MSynth25 uses integer 0.1-percent pan for destination 2,
                    // and 16.16 for destination 4. Both overwrite the same field in file order.
                    // Keep our existing full-precision DLS pan law, not that synth's internal quantization.
                    const double pan = c.destination == 2 ? c.scale : value;
                    if (std::abs(pan) > 500)
                    {
                        error = "DLS constant pan exceeds -500..500";
                        return false;
                    }
                    voice.pan = pan;
                    voice.pan_destination = c.destination;
                    voice.legacy_pan = voice.legacy_pan || c.destination == 2;
                }
                else if (supported && c.source == 0 && c.destination == 0x104)
                    voice.lfo_hz = 440 * std::exp2((value - 6900) / 1200);
                else if (supported && c.source == 0 && c.destination == 0x105)
                    voice.lfo_delay = time_seconds(c.scale);
                else if (supported && c.source == 1 && c.destination == 3)
                    voice.lfo_pitch = value;
                else if (supported && c.source == 1 && c.destination == 1)
                    voice.lfo_gain = value;
                else if (supported && c.source == 5 && c.destination == 3)
                    voice.envelope_pitch = value;
                else
                    supported = false;
                if (!supported)
                {
                    error = "Unsupported DLS connection: source " + std::to_string(c.source) + " control " +
                            std::to_string(c.control) + " destination " + std::to_string(c.destination) +
                            " transform " + std::to_string(c.transform);
                    return false;
                }
            }
            if (!envelope(amplitude, voice.key, voice.velocity, voice.amplitude) ||
                !envelope(pitch, voice.key, voice.velocity, voice.pitch) || voice.lfo_hz < .1 || voice.lfo_hz > 10 ||
                voice.lfo_delay > 10 || std::abs(voice.lfo_pitch) > 1200 || std::abs(voice.envelope_pitch) > 1200 ||
                voice.lfo_gain < -120 || voice.lfo_gain > 0)
            {
                error = "DLS articulation exceeds the supported level-1 parameter range";
                return false;
            }
            return true;
        }
        bool valid_controllers(DlsControllers c)
        {
            return c.volume <= 127 && c.expression <= 127 && c.pan <= 127 && c.pitch_bend <= 32767;
        }
        void set_voice_controllers(DlsVoice& v, DlsControllers c)
        {
            v.controllers = c;
            v.gain = v.nominal_gain * std::pow(c.volume / 127.0, 2) * std::pow(c.expression / 127.0, 2);
            const double position = std::clamp(c.pan / 127.0 + v.pan / 1000, 0.0, 1.0);
            v.left = std::sqrt(1 - position);
            v.right = std::sqrt(position);
            v.bend_step = std::exp2(dls_pitch_bend_cents(c.pitch_bend) / 1200.0);
        }
        double held_level(const DlsEnvelope& e, double time, bool amplitude)
        {
            // DLS errata requires the zero-attack transient at peak on the first sample,
            // even if a zero-length decay immediately selects a lower sustain afterwards.
            if (time == 0 && e.attack == 0)
                return 1;
            if (time < e.attack)
            {
                const double linear = time / e.attack;
                return amplitude ? (linear > 0 ? std::clamp(1 + 20 * std::log10(linear) / 96, 0.0, 1.0) : 0) : linear;
            }
            return e.decay > 0 ? std::max(e.sustain, 1 - (time - e.attack) / e.decay) : e.sustain;
        }
        double level(const DlsEnvelope& e, double time, double gate, bool amplitude)
        {
            if (time < gate)
                return held_level(e, time, amplitude);
            return e.release > 0 ? std::max(0.0, held_level(e, gate, amplitude) - (time - gate) / e.release) : 0;
        }
        double amplitude_at(const DlsVoice& v, double time)
        {
            const double gate = double(v.gate) / DlsSampler::rate;
            if (v.choke_frame && v.frames >= *v.choke_frame)
            {
                const double elapsed = double(v.frames - *v.choke_frame) / DlsSampler::rate;
                const double env = v.choke_seconds > 0 ? v.choke_level * (1 - elapsed / v.choke_seconds) : 0;
                return env <= 0 ? 0 : std::pow(10.0, (env - 1) * 96 / 20);
            }
            if (time < gate && time < v.amplitude.attack)
                return time / v.amplitude.attack;
            const double env = level(v.amplitude, time, gate, true);
            return env <= 0 ? 0 : std::pow(10.0, (env - 1) * 96 / 20);
        }
        bool finished(const DlsVoice& v)
        {
            if (v.choke_frame)
                return v.frames >= *v.choke_frame &&
                       double(v.frames - *v.choke_frame) / DlsSampler::rate >= v.choke_seconds;
            const double time = double(v.frames) / DlsSampler::rate;
            const double gate = double(v.gate) / DlsSampler::rate;
            if (time >= gate)
                return v.amplitude.release == 0 ||
                       time - gate >= held_level(v.amplitude, gate, true) * v.amplitude.release;
            return v.frames > 0 && v.amplitude.sustain == 0 && time >= v.amplitude.attack + v.amplitude.decay;
        }
    } // namespace
    int dls_pitch_bend_cents(unsigned bend)
    {
        const int scaled = (int(bend) - 8192) * 200;
        return scaled >= 0 ? scaled / 8192 : -((-scaled + 8191) / 8192);
    }
    bool apply_dls_controller(DlsControllers& state, std::uint8_t status, std::uint8_t first, std::uint8_t second,
                              std::string& error)
    {
        error.clear();
        auto next = state;
        if ((status & 0xf0) == 0xe0)
            next.pitch_bend = (unsigned(second) << 7) | first;
        else if ((status & 0xf0) == 0xb0 && second <= 127 && (first == 7 || first == 10 || first == 11))
        {
            if (first == 7)
                next.volume = second;
            else if (first == 10)
                next.pan = second;
            else
                next.expression = second;
        }
        else
        {
            error = "Unsupported DLS live controller or out-of-range data";
            return false;
        }
        if (!valid_controllers(next))
        {
            error = "Invalid DLS controller state";
            return false;
        }
        state = next;
        return true;
    }
    std::uint32_t dls_instrument_patch(const DlsInstrument& instrument)
    {
        return instrument.program | ((instrument.bank & 0x7f) << 7) | ((instrument.bank & 0x7f00) << 6) |
               (instrument.bank & 0x80000000);
    }
    bool find_dls_instrument(const DlsBank& bank, const DlsMidiChannel& channel, unsigned key,
                             std::optional<std::size_t>& out, std::string& error)
    {
        out.reset();
        error.clear();
        if (key > 128 || channel.program > 127 || channel.bank_msb > 127 || channel.bank_lsb > 127 ||
            bank.instruments.size() > 256)
        {
            error = "Invalid DLS MIDI route or instrument budget";
            return false;
        }
        const auto patch = (channel.bank_msb << 14) | (channel.bank_lsb << 7) | channel.program |
                           (channel.percussion ? 0x80000000U : 0U);
        for (unsigned pass = 0; pass < (channel.percussion && patch != 0x80000000U ? 2U : 1U); ++pass)
        {
            const auto wanted = pass ? 0x80000000U : patch;
            std::optional<std::size_t> candidate;
            for (std::size_t i = 0; i < bank.instruments.size(); ++i)
            {
                const auto& instrument = bank.instruments[i];
                if (dls_instrument_patch(instrument) != wanted)
                    continue;
                if (candidate || instrument.program > 127 || instrument.regions.size() > 4096)
                {
                    error = "Ambiguous DLS packed patch or invalid instrument";
                    return false;
                }
                candidate = i;
            }
            if (candidate)
            {
                const auto& regions = bank.instruments[*candidate].regions;
                if (key == 128 || std::any_of(regions.begin(), regions.end(), [&](const auto& region)
                                              { return key >= region.key_low && key <= region.key_high; }))
                {
                    out = candidate;
                    return true;
                }
            }
        }
        error = "No DLS instrument for this MIDI patch/key";
        return false;
    }
    std::uint64_t DlsSampler::play_channel(std::shared_ptr<const DlsBank> bank, const DlsMidiChannel& channel,
                                           unsigned key, unsigned velocity, double gate, double gain,
                                           std::string& error)
    {
        std::optional<std::size_t> instrument;
        if (!bank || key > 127 || !find_dls_instrument(*bank, channel, key, instrument, error))
        {
            if (!bank || key > 127)
                error = "Invalid DLS MIDI note/bank";
            last_error = error;
            ++rejected;
            return 0;
        }
        // channel may alias an existing voice; play() can reallocate that vector.
        const auto snapshot = channel;
        const auto id = play(std::move(bank), *instrument, key, velocity, gate, gain, error, snapshot.controllers);
        if (id)
            voices_.back().channel = snapshot;
        return id;
    }
    std::uint64_t DlsSampler::play(std::shared_ptr<const DlsBank> bank, std::size_t instrument, unsigned key,
                                   unsigned velocity, double gate_seconds, double gain, std::string& error,
                                   DlsControllers controllers)
    {
        error.clear();
        auto fail = [&](std::string reason) -> std::uint64_t
        {
            error = last_error = std::move(reason);
            ++rejected;
            return 0;
        };
        if (!bank || instrument >= bank->instruments.size() || key > 127 || velocity == 0 || velocity > 127 ||
            !std::isfinite(gate_seconds) || gate_seconds <= 0 || gate_seconds > 10 || !std::isfinite(gain) ||
            gain < 0 || gain > 1 || !valid_controllers(controllers))
            return fail("Invalid DLS diagnostic note request");
        if (voices_.size() >= max_voices || next_id_ == std::numeric_limits<std::uint64_t>::max())
            return fail("DLS voice/identity limit reached");
        const auto& inst = bank->instruments[instrument];
        std::size_t selected = inst.regions.size();
        for (std::size_t i = 0; i < inst.regions.size(); ++i)
        {
            const auto& r = inst.regions[i];
            if (key < r.key_low || key > r.key_high || velocity < r.velocity_low || velocity > r.velocity_high)
                continue;
            if (selected != inst.regions.size())
                return fail("Overlapping DLS note regions are unsupported");
            selected = i;
        }
        if (selected == inst.regions.size())
            return fail("No DLS region for this key/velocity");
        const auto& region = inst.regions[selected];
        if (region.wave >= bank->waves.size())
            return fail("Invalid DLS region wave");
        const auto& wave = bank->waves[region.wave];
        const auto& sample = region.settings;
        if (wave.samples.empty() || wave.rate < 8000 || wave.rate > 96000 || !sample.loop_in_bounds ||
            sample.unity_note > 127 || region.channel != 1 || region.wave_options || region.phase_group ||
            region.key_group || (region.options & ~1U) ||
            (sample.loop &&
             (!sample.loop->length || std::uint64_t(sample.loop->start) + sample.loop->length > wave.samples.size())))
            return fail("Unsupported DLS sample, phase link or exclusive key group");
        DlsVoice v;
        v.instrument = instrument;
        v.region = selected;
        v.key = key;
        v.velocity = velocity;
        v.controllers = controllers;
        // DirectMusic DMUS_REGION documents a local articulation, with global fallback only if absent.
        // This also supports the installed banks' melodic per-region art1 records.
        if (!configure(region.articulation.present ? region.articulation : inst.articulation, v, error))
            return fail(error);
        const double cents = (double(key) - sample.unity_note) * 100 + sample.fine_tune;
        v.pitch_extent = std::abs(cents) + std::abs(v.lfo_pitch) + std::abs(v.envelope_pitch);
        if (v.pitch_extent + std::abs(dls_pitch_bend_cents(controllers.pitch_bend)) > 9600)
            return fail("DLS pitch exceeds the bounded sampler range");
        v.base_step = double(wave.rate) / rate * std::exp2(cents / 1200);
        // DLS 1.1 errata: signed centibel values are gain, capped at unity.
        const double sample_gain =
            sample.attenuation == zero_time ? 0 : std::pow(10.0, std::min(0.0, sample.attenuation / fixed) / 200);
        v.nominal_gain = gain * sample_gain * std::pow(velocity / 127.0, 2);
        set_voice_controllers(v, controllers);
        v.gate = std::max<std::uint64_t>(1, std::uint64_t(std::llround(gate_seconds * rate)));
        v.bank = std::move(bank);
        v.id = next_id_++;
        voices_.push_back(std::move(v));
        ++started;
        last_error.clear();
        return voices_.back().id;
    }
    bool DlsSampler::receive(std::shared_ptr<const DlsBank> bank, std::uint8_t status, std::uint8_t first,
                             std::uint8_t second, double gain, std::string& error)
    {
        error.clear();
        auto fail = [&](std::string reason)
        {
            error = last_midi_error = std::move(reason);
            ++midi_rejected;
            return false;
        };
        const unsigned slot = status & 15, kind = status & 0xf0;
        auto next = midi_channels_[slot];
        if (kind != 0xe0 && (first > 127 || (kind != 0xc0 && second > 127)))
            return fail("Invalid MIDI data byte");
        if (kind == 0x80 || (kind == 0x90 && second == 0))
        {
            // The original head-inserted list releases the newest still-held matching note only.
            auto it = std::find_if(voices_.rbegin(), voices_.rend(),
                                   [&](const auto& v)
                                   {
                                       return v.midi_channel == slot && v.key == first &&
                                              v.gate == std::numeric_limits<std::uint64_t>::max() && !v.choke_frame;
                                   });
            if (it == voices_.rend())
                ++midi_unmatched_stops;
            else
            {
                // Original NoteOff clamps a stop at/before start to one subsequent sample.
                it->gate = std::max<std::uint64_t>(1, it->frames);
                ++midi_released;
            }
        }
        else if (kind == 0x90)
        {
            // Validate and allocate before changing older voices. Exhaustion/unsupported regions
            // explicitly fail rather than stealing voices or partly replacing existing notes.
            const auto id = play_channel(std::move(bank), next, first, second, 1, gain, error);
            if (!id)
                return fail(error);
            auto& created = voices_.back();
            created.midi_channel = slot;
            created.gate = std::numeric_limits<std::uint64_t>::max();
            const auto& region = created.bank->instruments[created.instrument].regions[created.region];
            if (!(region.options & 1))
                for (auto& v : voices_)
                    if (v.id != id && v.midi_channel == slot && v.key == first && !v.choke_frame)
                    {
                        // Original fast stop caps the remaining amplitude release at rate/70.
                        // Retain our full-precision DLS envelope, rebased continuously at this boundary.
                        const auto start = std::max<std::uint64_t>(1, v.frames);
                        const double t = double(start) / rate;
                        const double env = level(v.amplitude, t, double(v.gate) / rate, true);
                        v.choke_frame = start;
                        v.choke_level = env;
                        v.choke_seconds = std::min(double(rate / 70) / rate, env * v.amplitude.release);
                        v.gate = std::min(v.gate, start);
                        ++midi_choked;
                    }
        }
        else if (kind == 0xc0)
            midi_channels_[slot].program = first;
        else if (kind == 0xb0 && (first == 0 || first == 32))
        {
            if (first == 0)
                midi_channels_[slot].bank_msb = second;
            else
                midi_channels_[slot].bank_lsb = second;
            pending_banks_[slot][first == 0 ? 0 : 1] = second;
        }
        else
        {
            if (!apply_dls_controller(next.controllers, status, first, second, error))
                return fail(error);
            for (const auto& v : voices_)
                if (v.midi_channel == slot &&
                    v.pitch_extent + std::abs(dls_pitch_bend_cents(next.controllers.pitch_bend)) > 9600)
                    return fail("MIDI channel pitch exceeds a sounding voice's budget");
            // Preflight all voices before committing; include release tails and isolate auditions.
            midi_channels_[slot] = next;
            for (auto& v : voices_)
                if (v.midi_channel == slot)
                {
                    set_voice_controllers(v, next.controllers);
                    ++controls_updated;
                }
        }
        ++midi_received;
        last_midi_error.clear();
        return true;
    }
    bool DlsSampler::set_controllers(std::uint64_t id, DlsControllers controllers, std::string& error)
    {
        error.clear();
        const auto it = std::find_if(voices_.begin(), voices_.end(), [&](const auto& v) { return v.id == id; });
        if (!valid_controllers(controllers) || it == voices_.end() ||
            it->pitch_extent + std::abs(dls_pitch_bend_cents(controllers.pitch_bend)) > 9600)
        {
            error = last_control_error = "Invalid/stale DLS voice controls or pitch budget exceeded";
            ++controls_rejected;
            return false;
        }
        set_voice_controllers(*it, controllers);
        ++controls_updated;
        last_control_error.clear();
        return true;
    }
    bool DlsSampler::release(std::uint64_t id)
    {
        for (auto& v : voices_)
            if (v.id == id)
            {
                v.gate = std::min(v.gate, v.frames);
                return true;
            }
        return false;
    }
    void DlsSampler::advance_voices(std::uint64_t frames, std::span<float> stereo)
    {
        for (auto& v : voices_)
        {
            const auto& r = v.bank->instruments[v.instrument].regions[v.region];
            const auto& wave = v.bank->waves[r.wave];
            for (std::uint64_t f = 0; f < frames && !finished(v); ++f)
            {
                if (!r.settings.loop && v.cursor >= wave.samples.size())
                    break;
                const double time = double(v.frames) / rate;
                const double lfo =
                    time >= v.lfo_delay ? std::sin(2 * std::numbers::pi * v.lfo_hz * (time - v.lfo_delay)) : 0;
                if (f < stereo.size() / 2)
                {
                    const auto a = std::size_t(v.cursor);
                    auto b = std::min(a + 1, wave.samples.size() - 1);
                    if (r.settings.loop && a + 1 >= std::size_t(r.settings.loop->start) + r.settings.loop->length)
                        b = r.settings.loop->start;
                    const double pcm = wave.samples[a] + (wave.samples[b] - wave.samples[a]) * (v.cursor - a);
                    // LFO gain is unipolar; its delayed phase has no attenuation.
                    const double lfo_gain =
                        time >= v.lfo_delay ? std::pow(10.0, v.lfo_gain * (lfo + 1) *
                                                                 std::min(1.0, (time - v.lfo_delay) * v.lfo_hz) / 400)
                                            : 1;
                    const double value = pcm * v.gain * amplitude_at(v, time) * lfo_gain;
                    stereo[f * 2] += float(value * v.left);
                    stereo[f * 2 + 1] += float(value * v.right);
                }
                const double pitch =
                    v.envelope_pitch * level(v.pitch, time, double(v.gate) / rate, false) + v.lfo_pitch * lfo;
                v.cursor += v.base_step * v.bend_step * std::exp2(pitch / 1200);
                if (r.settings.loop)
                {
                    const auto loop = *r.settings.loop;
                    if (v.cursor >= double(loop.start) + loop.length)
                    {
                        v.loops += std::uint64_t((v.cursor - loop.start) / loop.length);
                        v.cursor = loop.start + std::fmod(v.cursor - loop.start, loop.length);
                    }
                }
                ++v.frames;
            }
        }
        const auto before = voices_.size();
        std::erase_if(voices_,
                      [](const auto& v)
                      {
                          const auto& r = v.bank->instruments[v.instrument].regions[v.region];
                          return finished(v) || (!r.settings.loop && v.cursor >= v.bank->waves[r.wave].samples.size());
                      });
        completed += before - voices_.size();
    }
    void DlsSampler::discard_midi_from(std::uint64_t frame)
    {
        cancelled_messages +=
            std::erase_if(pending_midi_, [=](const auto& event) { return event.message.frame >= frame; });
    }
    void DlsSampler::silence()
    {
        voices_.clear();
        pending_midi_.clear();
    }
    void DlsSampler::clear()
    {
        silence();
        pending_banks_ = {};
        sample_clock_ = 0;
        cancelled_messages = 0;
        queued_messages = queue_rejected = dispatched_messages = coalesced_messages = dispatch_failures = 0;
        failed_dispatch_frame.reset();
        last_queue_error.clear();
        last_dispatch_error.clear();
        midi_channels_.fill(DlsMidiChannel{});
        midi_channels_[9].percussion = true;
        midi_received = midi_rejected = midi_released = midi_choked = midi_unmatched_stops = 0;
        last_midi_error.clear();
        started = completed = rejected = 0;
        controls_updated = controls_rejected = 0;
        last_control_error.clear();
        last_error.clear();
    }
} // namespace OpenBlood2
