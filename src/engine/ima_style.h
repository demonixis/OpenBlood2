#pragma once
#include "engine/music_asset.h"
#include <array>

namespace OpenBlood2
{
    // IMA 2.5 pattern records, not MIDI events ready for playback. Pitch conversion,
    // variations, humanization, curves, band selection and section timing are separate.
    struct ImaNote
    {
        std::uint8_t type = 0, channel = 0;
        std::int16_t time_offset = 0;
        std::uint16_t variations = 0;
        std::uint8_t pitch_modifier = 0, byte7 = 0, midi_note = 0, velocity = 0;
        std::uint16_t music_value = 0;
        std::int16_t duration = 0;
        std::uint8_t time_range = 0, duration_range = 0, velocity_range = 0, play_mode = 0;
    };
    struct ImaGrid
    {
        std::int16_t index = 0; // Authored clik field; list order is retained independently.
        std::uint32_t offset = 0;
        std::vector<ImaNote> notes;
        std::size_t curve_chunks = 0;
    };
    struct ImaPattern
    {
        std::string name;
        std::uint32_t offset = 0, word0 = 0, flags = 0;
        std::int32_t length = 0;
        std::array<std::int16_t, 4> timing{}; // Raw fields at patt offsets 12..19.
        std::uint32_t chord_mask = 0, scale_mask = 0;
        std::array<std::int8_t, 16> upper_pitch{}, lower_pitch{};
        std::uint16_t pitch_range_channels = 0, word606 = 0;
        std::array<std::array<std::uint16_t, 16>, 16> variation_flags{};
        std::int8_t root = 0;
        std::array<std::uint8_t, 16> play_modes{}; // Authored; defaults are prepared separately.
        std::vector<ImaGrid> grids;
    };
    struct ImaStyle
    {
        std::string name;
        MusicAssetInfo source; // All source ranges, including partially decoded/opaque records.
        std::int16_t beats_per_measure = 0, beat_unit = 0, ticks_per_grid = 0;
        std::int16_t authored_ticks_per_beat = 0, authored_ticks_per_measure = 0, tempo = 0;
        std::uint16_t word44 = 0;
        std::array<std::uint8_t, 16> identifier{};
        std::array<std::uint8_t, 32> tail{};
        std::vector<ImaPattern> patterns;
        std::size_t notes = 0, grids = 0, curve_chunks = 0;
    };
    // Bounded installed AASY profile: styl94, patt625, clik2, note stride18.
    // Owns all output; failure clears it. Success certifies these records, not playback.
    bool read_ima_style(std::span<const std::uint8_t>, ImaStyle&, std::string& error, std::size_t note_budget = 65536);
} // namespace OpenBlood2
