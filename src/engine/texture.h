#pragma once
#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace OpenBlood2
{
    class Resources;
    struct Texture
    {
        std::uint32_t width = 0, height = 0, mipmaps = 0, flags = 0, user_flags = 0;
        std::vector<std::uint8_t> rgba;
        // Authored levels 1..mipmaps-1 in order, each max(1, size >> level), RGBA8.
        std::vector<std::uint8_t> mip_rgba;
    };
    // Bytes mip_rgba must hold for the texture's dimensions and level count.
    std::size_t texture_mip_bytes(const Texture&);
    // Blood II DTX -2 only; validates all stored mip/alpha bytes and decodes every
    // authored level with the shared palette/masks. Empty output on failure. Later
    // DTX generations and shared palettes are rejected.
    bool read_texture(std::span<const std::uint8_t>, Texture&, std::string&);
    bool read_texture_resource(const Resources&, std::string_view, Texture&, std::string&);
} // namespace OpenBlood2
