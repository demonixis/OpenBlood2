#pragma once
#include <cstdint>
#include <map>
#include <span>
#include <string>
#include <string_view>
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
    // Installed iMusic.ini [Styles] NameN/FileN mapping. Bounded ASCII basenames;
    // GUID/tempo metadata remain opaque. Failure clears the owned mapping.
    bool read_music_style_index(std::string_view, std::map<std::string, std::string>&, std::string& error);
    constexpr std::size_t max_music_asset_bytes = 32 * 1024 * 1024;
} // namespace OpenBlood2
