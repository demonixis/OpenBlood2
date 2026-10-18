#pragma once
#include "engine/music_asset.h"
#include <array>
#include <optional>

namespace OpenBlood2
{
    struct ImaBandChannel
    {
        std::uint8_t legacy_program = 0, volume = 0, pan = 0, program = 0, byte154 = 0;
        std::int8_t transpose = 0;
        std::uint16_t bank = 0; // Raw packed bank/flags, not a DLS bank identifier.
    };
    struct ImaBand
    {
        std::string name, collection; // Display labels, never filesystem paths.
        std::uint32_t offset = 0;     // band payload's chunk header.
        std::array<std::uint8_t, 8> reserved32{};
        std::int8_t byte104 = 0;
        std::uint8_t byte105 = 0;
        std::array<ImaBandChannel, 16> channels{};
    };
    struct ImaBands
    {
        MusicAssetInfo source;      // Including nested/unknown ranges not interpreted here.
        std::vector<ImaBand> bands; // Direct AABN containers, in file order.
    };
    enum class ImaBandScope
    {
        Section,
        Style
    };
    struct ImaBandChoice
    {
        ImaBandScope scope = ImaBandScope::Section;
        std::size_t index = 0; // Index in the selected immutable source, never a runtime pointer.
        bool operator==(const ImaBandChoice&) const = default;
    };
    // Loaded section overrides its style. Within each file, respect header-before-band loading.
    // Style fallback requires nonzero byte104; no implicit GM/default band is invented.
    // Failure clears output. Inputs must be reader-produced immutable catalogues.
    bool select_ima_band(const ImaBands& section, const ImaBands* style, std::optional<ImaBandChoice>&,
                         std::string& error);
    struct ImaMidiSetup
    {
        std::uint8_t status = 0, data1 = 0, data2 = 0;
        bool operator==(const ImaMidiSetup&) const = default;
    };
    // Installed AASE/AASY profile: one 202-byte band header per direct RIFF/AABN.
    // Zero bands is valid data. Failure clears all output; numeric fields stay raw.
    bool read_ima_bands(std::span<const std::uint8_t>, MusicAssetKind, ImaBands&, std::string& error,
                        std::size_t band_budget = 256);
    // Original patch packing, empty-collection fallback and source-channel-5 percussion flag.
    std::uint32_t ima_band_patch(const ImaBandChannel&, bool has_collection, bool percussion);
    // Ordered CC7, CC10, bank MSB, bank LSB, program for all 16 routed channels.
    // No playback or collection lookup. Refuses invalid MIDI values, clears output on failure.
    bool ima_band_setup(const ImaBand&, std::array<ImaMidiSetup, 80>&, std::string& error);
} // namespace OpenBlood2
