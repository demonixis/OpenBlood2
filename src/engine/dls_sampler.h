#pragma once
#include "engine/dls_bank.h"
#include "engine/ima_clock.h"
#include "engine/ima_note_event.h"
#include <array>
#include <memory>
#include <optional>

namespace OpenBlood2
{
    struct DlsEnvelope
    {
        double attack = 0, decay = 0, sustain = 1, release = 0;
    };
    struct DlsControllers
    {
        unsigned volume = 100, expression = 127, pan = 64;
        unsigned pitch_bend = 8192; // Original raw-byte decoder admits 0..32767; normal MIDI uses 0..16383.
        bool operator==(const DlsControllers&) const = default;
    };
    // Installed synth default range: 200 cents, signed floor division. Requires bend <= 32767.
    int dls_pitch_bend_cents(unsigned bend);
    // CC7/10/11 and pitch bend only. Explicit caller-selected voice/channel; low status nibble is not routing.
    // Failure preserves state. Raw bend bytes are not masked to seven bits.
    bool apply_dls_controller(DlsControllers&, std::uint8_t status, std::uint8_t first, std::uint8_t second,
                              std::string& error);
    struct DlsMidiChannel
    {
        unsigned bank_msb = 0, bank_lsb = 0, program = 0;
        bool percussion = false;
        DlsControllers controllers;
    };
    // Original MSynth packed identifier; DLS bank layout differs from IMA packed flags.
    std::uint32_t dls_instrument_patch(const DlsInstrument&);
    // Immutable reader-produced bank. key==128 inspects any key; notes require 0..127.
    // Exact patch/key first, then original percussion bank/program zero fallback.
    // Failure clears output. Ambiguous packed identifiers are refused, not ordered arbitrarily.
    // Why a lookup of valid input found no instrument: no instrument has the patch (Missing), or one
    // unambiguous instrument has it but no region covers the key (Uncovered). Invalid input and
    // ambiguous patches leave None. The lookup still fails; the caller decides what the note means.
    enum class DlsMiss : std::uint8_t
    {
        None,
        Uncovered,
        Missing
    };
    bool find_dls_instrument(const DlsBank&, const DlsMidiChannel&, unsigned key, std::optional<std::size_t>&,
                             std::string& error, DlsMiss* miss = nullptr);
    struct DlsVoice
    {
        std::uint64_t id = 0, frames = 0, gate = 0, loops = 0;
        std::shared_ptr<const DlsBank> bank;
        std::size_t instrument = 0, region = 0;
        unsigned key = 0, velocity = 0;
        DlsControllers controllers;
        std::optional<unsigned> midi_channel; // Actual receiver slot; absent for independent auditions.
        std::optional<std::uint64_t> choke_frame;
        double choke_level = 0, choke_seconds = 0; // Bounded replacement fast-release envelope.
        std::optional<DlsMidiChannel> channel;     // Present only for routed notes; immutable note-on snapshot.
        double cursor = 0, base_step = 0, gain = 0, left = 0, right = 0;
        double nominal_gain = 0, bend_step = 1, pitch_extent = 0; // Uncontrolled gain and pitch budget.
        double pan = 0;               // Constant offset in 0.1-percent units, before CC10 and clamping.
        unsigned pan_destination = 0; // Last constant-pan writer: 2 (integer legacy), 4 (16.16 DLS), or none.
        bool legacy_pan = false;      // The selected graph contained a validated legacy pan record.
        double lfo_hz = 5, lfo_delay = .01, lfo_pitch = 0, lfo_gain = 0, envelope_pitch = 0;
        DlsEnvelope amplitude, pitch;
    };
    // A received note-on played as silence (first occurrence per patch/key and kind).
    struct DlsSilentNote
    {
        unsigned bank_msb = 0, bank_lsb = 0, program = 0, key = 0;
        bool percussion = false;
        DlsMiss miss = DlsMiss::Uncovered;
        std::uint64_t frame = 0; // Sample clock of the first occurrence.
    };
    struct DlsTimedMidi
    {
        std::uint64_t frame = 0; // Absolute AudioMixer sample boundary, not an IMA tick.
        std::uint8_t status = 0, first = 0, second = 0;
    };
    struct DlsQueuedMidi
    {
        DlsTimedMidi message;
        std::shared_ptr<const DlsBank> bank; // Only note-ons retain the immutable resource.
        double gain = 0;
        unsigned bank_msb = 0, bank_lsb = 0; // Program's bank captured at submission.
    };
    // A bounded sample voice engine; explicit sample scheduling is not a section/playlist sequencer.
    // Callers supply immutable decoded banks and note-on controller snapshots.
    class DlsSampler
    {
      public:
        static constexpr unsigned rate = 48000, max_voices = 32;
        // Immediate MIDI receiver, not a timestamp queue. Channel 9 defaults to percussion.
        // No bank is required except for note-on. Failure preserves channels and live voices.
        bool receive(std::shared_ptr<const DlsBank>, std::uint8_t status, std::uint8_t first, std::uint8_t second,
                     double gain, std::string& error);
        const auto& midi_channels() const
        {
            return midi_channels_;
        }
        std::uint64_t midi_received = 0, midi_rejected = 0, midi_released = 0, midi_choked = 0;
        std::uint64_t midi_unmatched_stops = 0;
        // Received MIDI notes never end the music over a routing gap. DLS Level 1: a note no region of
        // its (existing, unambiguous) instrument covers sounds nothing. Replacement policy: a note whose
        // bank/program names no instrument is silent too (the original synth's fallback, General MIDI
        // or silence, is unconfirmed). Both are counted and each patch/key is logged once per world;
        // auditions, ambiguous patches and invalid input stay explicit failures.
        static constexpr std::size_t max_silent_notes = 256;
        std::uint64_t midi_uncovered_notes = 0, midi_missing_notes = 0;
        std::vector<DlsSilentNote> silent_notes; // Distinct patch/key/kind, first occurrence first.
        std::string last_midi_error;
        static constexpr std::size_t max_pending_midi = 65536;
        // Atomic batch admission. Original bank selects are submission-time latches;
        // programs capture those latches, controls/programs take effect at their sample boundary.
        // Notes at equal times retain submission order and see the last due channel values.
        // Convert original IMA event ticks before the same atomic sample-queue admission.
        bool enqueue_ima(std::shared_ptr<const DlsBank>, const ImaClock&, std::span<const ImaTimedMidiMessage>,
                         double gain, std::string& error);
        bool enqueue(std::shared_ptr<const DlsBank>, std::span<const DlsTimedMidi>, double gain, std::string& error);
        const auto& pending_midi() const
        {
            return pending_midi_;
        }
        std::uint64_t sample_clock() const
        {
            return sample_clock_;
        }
        const auto& pending_banks() const
        {
            return pending_banks_;
        }
        std::uint64_t queued_messages = 0, queue_rejected = 0, dispatched_messages = 0, coalesced_messages = 0;
        std::uint64_t dispatch_failures = 0;
        std::optional<std::uint64_t> failed_dispatch_frame;
        std::string last_queue_error, last_dispatch_error; // Delivery history survives later successes.
        std::uint64_t play(std::shared_ptr<const DlsBank>, std::size_t instrument, unsigned key, unsigned velocity,
                           double gate_seconds, double gain, std::string& error, DlsControllers = {});
        std::uint64_t play_channel(std::shared_ptr<const DlsBank>, const DlsMidiChannel&, unsigned key,
                                   unsigned velocity, double gate_seconds, double gain, std::string& error);
        // Immediate update at the current audio sample boundary. Does not reset any voice clock or envelope.
        bool set_controllers(std::uint64_t id, DlsControllers, std::string& error);
        bool release(std::uint64_t id);
        // Adds to an interleaved stereo prefix. All frames advance, including unrendered ones.
        bool advance(std::uint64_t frames, std::span<float> stereo);
        // Explicit instrument-only cancellation. Preserve the shared clock, channels and failure history.
        // Remove future events when replacing the instrument stream; keep clock/channels.
        void discard_midi_from(std::uint64_t frame);
        std::uint64_t cancelled_messages = 0;
        void silence();
        void clear();
        const auto& voices() const
        {
            return voices_;
        }
        std::uint64_t started = 0, completed = 0, rejected = 0;
        std::uint64_t controls_updated = 0, controls_rejected = 0;
        std::string last_control_error;
        std::string last_error;

      private:
        void advance_voices(std::uint64_t frames, std::span<float> stereo);
        std::uint64_t sample_clock_ = 0;
        std::vector<DlsQueuedMidi> pending_midi_;
        std::array<std::array<unsigned, 2>, 16> pending_banks_{};
        std::uint64_t next_id_ = 1; // Never reused, including across clear().
        std::vector<DlsVoice> voices_;
        std::array<DlsMidiChannel, 16> midi_channels_ = []
        {
            std::array<DlsMidiChannel, 16> channels{};
            // Explicitly initialize every slot: GCC 16 -O3 can zero the tail of
            // this nested aggregate after the percussion-slot write (isolated repro in evidence).
            channels.fill(DlsMidiChannel{});
            channels[9].percussion = true;
            return channels;
        }();
    };
} // namespace OpenBlood2
