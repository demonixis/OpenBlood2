#pragma once
// Frozen scalar baseline from src/engine/dls_sampler.cpp at 309ae21.
// Deliberately retains the old modulation work for exact differential checks.
#include "engine/dls_sampler.h"
#include <algorithm>
#include <cmath>
#include <numbers>
namespace OpenBlood2::ScalarReference
{
    inline constexpr auto rate = DlsSampler::rate;
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
            return v.amplitude.release == 0 || time - gate >= held_level(v.amplitude, gate, true) * v.amplitude.release;
        return v.frames > 0 && v.amplitude.sustain == 0 && time >= v.amplitude.attack + v.amplitude.decay;
    }
    std::uint64_t advance(std::vector<DlsVoice>& voices_, std::uint64_t frames, std::span<float> stereo)
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
                    if (r.settings.loop)
                    {
                        b = a + 1;
                        if (b >= std::size_t(r.settings.loop->start) + r.settings.loop->length)
                            b = r.settings.loop->start;
                    }
                    const auto at = [&](std::size_t i)
                    { return i < wave.samples.size() ? wave.samples[i] : wave.samples[r.settings.loop->start]; };
                    const double pcm = at(a) + (at(b) - at(a)) * (v.cursor - a);
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
        return before - voices_.size();
    }
} // namespace OpenBlood2::ScalarReference
