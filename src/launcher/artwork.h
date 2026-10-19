#pragma once
#include <cstdint>
#include <filesystem>
#include <span>
#include <vector>
namespace OpenBlood2::Launcher
{
    // Optional original GAME1 / demo BLOOD2 bitmap. Data-only bounded PE32/DIB decode; output is 320x240 RGBA.
    bool decode_artwork(std::span<const std::uint8_t>, std::vector<std::uint8_t>& rgba);
    bool load_artwork(const std::filesystem::path& directory, std::vector<std::uint8_t>& rgba);
} // namespace OpenBlood2::Launcher
