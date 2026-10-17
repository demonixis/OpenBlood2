#pragma once
#include "engine/music_asset.h"
#include <array>

namespace OpenBlood2
{
    struct ImaSectionCommand
    {
        std::int32_t time = 0;
        std::array<std::uint8_t, 4> value{}; // Original cmnd bytes; no invented intensity mapping.
    };
    struct ImaSectionMute
    {
        std::int32_t time = 0;
        std::uint16_t value = 0, word6 = 0;
    };
    struct ImaSectionChord
    {
        std::string name;
        std::uint8_t flags = 0;
        std::int8_t beat = 0;
        std::int16_t measure = 0;
        std::uint32_t chord_pattern = 0, scale_pattern = 0, word44 = 0;
        std::int8_t root = 0;
        std::array<std::uint8_t, 68> tail{};
    };
    struct ImaSection
    {
        MusicAssetInfo source;
        std::string name, personality; // Authored label only, never a filesystem path.
        std::uint32_t word0 = 0, word48 = 0;
        std::int16_t tempo = 0, word38 = 0, measures = 0, ticks_per_measure = 0, ticks_per_beat = 0;
        std::uint16_t tempo_fraction = 0;
        std::uint8_t default_chord = 0;
        std::array<std::uint8_t, 3> padding53{};
        std::array<std::uint8_t, 16> style_id{}, personality_id{};
        std::array<std::uint8_t, 32> tail{};
        std::vector<ImaSectionCommand> commands;
        std::vector<ImaSectionMute> mutes;
        std::vector<ImaSectionChord> chords;
        std::size_t opaque_note_chunks = 0, opaque_curve_chunks = 0;
        std::size_t records() const
        {
            return commands.size() + mutes.size() + chords.size();
        }
    };
    struct ImaSectionTiming
    {
        std::int32_t duration_ticks = 0;
        std::uint32_t microseconds_per_quarter = 0; // Original MIDI-export quantization (1/64 BPM).
    };
    // Installed AASE profile: secn120, cmnd/mute stride8, chrd stride117.
    // Owns decoded values; failure clears output. Unknown chunks remain source ranges.
    bool read_ima_section(std::span<const std::uint8_t>, ImaSection&, std::string& error,
                          std::size_t record_budget = 4096);
    // Authored header timing only. The original loader can replace meter from its style.
    // These functions do not select patterns/personality, schedule events or play music.
    bool ima_section_timing(const ImaSection&, ImaSectionTiming&, std::string& error);
    bool ima_section_command_measure(const ImaSection&, std::int32_t time, std::int16_t& measure);
} // namespace OpenBlood2
