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
        bool read_widths(std::span<const std::uint8_t>, const HudSurface&, std::string&, unsigned terminal_overhang = 0);
        void draw(HudSurface&, std::string_view, int x, int y, unsigned justify) const;
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
    inline constexpr std::uint32_t hud_magenta = 0xff00ff;
} // namespace OpenBlood2
