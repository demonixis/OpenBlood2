#include "engine/texture.h"
#include "engine/formats/reader.h"
#include "engine/resources.h"
#include <algorithm>
#include <bit>

namespace OpenBlood2
{
    bool read_texture_resource(const Resources& resources, std::string_view path, Texture& out, std::string& error)
    {
        out = {};
        const auto* resource = resources.find(path);
        if (!resource)
        {
            error = "Missing exact-path resource";
            return false;
        }
        if (resource->size > 3 * 1024 * 1024)
        {
            error = "DTX resource exceeds byte limit";
            return false;
        }
        std::vector<std::uint8_t> bytes;
        return resources.read(path, bytes, error) && read_texture(bytes, out, error);
    }
    bool read_texture(std::span<const std::uint8_t> bytes, Texture& out, std::string& error)
    {
        out = {};
        error.clear();
        auto fail = [&](const char* why)
        {
            error = why;
            return false;
        };
        if (bytes.size() < 1068 || bytes.size() > 3 * 1024 * 1024)
            return fail("DTX: truncated or oversized palette image");
        Reader r(bytes);
        if (r.u32() != 0 || r.u32() != 0xfffffffeu)
            return fail("DTX: only embedded-palette version -2 is supported");
        Texture image;
        image.width = r.u16();
        image.height = r.u16();
        image.mipmaps = r.u16();
        const auto sections = r.u16();
        image.flags = r.u32();
        image.user_flags = r.u32();
        // These are not the later -5 BPP field/128-byte command string. Preserve
        // the generation boundary. Two serialized 32-bit slots at 36..43 are opaque.
        r.skip(20);
        if (!std::has_single_bit(image.width) || !std::has_single_bit(image.height) || image.width > 1024 ||
            image.height > 1024 || !image.mipmaps || image.mipmaps > 8 ||
            image.mipmaps > static_cast<unsigned>(std::bit_width(std::max(image.width, image.height))))
            return fail("DTX: invalid dimensions/mip count");
        if (sections || (image.flags & ~0x2bu))
            return fail("DTX: unsupported sections/flags");
        std::size_t indices = 0, alpha = 0;
        auto w = image.width, h = image.height;
        for (unsigned i = 0; i < image.mipmaps; ++i)
        {
            const std::size_t pixels = w * h;
            indices += pixels;
            alpha += (pixels + 1) / 2;
            w = std::max(1u, w / 2);
            h = std::max(1u, h / 2);
        }
        const bool masks = (image.flags & 2) != 0;
        if (bytes.size() != 1068 + indices + (masks ? alpha : 0))
            return fail("DTX: inconsistent mip/alpha payload length");
        const auto palette = r.take(1024), pixels = r.take(indices);
        const auto masks_data = r.take(masks ? alpha : 0);
        if (!r.good() || r.remaining())
            return fail("DTX: invalid payload bounds");
        image.rgba.resize(std::size_t(image.width) * image.height * 4);
        for (std::size_t i = 0; i < image.rgba.size() / 4; ++i)
        {
            const auto p = std::size_t(pixels[i]) * 4;
            image.rgba[i * 4] = palette[p + 1];
            image.rgba[i * 4 + 1] = palette[p + 2];
            image.rgba[i * 4 + 2] = palette[p + 3];
            image.rgba[i * 4 + 3] = masks ? ((masks_data[i / 2] >> (i % 2 ? 0 : 4)) & 15) * 17 : palette[p];
        }
        out = std::move(image);
        return true;
    }
} // namespace OpenBlood2
