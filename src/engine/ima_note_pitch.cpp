#include "engine/ima_note_pitch.h"
#include <bit>

namespace OpenBlood2
{
    namespace
    {
        std::uint32_t shift(std::uint32_t mask, unsigned count = 1)
        {
            // x86 SAR count masking; C++20 defines signed right shift toward -infinity.
            return static_cast<std::uint32_t>(std::bit_cast<std::int32_t>(mask) >> (count & 31));
        }
        std::int16_t word(int value)
        {
            return std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(value));
        }
        std::uint8_t harmonic_pitch(std::uint16_t music, std::uint8_t modifier, const ImaHarmonicContext& context,
                                    std::uint8_t flags)
        {
            auto degree = static_cast<std::uint8_t>((music >> 8) & 15);
            auto steps = static_cast<std::uint8_t>((music >> 4) & 15);
            int chromatic = music & 15;
            if (chromatic > 8)
                chromatic -= 16;
            const auto base = static_cast<std::uint8_t>((music >> 12) * 12 + context.chord_root);
            auto pitch = base;
            const int threshold = flags & 0x20 ? 4 : 3;
            if ((flags & 15) > threshold)
                degree += (flags & 15) - threshold;
            auto chord = context.chord_mask;
            // Finite: positive masks exhaust within 32 bits; negative masks become
            // all ones, then exhaust the byte-sized degree counter.
            while (chord)
            {
                if (chord & 1)
                {
                    if (!degree)
                        break;
                    --degree;
                }
                chord = shift(chord);
                if (!chord)
                {
                    if (!modifier)
                        return 0;
                    pitch = base;
                    steps = static_cast<std::uint8_t>(std::bit_cast<std::int8_t>(modifier) >> 4);
                    chromatic = modifier & 15; // Fallback keeps the low nibble unsigned.
                    break;
                }
                ++pitch;
            }
            if (steps)
            {
                auto scale = context.scale_mask | (context.scale_mask << 12);
                const int remainder = (int(pitch) - context.scale_root) % 12;
                scale = shift(scale, static_cast<unsigned>(remainder));
                while (scale)
                {
                    if (scale & 1)
                    {
                        if (!steps)
                            break;
                        --steps;
                    }
                    scale = shift(scale);
                    ++pitch;
                }
            }
            return static_cast<std::uint8_t>(pitch + chromatic);
        }
        bool scale_pitch(const ImaNote& note, const ImaHarmonicContext& context, std::int16_t& out)
        {
            auto remaining = static_cast<std::uint16_t>(std::bit_cast<std::int8_t>(note.pitch_modifier) >> 4);
            auto scale = context.scale_mask | (context.scale_mask << 12);
            int pitch = (note.music_value >> 12) * 12 + context.scale_root;
            // At most 32 iterations before SAR reaches 0 or all ones. A negative
            // signed nibble is a wrapping 16-bit countdown in the original.
            while (remaining)
            {
                if (scale == 0)
                    return false;
                if (scale == 0xffffffffu)
                {
                    pitch += remaining;
                    break;
                }
                scale = shift(scale);
                if (scale & 1)
                    --remaining;
                ++pitch;
            }
            int chromatic = note.pitch_modifier & 15;
            if (chromatic > 8)
                chromatic -= 16;
            out = word(pitch + chromatic + 12);
            return true;
        }
    } // namespace

    bool resolve_ima_note_pitch(const ImaNote& note, const ImaPattern& pattern, const ImaHarmonicContext& context,
                                ImaNotePitch& out, std::string& error)
    {
        out = {};
        error.clear();
        if ((note.type != 8 && note.type != 9) || note.channel >= 16)
        {
            error = "IMA pitch: unsupported note type/channel";
            return false;
        }
        const auto raw = std::bit_cast<std::int8_t>(note.midi_note);
        if (!context.enabled || !note.music_value)
        {
            out.pitch = raw;
            return true;
        }
        auto mode = note.play_mode;
        if ((mode & 15) == 7 || !(mode & 0x80))
        {
            mode = pattern.play_modes[note.channel];
            if (!mode)
                mode = note.channel == 5 ? 1 : note.channel == 4 ? 2 : 3;
        }
        mode &= 15;
        std::int16_t pitch = raw;
        if (mode == 4)
        {
            if (!scale_pitch(note, context, pitch))
            {
                error = "IMA pitch: scale-only walk exhausts its mask before reaching the requested degree";
                return false;
            }
        }
        else if (mode != 1)
        {
            auto music = note.music_value;
            if (mode == 5 || mode == 6)
                music = static_cast<std::uint16_t>((music & 0xf00f) | (((music & 0x0f00) >> 3) + (music & 0x00f0)));
            const bool use_flags = mode != 2 && mode != 5;
            pitch = harmonic_pitch(music, note.pitch_modifier, context, use_flags ? context.chord_flags : 0);
            if (use_flags && (context.chord_flags & 0x40))
            {
                pitch -= 12;
                if (pitch <= 0)
                    pitch += 12;
            }
        }
        if (pitch)
        {
            pitch = word(pitch - 12);
            if ((pattern.pitch_range_channels & (1u << note.channel)) && (context.chord_flags & 0x10))
            {
                const int upper = pattern.upper_pitch[note.channel], lower = pattern.lower_pitch[note.channel];
                if (upper < pitch)
                    pitch = word(pitch - (static_cast<std::uint16_t>(pitch - upper + 11) / 12) * 12);
                if (lower > pitch)
                    pitch = word(pitch + (static_cast<std::uint16_t>(lower - pitch + 11) / 12) * 12);
            }
        }
        out = {pitch, mode, true};
        return true;
    }
} // namespace OpenBlood2
