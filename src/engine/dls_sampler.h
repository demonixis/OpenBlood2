#pragma once
#include "engine/dls_bank.h"
#include <memory>

namespace OpenBlood2
{
    struct DlsEnvelope
    {
        double attack = 0, decay = 0, sustain = 1, release = 0;
    };
    struct DlsVoice
    {
        std::uint64_t id = 0, frames = 0, gate = 0, loops = 0;
        std::shared_ptr<const DlsBank> bank;
        std::size_t instrument = 0, region = 0;
        unsigned key = 0, velocity = 0;
        double cursor = 0, base_step = 0, gain = 0, left = 0, right = 0;
        double pan = 0;               // Constant offset in 0.1-percent units, before CC10 and clamping.
        unsigned pan_destination = 0; // Last constant-pan writer: 2 (integer legacy), 4 (16.16 DLS), or none.
        bool legacy_pan = false;      // The selected graph contained a validated legacy pan record.
        double lfo_hz = 5, lfo_delay = .01, lfo_pitch = 0, lfo_gain = 0, envelope_pitch = 0;
        DlsEnvelope amplitude, pitch;
    };
    // A bounded sample voice engine for diagnostic notes, not a MIDI sequencer.
    // Callers supply immutable, decoded banks. Controllers are fixed at MIDI power-on defaults.
    class DlsSampler
    {
      public:
        static constexpr unsigned rate = 48000, max_voices = 32;
        std::uint64_t play(std::shared_ptr<const DlsBank>, std::size_t instrument, unsigned key, unsigned velocity,
                           double gate_seconds, double gain, std::string& error);
        bool release(std::uint64_t id);
        // Adds to an interleaved stereo prefix. All frames advance, including unrendered ones.
        bool advance(std::uint64_t frames, std::span<float> stereo);
        void clear();
        const auto& voices() const
        {
            return voices_;
        }
        std::uint64_t started = 0, completed = 0, rejected = 0;
        std::string last_error;

      private:
        std::uint64_t next_id_ = 1; // Never reused, including across clear().
        std::vector<DlsVoice> voices_;
    };
} // namespace OpenBlood2
