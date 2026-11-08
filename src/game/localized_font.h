#pragma once
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace OpenBlood2
{
    struct HudRect;
    struct HudSurface;
    struct HudFont;
    class LocalizedFont;

    // Preserve native bitmap glyph advances and fill gaps with the bundled face.
    class LocalizedBitmapLine
    {
      public:
        LocalizedBitmapLine(const HudFont&, const LocalizedFont&, std::string_view);
        ~LocalizedBitmapLine();
        int advance() const { return width_; }
        void draw(HudSurface&, const HudRect&, std::optional<std::uint32_t> fallback_color = {}) const;

      private:
        struct Part;
        const HudFont& bitmap_;
        const LocalizedFont& fallback_;
        std::vector<Part> parts_;
        int width_ = 0;
    };

    // A bundled Unicode face for translated labels on the original CPU menu canvas.
    class LocalizedFont
    {
      public:
        LocalizedFont();
        ~LocalizedFont();
        bool load(std::string& error);
        explicit operator bool() const;
        bool supports(std::string_view) const;
        std::span<const std::uint8_t> bytes() const;
        int text_width(std::string_view, int height) const;
        void draw(HudSurface&, std::string_view, const HudRect&, std::uint32_t rgb) const;
        // Tint a Unicode menu run using the mounted bitmap font's current state or the page's title art.
        void draw_menu(HudSurface&, std::string_view, const HudRect&, const HudSurface& ink_source,
                       bool title = false) const;
        struct TextReveal
        {
            std::size_t first, last; // UTF-8 boundaries in the original string, before wrapping.
        };
        // The original formatted text uses '|' for a line break and '~' for a tab.
        void draw_wrapped(HudSurface&, std::string_view, const HudRect&, int line_height, std::uint32_t rgb,
                          const HudFont* bitmap = nullptr, std::optional<TextReveal> reveal = {}) const;

      private:
        struct Face;
        std::unique_ptr<Face> face_;
        void draw_glyphs(HudSurface&, std::string_view, const HudRect&, std::uint32_t rgb,
                         const HudSurface* ink_source, bool title) const;
    };
} // namespace OpenBlood2
