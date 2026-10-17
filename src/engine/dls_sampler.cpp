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
            // DLS equal-power pan: CC10/127 plus the constant offset in 0.1 percent units.
            const double position = std::clamp(64.0 / 127 + voice.pan / 1000, 0.0, 1.0);
            voice.left = std::sqrt(1 - position);
            voice.right = std::sqrt(position);
            return true;
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
            if (time < gate && time < v.amplitude.attack)
                return time / v.amplitude.attack;
            const double env = level(v.amplitude, time, gate, true);
            return env <= 0 ? 0 : std::pow(10.0, (env - 1) * 96 / 20);
        }
        bool finished(const DlsVoice& v)
        {
            const double time = double(v.frames) / DlsSampler::rate;
            const double gate = double(v.gate) / DlsSampler::rate;
            if (time >= gate)
                return v.amplitude.release == 0 ||
                       time - gate >= held_level(v.amplitude, gate, true) * v.amplitude.release;
            return v.frames > 0 && v.amplitude.sustain == 0 && time >= v.amplitude.attack + v.amplitude.decay;
        }
    } // namespace
    std::uint64_t DlsSampler::play(std::shared_ptr<const DlsBank> bank, std::size_t instrument, unsigned key,
                                   unsigned velocity, double gate_seconds, double gain, std::string& error)
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
            gain < 0 || gain > 1)
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
            region.key_group ||
            (sample.loop &&
             (!sample.loop->length || std::uint64_t(sample.loop->start) + sample.loop->length > wave.samples.size())))
            return fail("Unsupported DLS sample, phase link or exclusive key group");
        DlsVoice v;
        v.instrument = instrument;
        v.region = selected;
        v.key = key;
        v.velocity = velocity;
        // DirectMusic DMUS_REGION documents a local articulation, with global fallback only if absent.
        // This also supports the installed banks' melodic per-region art1 records.
        if (!configure(region.articulation.present ? region.articulation : inst.articulation, v, error))
            return fail(error);
        const double cents = (double(key) - sample.unity_note) * 100 + sample.fine_tune;
        if (std::abs(cents) + std::abs(v.lfo_pitch) + std::abs(v.envelope_pitch) > 9600)
            return fail("DLS pitch exceeds the bounded sampler range");
        v.base_step = double(wave.rate) / rate * std::exp2(cents / 1200);
        // DLS 1.1 errata: signed centibel values are gain, capped at unity.
        const double sample_gain =
            sample.attenuation == zero_time ? 0 : std::pow(10.0, std::min(0.0, sample.attenuation / fixed) / 200);
        v.gain = gain * sample_gain * std::pow(velocity / 127.0, 2) * std::pow(100.0 / 127, 2);
        v.gate = std::max<std::uint64_t>(1, std::uint64_t(std::llround(gate_seconds * rate)));
        v.bank = std::move(bank);
        v.id = next_id_++;
        voices_.push_back(std::move(v));
        ++started;
        last_error.clear();
        return voices_.back().id;
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
    bool DlsSampler::advance(std::uint64_t frames, std::span<float> stereo)
    {
        if (frames > rate * 60 + 1 || stereo.size() % 2 || stereo.size() / 2 > frames || stereo.size() > 24000)
            return false;
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
                v.cursor += v.base_step * std::exp2(pitch / 1200);
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
        return true;
    }
    void DlsSampler::clear()
    {
        voices_.clear();
        started = completed = rejected = 0;
        last_error.clear();
    }
} // namespace OpenBlood2
