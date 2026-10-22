#pragma once
#include "engine/resources.h"
#include <array>
#include <map>
#include <memory>
namespace OpenBlood2
{
    struct HudRect
    {
        int left = 0, top = 0, right = 0, bottom = 0;
    };
    struct HudSurface
    {
        unsigned width = 0, height = 0;
        std::vector<std::uint8_t> rgba;
    };
    // PCX v5 RLE, 8-bit indexed or RGB planes. Color keys belong to composition.
    bool read_pcx(std::span<const std::uint8_t>, HudSurface&, std::string&);
    struct HudFont
    {
        const HudSurface* surface = nullptr;
        unsigned height = 0;
        std::array<unsigned, 95> offsets{};
        std::array<unsigned, 94> widths{};
        unsigned space_width = 0; // Text fonts: the menus' quarter-height adapter policy; numbers use none.
        bool read_widths(std::span<const std::uint8_t>, const HudSurface&, std::string&,
                         unsigned terminal_overhang = 0);
        void draw(HudSurface&, std::string_view, int x, int y, unsigned justify) const;
        // CoolFont DrawSolid: every unkeyed glyph pixel takes `rgb` (0xRRGGBB).
        void draw_solid(HudSurface&, std::string_view, int x, int y, unsigned justify, std::uint32_t rgb) const;
        // CoolFont DrawFormat (its source is unavailable): left-justified words wrapped before
        // `width` pixels; '|' breaks the line and '~' indents by four spaces.
        void draw_format(HudSurface&, std::string_view, int x, int y, int width) const;
        unsigned text_width(std::string_view) const;

      private:
        void draw_glyphs(HudSurface&, std::string_view, int x, int y, unsigned justify, const std::uint32_t* rgb) const;
    };
    // Typed CPU surface service; no legacy client ABI.
    class HudCanvas
    {
      public:
        explicit HudCanvas(const Resources& resources) : resources_(resources) {}
        HudSurface* CreateSurface(unsigned width, unsigned height);
        HudSurface* CreateSurfaceFromBitmap(const char* path);
        void GetSurfaceDims(const HudSurface*, unsigned*, unsigned*) const;
        void FillRect(HudSurface*, const HudRect*, std::uint32_t color);
        void DrawSurfaceToSurface(HudSurface*, const HudSurface*, const HudRect*, int x, int y);
        void DrawSurfaceToSurfaceTransparent(HudSurface*, const HudSurface*, const HudRect*, int x, int y,
                                             std::uint32_t key);
        bool load_font(const char* bitmap, const char* widths, HudFont&, unsigned terminal_overhang = 0);
        std::string error;
        const auto& images() const
        {
            return images_;
        }

      private:
        const Resources& resources_;
        std::map<std::string, HudSurface> images_;
        std::vector<std::unique_ptr<HudSurface>> surfaces_;
    };
    void hud_blit(HudSurface&, const HudSurface&, const HudRect*, int x, int y, bool keyed, std::uint32_t key);
    // ScaleSurfaceToSurface over whole surfaces: nearest-neighbour stretch of `src` onto `dst`.
    void hud_scale(HudSurface& dst, const HudSurface& src);
    inline constexpr std::uint32_t hud_magenta = 0xff00ff;
} // namespace OpenBlood2
