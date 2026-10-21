#include "engine/hud_surface.h"
#include <algorithm>
namespace OpenBlood2
{
    bool read_pcx(std::span<const std::uint8_t> b, HudSurface& output, std::string& error)
    {
        output = {};
        error = "PCX: invalid header, dimensions, palette or RLE data";
        if (b.size() < 128 || b.size() > 16 * 1024 * 1024 || b[0] != 10 || b[1] != 5 || b[2] != 1 || b[3] != 8)
            return false;
        auto word = [&](unsigned p) { return unsigned(b[p]) | (unsigned(b[p + 1]) << 8); };
        if (word(8) < word(4) || word(10) < word(6))
            return false;
        const unsigned width = word(8) - word(4) + 1, height = word(10) - word(6) + 1;
        const unsigned stride = word(66), planes = b[65];
        if (width > 4096 || height > 4096 || width * height > 4 * 1024 * 1024 || stride < width || stride > 8192 ||
            (planes != 1 && planes != 3))
            return false;
        std::size_t end = b.size();
        if (planes == 1)
        {
            if (end < 897 || b[end - 769] != 12)
                return false;
            end -= 769;
        }
        HudSurface result{width, height, std::vector<std::uint8_t>(width * height * 4)};
        std::vector<std::uint8_t> row(stride * planes);
        std::size_t pos = 128;
        for (unsigned y = 0; y < height; ++y)
        {
            std::size_t n = 0;
            while (n < row.size())
            {
                if (pos >= end)
                    return false;
                auto value = b[pos++];
                unsigned count = 1;
                if ((value & 0xc0) == 0xc0)
                {
                    count = value & 0x3f;
                    if (!count || pos >= end)
                        return false;
                    value = b[pos++];
                }
                if (count > row.size() - n)
                    return false;
                std::fill_n(row.begin() + n, count, value);
                n += count;
            }
            for (unsigned x = 0; x < width; ++x)
            {
                const auto target = (y * width + x) * 4;
                for (unsigned c = 0; c < 3; ++c)
                    result.rgba[target + c] = planes == 1 ? b[end + 1 + row[x] * 3 + c] : row[c * stride + x];
                result.rgba[target + 3] = 255;
            }
        }
        if (pos != end)
            return false;
        output = std::move(result);
        error.clear();
        return true;
    }
    void hud_blit(HudSurface& dst, const HudSurface& src, const HudRect* rect, int x, int y, bool keyed,
                  std::uint32_t key)
    {
        const HudRect r = rect ? *rect : HudRect{0, 0, int(src.width), int(src.height)};
        const int left = std::max({r.left, 0, r.left - x}), top = std::max({r.top, 0, r.top - y});
        const int right = std::min({r.right, int(src.width), r.left + int(dst.width) - x});
        const int bottom = std::min({r.bottom, int(src.height), r.top + int(dst.height) - y});
        for (int sy = top; sy < bottom; ++sy)
            for (int sx = left; sx < right; ++sx)
            {
                const auto s = (sy * src.width + sx) * 4;
                const auto d = ((y + sy - r.top) * dst.width + x + sx - r.left) * 4;
                const auto rgb = (unsigned(src.rgba[s]) << 16) | (unsigned(src.rgba[s + 1]) << 8) | src.rgba[s + 2];
                if (!keyed || rgb != key)
                    std::copy_n(src.rgba.begin() + s, 4, dst.rgba.begin() + d);
            }
    }
    bool HudFont::read_widths(std::span<const std::uint8_t> b, const HudSurface& bitmap, std::string& error, unsigned terminal_overhang)
    {
        *this = {};
        error = "FNT: expected height and 94 bounded little-endian glyph widths";
        if (b.size() != 190 || terminal_overhang > 1)
            return false;
        auto word = [&](unsigned p) { return unsigned(b[p]) | (unsigned(b[p + 1]) << 8); };
        HudFont result;
        result.height = word(0);
        if (!result.height || result.height != bitmap.height)
            return false;
        for (unsigned i = 0; i < 94; ++i)
        {
            result.widths[i] = word(2 + i * 2);
            result.offsets[i + 1] = result.offsets[i] + result.widths[i];
            if (result.offsets[i + 1] > bitmap.width + (i == 93 ? terminal_overhang : 0))
                return false;
        }
        if (result.offsets[94] < bitmap.width || result.offsets[94] > bitmap.width + terminal_overhang)
            return false;
        result.surface = &bitmap;
        *this = result;
        error.clear();
        return true;
    }
    void HudFont::draw(HudSurface& dst, std::string_view text, int x, int y, unsigned justify) const
    {
        if (!surface)
            return;
        unsigned length = 0;
        for (unsigned char c : text)
            if (c >= 33 && c <= 126)
                length += widths[c - 33];
            else if (c == ' ')
                length += space_width;
        if (justify == 1)
            x -= length / 2;
        if (justify == 2)
            x -= length;
        for (unsigned char c : text)
        {
            if (c == ' ')
                x += int(space_width);
            if (c < 33 || c > 126)
                continue;
            const auto i = c - 33;
            HudRect rect{int(offsets[i]), 0, int(offsets[i + 1]), int(height)};
            hud_blit(dst, *surface, &rect, x, y, true, hud_magenta);
            x += widths[i];
        }
    }
    HudSurface* HudCanvas::CreateSurface(unsigned width, unsigned height)
    {
        if (!width || !height || width > 4096 || height > 4096)
        {
            error = "HUD surface dimensions invalid";
            return nullptr;
        }
        auto surface =
            std::make_unique<HudSurface>(HudSurface{width, height, std::vector<std::uint8_t>(width * height * 4)});
        auto* result = surface.get();
        surfaces_.push_back(std::move(surface));
        return result;
    }
    HudSurface* HudCanvas::CreateSurfaceFromBitmap(const char* path)
    {
        if (!error.empty())
            return nullptr;
        std::string key;
        if (!resource_path(path, key))
        {
            error = "Invalid HUD resource path";
            return nullptr;
        }
        if (auto it = images_.find(key); it != images_.end())
            return &it->second;
        const auto* resource = resources_.find(key);
        if (!resource || resource->size > 16 * 1024 * 1024)
        {
            error = "Missing/oversized HUD resource: " + key;
            return nullptr;
        }
        std::vector<std::uint8_t> bytes;
        HudSurface surface;
        if (!resources_.read(key, bytes, error) || !read_pcx(bytes, surface, error))
        {
            error = key + ": " + error;
            return nullptr;
        }
        return &images_.emplace(key, std::move(surface)).first->second;
    }
    void HudCanvas::GetSurfaceDims(const HudSurface* s, unsigned* w, unsigned* h) const
    {
        *w = s ? s->width : 0;
        *h = s ? s->height : 0;
    }
    void HudCanvas::FillRect(HudSurface* s, const HudRect* rect, std::uint32_t color)
    {
        if (!s)
            return;
        const auto r = rect ? *rect : HudRect{0, 0, int(s->width), int(s->height)};
        for (int y = std::max(0, r.top); y < std::min(int(s->height), r.bottom); ++y)
            for (int x = std::max(0, r.left); x < std::min(int(s->width), r.right); ++x)
            {
                auto p = (y * s->width + x) * 4;
                s->rgba[p] = color >> 16;
                s->rgba[p + 1] = color >> 8;
                s->rgba[p + 2] = color;
                s->rgba[p + 3] = 255;
            }
    }
    void HudCanvas::DrawSurfaceToSurface(HudSurface* d, const HudSurface* s, const HudRect* r, int x, int y)
    {
        if (d && s)
            hud_blit(*d, *s, r, x, y, false, 0);
    }
    void HudCanvas::DrawSurfaceToSurfaceTransparent(HudSurface* d, const HudSurface* s, const HudRect* r, int x, int y,
                                                    std::uint32_t key)
    {
        if (d && s)
            hud_blit(*d, *s, r, x, y, true, key);
    }
    bool HudCanvas::load_font(const char* bitmap, const char* widths, HudFont& font, unsigned terminal_overhang)
    {
        auto* image = CreateSurfaceFromBitmap(bitmap);
        if (!image)
            return false;
        std::vector<std::uint8_t> bytes;
        const auto* resource = resources_.find(widths);
        if (!resource || resource->size != 190)
        {
            error = std::string("Missing/invalid FNT: ") + widths;
            return false;
        }
        return resources_.read(widths, bytes, error) && font.read_widths(bytes, *image, error, terminal_overhang);
    }
} // namespace OpenBlood2
