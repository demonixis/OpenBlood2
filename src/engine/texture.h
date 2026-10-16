#pragma once
#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace OpenBlood2
{
    struct Texture
    {
        std::uint32_t width = 0, height = 0, mipmaps = 0, flags = 0, user_flags = 0;
        std::vector<std::uint8_t> rgba;
    };
    // Blood II DTX -2 only; validates all stored mip/alpha bytes, decodes mip zero.
    // Empty output on failure. Later DTX generations and shared palettes are rejected.
    bool read_texture(std::span<const std::uint8_t>, Texture&, std::string&);
} // namespace OpenBlood2
