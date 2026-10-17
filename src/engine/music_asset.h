#pragma once
#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace OpenBlood2
{
    enum class MusicAssetKind
    {
        Section,
        Style,
        Instruments
    };
    struct MusicChunk
    {
        std::string id, form;
        std::uint32_t offset = 0, size = 0, depth = 0;
    };
    struct MusicAssetInfo
    {
        std::vector<MusicChunk> chunks;
        std::string style_reference;
    };
    // Structural inspection only. Musical records remain opaque, not synthesized.
    // Output owns its metadata and is empty after any failure.
    bool read_music_asset(std::span<const std::uint8_t>, MusicAssetKind, MusicAssetInfo&, std::string& error);
    constexpr std::size_t max_music_asset_bytes = 32 * 1024 * 1024;
} // namespace OpenBlood2
