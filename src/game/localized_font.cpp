#include "game/localized_font.h"
#include "engine/hud_surface.h"
#include "engine/localization.h"
#include "engine/sha256.h"
#include <SDL3/SDL.h>
#include <algorithm>
#include <array>
#include <cmath>
#define STBTT_STATIC
#define STB_TRUETYPE_IMPLEMENTATION
#include <imstb_truetype.h>

namespace OpenBlood2
{
    namespace
    {
        using Ink = std::array<unsigned, 3>;
        using Palette = std::array<Ink, 32>;

        // Brightness quantiles retain the chosen PCX state's shadows and highlights. Title images
        // have a baked black backdrop, so leave those nearly black texels out of the ink sample.
        bool menu_palette(const HudSurface& source, bool title, Palette& palette)
        {
            struct Bin
            {
                std::size_t count = 0;
                Ink sum{};
            };
            std::array<Bin, 256> histogram{};
            std::size_t samples = 0;
            for (std::size_t i = 0; i + 3 < source.rgba.size(); i += 4)
            {
                const auto* pixel = source.rgba.data() + i;
                if (!pixel[3] || (pixel[0] == 255 && pixel[1] == 0 && pixel[2] == 255))
                    continue;
                const auto brightness = (2u * pixel[0] + 3u * pixel[1] + pixel[2]) / 6u;
                if (title && brightness < 12)
                    continue;
                auto& bin = histogram[brightness];
                ++samples;
                ++bin.count;
                for (std::size_t c = 0; c < bin.sum.size(); ++c)
                    bin.sum[c] += pixel[c];
            }
            if (!samples)
                return false;
            std::size_t value = 0, cumulative = histogram[0].count;
            for (std::size_t i = 0; i < palette.size(); ++i)
            {
                const auto quantile = i * (samples - 1) / (palette.size() - 1);
                while (cumulative <= quantile)
                    cumulative += histogram[++value].count;
                const auto& bin = histogram[value];
                for (std::size_t c = 0; c < bin.sum.size(); ++c)
                    palette[i][c] = unsigned(bin.sum[c] / bin.count);
            }
            return true;
        }

        unsigned grain(unsigned x, unsigned y, unsigned glyph)
        {
            std::uint32_t mixed = x * 0x9e3779b9u ^ y * 0x85ebca6bu ^ glyph * 0xc2b2ae35u;
            mixed = (mixed ^ (mixed >> 16)) * 0x7feb352du;
            return (mixed ^ (mixed >> 15)) & 255;
        }
        struct BitmapLetter
        {
            char base = 0, mark = 0;
            bool mirror_mark = false, inverted = false;
            int repeat = 1;
        };

        BitmapLetter bitmap_letter(Uint32 codepoint)
        {
            if (codepoint >= ' ' && codepoint <= '~')
                return {char(codepoint)};
            if (codepoint == U'’' || codepoint == U'‘')
                return {'\''};
            if (codepoint == U'…')
                return {'.', 0, false, false, 3};
            if (codepoint == U'¡' || codepoint == U'¿')
                return {codepoint == U'¡' ? '!' : '?', 0, false, true};

            // These precomposed Latin letters use the atlas base and a scaled punctuation mark
            // from the same normal, selected, disabled or small face.
            struct Accent
            {
                std::u32string_view letters;
                std::string_view bases;
                char mark;
                bool mirror = false;
            };
            constexpr Accent accents[]{{U"ÁÉÍÓÚÝáéíóúý", "AEIOUYaeiouy", '\''},
                                       {U"ÀÈÌÒÙàèìòù", "AEIOUaeiou", '\'', true},
                                       {U"ÂÊÎÔÛâêîôû", "AEIOUaeiou", '^'},
                                       {U"ÃÑÕãñõ", "ANOano", '~'},
                                       {U"ÄËÏÖÜŸäëïöüÿ", "AEIOUYaeiouy", '"'},
                                       {U"Çç", "Cc", ','}};
            for (const auto& accent : accents)
                if (const auto index = accent.letters.find(char32_t(codepoint)); index != accent.letters.npos)
                    return {accent.bases[index], accent.mark, accent.mirror};
            return {};
        }

        HudRect glyph_cell(const HudFont& font, char glyph)
        {
            const auto index = std::size_t(glyph - '!');
            return {int(font.offsets[index]), 0, int(font.offsets[index + 1]), int(font.height)};
        }

        bool glyph_pixel(const std::uint8_t* pixel)
        {
            return pixel[0] != 255 || pixel[1] != 0 || pixel[2] != 255;
        }

        HudRect glyph_ink(const HudSurface& atlas, const HudRect& cell)
        {
            HudRect ink{cell.right, cell.bottom, cell.left, cell.top};
            for (int y = cell.top; y < cell.bottom; ++y)
                for (int x = cell.left; x < std::min(cell.right, int(atlas.width)); ++x)
                    if (glyph_pixel(atlas.rgba.data() + (std::size_t(y) * atlas.width + x) * 4))
                    {
                        ink.left = std::min(ink.left, x);
                        ink.right = std::max(ink.right, x + 1);
                        ink.top = std::min(ink.top, y);
                        ink.bottom = std::max(ink.bottom, y + 1);
                    }
            return ink;
        }

        void stamp(HudSurface& target, const HudSurface& atlas, const HudRect& source, const HudRect& dest,
                   const HudRect& clip, bool mirror_x = false, bool mirror_y = false)
        {
            const int source_width = source.right - source.left, source_height = source.bottom - source.top;
            const int width = dest.right - dest.left, height = dest.bottom - dest.top;
            if (source_width <= 0 || source_height <= 0 || width <= 0 || height <= 0)
                return;
            for (int y = std::max({dest.top, clip.top, 0}); y < std::min({dest.bottom, clip.bottom, int(target.height)});
                 ++y)
            {
                const int row = (2 * (y - dest.top) + 1) * source_height / (2 * height);
                const int sy = mirror_y ? source.bottom - 1 - row : source.top + row;
                for (int x = std::max({dest.left, clip.left, 0});
                     x < std::min({dest.right, clip.right, int(target.width)}); ++x)
                {
                    const int column = (2 * (x - dest.left) + 1) * source_width / (2 * width);
                    const int sx = mirror_x ? source.right - 1 - column : source.left + column;
                    if (sx >= int(atlas.width)) // Original FNT may overhang the atlas by one terminal column.
                        continue;
                    const auto* pixel = atlas.rgba.data() + (std::size_t(sy) * atlas.width + sx) * 4;
                    if (glyph_pixel(pixel))
                        std::copy_n(pixel, 4, target.rgba.data() + (std::size_t(y) * target.width + x) * 4);
                }
            }
        }

        void draw_letter(HudSurface& target, const HudFont& font, BitmapLetter letter, int left, int right,
                         const HudRect& clip)
        {
            if (letter.base == ' ' || left >= right)
                return;
            const auto cell = glyph_cell(font, letter.base);
            const auto& atlas = *font.surface;
            HudRect dest{left, clip.top, right, clip.top + int(font.height)};
            if (!letter.mark)
            {
                stamp(target, atlas, cell, dest, clip, letter.inverted, letter.inverted);
                return;
            }

            const auto base = glyph_ink(atlas, cell);
            const auto mark = glyph_ink(atlas, glyph_cell(font, letter.mark));
            if (base.bottom <= base.top || mark.bottom <= mark.top)
            {
                stamp(target, atlas, cell, dest, clip);
                return;
            }
            const int mark_height = std::max(2, int(font.height) / 7);
            const int cell_width = cell.right - cell.left;
            const int mark_width = std::clamp((mark.right - mark.left) * mark_height / (mark.bottom - mark.top),
                                              1, cell_width);
            const int center = (base.left + base.right) / 2 - cell.left;
            const int mark_left = std::clamp(center - mark_width / 2, 0, cell_width - mark_width);
            const bool below = letter.mark == ',';
            const int shift = below ? 0 : std::clamp(mark_height + 1 - base.top, 0, cell.bottom - base.bottom);
            dest.top += shift;
            dest.bottom += shift;
            stamp(target, atlas, cell, dest, clip);
            const int mark_top = below ? base.bottom - 1 : std::max(0, base.top + shift - mark_height - 1);
            const int x = left + mark_left * (right - left) / cell_width;
            const int end = left + (mark_left + mark_width) * (right - left) / cell_width;
            stamp(target, atlas, mark, {x, clip.top + mark_top, std::max(x + 1, end), clip.top + mark_top + mark_height},
                  clip, letter.mirror_mark);
        }

        int bitmap_advance(const HudFont& font, BitmapLetter letter)
        {
            return letter.base == ' ' ? int(std::max(1u, font.height / 4)) : int(font.widths[letter.base - '!']);
        }
    } // namespace

    struct LocalizedBitmapLine::Part
    {
        BitmapLetter letter;
        std::string_view run;
        int advance;
    };

    LocalizedBitmapLine::LocalizedBitmapLine(const HudFont& bitmap, const LocalizedFont& fallback,
                                             std::string_view value)
        : bitmap_(bitmap), fallback_(fallback)
    {
        const char* cursor = value.data();
        for (auto remaining = value.size(); remaining;)
        {
            const auto* start = cursor;
            const auto letter = bitmap_letter(SDL_StepUTF8(&cursor, &remaining));
            if (letter.base)
            {
                const int advance = bitmap_advance(bitmap, letter);
                for (int i = 0; i < letter.repeat; ++i)
                    parts_.push_back({letter, {}, advance});
                width_ += advance * letter.repeat;
            }
            else
            {
                while (remaining)
                {
                    const auto* next = cursor;
                    auto rest = remaining;
                    if (bitmap_letter(SDL_StepUTF8(&next, &rest)).base)
                        break;
                    cursor = next;
                    remaining = rest;
                }
                const std::string_view run(start, std::size_t(cursor - start));
                const int advance = fallback.text_width(run, int(bitmap.height));
                parts_.push_back({{}, run, advance});
                width_ += advance;
            }
        }
    }

    LocalizedBitmapLine::~LocalizedBitmapLine() = default;

    void LocalizedBitmapLine::draw(HudSurface& target, const HudRect& clip,
                                   std::optional<std::uint32_t> fallback_color) const
    {
        const int fitted = std::min(width_, clip.right - clip.left);
        if (fitted <= 0 || !bitmap_.surface)
            return;
        int offset = 0;
        for (const auto& part : parts_)
        {
            const int left = clip.left + int(std::int64_t(offset) * fitted / width_);
            offset += part.advance;
            const int right = clip.left + int(std::int64_t(offset) * fitted / width_);
            if (part.letter.base)
                draw_letter(target, bitmap_, part.letter, left, right, clip);
            else if (fallback_color)
                fallback_.draw(target, part.run, {left, clip.top, right, clip.bottom}, *fallback_color);
            else
                fallback_.draw_menu(target, part.run, {left, clip.top, right, clip.bottom}, *bitmap_.surface);
        }
    }

    struct LocalizedFont::Face
    {
        std::vector<std::uint8_t> bytes;
        stbtt_fontinfo info{};
    };

    LocalizedFont::LocalizedFont() = default;
    LocalizedFont::~LocalizedFont() = default;

    bool LocalizedFont::load(std::string& error)
    {
        error.clear();
        if (face_)
            return true;
        auto next = std::make_unique<Face>();
        if (!read_application_asset("lang/fonts/locale.ttf", 6 * 1024 * 1024, next->bytes, error))
            return false;
        // stb_truetype requires trusted input. Authenticate the packaged face before decoding it.
        if (digest_text(sha256(next->bytes)) != BLOOD2_LOCALIZED_FONT_SHA256 ||
            !stbtt_InitFont(&next->info, next->bytes.data(), 0))
        {
            error = "Invalid bundled localization font";
            return false;
        }
        face_ = std::move(next);
        return true;
    }

    LocalizedFont::operator bool() const
    {
        return bool(face_);
    }

    bool LocalizedFont::supports(std::string_view text) const
    {
        if (!face_)
            return false;
        auto* cursor = text.data();
        auto remaining = text.size();
        while (remaining)
        {
            const auto before = remaining;
            const auto codepoint = SDL_StepUTF8(&cursor, &remaining);
            if (!codepoint || (codepoint == SDL_INVALID_UNICODE_CODEPOINT && before - remaining == 1) ||
                !stbtt_FindGlyphIndex(&face_->info, int(codepoint)))
                return false;
        }
        return true;
    }

    std::span<const std::uint8_t> LocalizedFont::bytes() const
    {
        return face_ ? std::span<const std::uint8_t>(face_->bytes) : std::span<const std::uint8_t>{};
    }

    int LocalizedFont::text_width(std::string_view text, int height) const
    {
        if (!face_ || height <= 0)
            return 0;
        const auto* font = &face_->info;
        const auto scale = stbtt_ScaleForPixelHeight(font, float(height));
        auto* cursor = text.data();
        auto remaining = text.size();
        int units = 0, previous = 0;
        while (remaining)
        {
            const auto codepoint = SDL_StepUTF8(&cursor, &remaining);
            if (!codepoint)
                break;
            const auto glyph = stbtt_FindGlyphIndex(font, int(codepoint));
            int advance = 0;
            stbtt_GetGlyphHMetrics(font, glyph, &advance, nullptr);
            units += advance + stbtt_GetGlyphKernAdvance(font, previous, glyph);
            previous = glyph;
        }
        return int(std::ceil(float(units) * scale));
    }

    void LocalizedFont::draw(HudSurface& target, std::string_view text, const HudRect& rect, std::uint32_t rgb) const
    {
        draw_glyphs(target, text, rect, rgb, nullptr, false);
    }

    void LocalizedFont::draw_menu(HudSurface& target, std::string_view text, const HudRect& rect,
                                  const HudSurface& ink_source, bool title) const
    {
        draw_glyphs(target, text, rect, 0x9b6347, &ink_source, title);
    }

    void LocalizedFont::draw_glyphs(HudSurface& target, std::string_view text, const HudRect& rect, std::uint32_t rgb,
                                    const HudSurface* ink_source, bool title) const
    {
        const int max_width = rect.right - rect.left, row_height = rect.bottom - rect.top;
        if (!face_ || text.empty() || max_width <= 0 || row_height <= 0)
            return;
        const int width = text_width(text, row_height);
        const int height = width > max_width ? std::max(1, row_height * max_width / width) : row_height;
        Palette palette{};
        const bool textured = ink_source && menu_palette(*ink_source, title, palette);
        const bool rough = textured && height >= 20;
        const auto* font = &face_->info;
        const auto scale = stbtt_ScaleForPixelHeight(font, float(height));
        int ascent = 0;
        stbtt_GetFontVMetrics(font, &ascent, nullptr, nullptr);
        const int baseline = rect.top + (row_height - height) / 2 + int(std::round(float(ascent) * scale));
        const int clip_left = std::max(0, rect.left), clip_top = std::max(0, rect.top);
        const int clip_right = std::min(int(target.width), rect.right);
        const int clip_bottom = std::min(int(target.height), rect.bottom);
        const Ink solid{(rgb >> 16) & 255, (rgb >> 8) & 255, rgb & 255};
        auto* cursor = text.data();
        auto remaining = text.size();
        int units = 0, previous = 0;
        while (remaining)
        {
            const auto codepoint = SDL_StepUTF8(&cursor, &remaining);
            if (!codepoint)
                break;
            const auto glyph = stbtt_FindGlyphIndex(font, int(codepoint));
            units += stbtt_GetGlyphKernAdvance(font, previous, glyph);
            int w = 0, h = 0, left = 0, top = 0, advance = 0;
            auto* bitmap = stbtt_GetGlyphBitmap(font, scale, scale, glyph, &w, &h, &left, &top);
            if (bitmap)
            {
                left += rect.left + int(std::round(float(units) * scale));
                top += baseline;
                const int border = rough ? 1 : 0;
                const auto mask = [&](int x, int y) -> unsigned
                { return x >= 0 && x < w && y >= 0 && y < h ? bitmap[y * w + x] : 0; };
                for (int y = std::max(top - border, clip_top); y < std::min(top + h + border, clip_bottom); ++y)
                    for (int x = std::max(left - border, clip_left); x < std::min(left + w + border, clip_right); ++x)
                    {
                        const int u = x - left, v = y - top;
                        unsigned coverage = mask(u, v);
                        const Ink* ink = &solid;
                        if (textured)
                        {
                            const unsigned gx = unsigned(u + 1), gy = unsigned(v + 1);
                            // Use broad mottles and smaller flecks. The fixed glyph pattern follows
                            // the character like the original bitmap and never consumes game RNG.
                            const unsigned broad = grain((gx + gy / 2) / 3, gy / 3, codepoint);
                            const unsigned fine = grain(gx, gy, codepoint);
                            if (rough)
                            {
                                const auto neighbour = std::max({mask(u - 1, v), mask(u + 1, v), mask(u, v - 1),
                                                                 mask(u, v + 1)});
                                coverage = std::max(coverage, neighbour * 144 / 255);
                                coverage -= std::min(coverage, fine / 12);
                            }
                            const unsigned mottled = (3 * broad + fine) / 4;
                            const unsigned tone = 7 + (14 * coverage + 10 * mottled) / 255;
                            ink = &palette[tone];
                        }
                        auto* pixel = target.rgba.data() + (std::size_t(y) * target.width + unsigned(x)) * 4;
                        for (unsigned c = 0; c < 3; ++c)
                            pixel[c] = std::uint8_t((pixel[c] * (255 - coverage) + (*ink)[c] * coverage + 127) / 255);
                    }
                stbtt_FreeBitmap(bitmap, nullptr);
            }
            stbtt_GetGlyphHMetrics(font, glyph, &advance, nullptr);
            units += advance;
            previous = glyph;
        }
    }

    void LocalizedFont::draw_wrapped(HudSurface& target, std::string_view text, const HudRect& rect, int line_height,
                                     std::uint32_t rgb, const HudFont* bitmap, std::optional<TextReveal> reveal) const
    {
        const int width = rect.right - rect.left, height = rect.bottom - rect.top;
        if (!face_ || text.empty() || width <= 0 || height <= 0 || line_height <= 0)
            return;
        std::string formatted;
        if (!reveal)
            for (const char c : text)
            {
                if (c == '~')
                    formatted += "    ";
                else
                    formatted += c == '|' ? '\n' : c;
            }
        const std::string_view source = reveal ? text : std::string_view(formatted);
        const auto wrap = [&](int line_height)
        {
            const auto* font = &face_->info;
            const auto scale = stbtt_ScaleForPixelHeight(font, float(line_height));
            constexpr std::u32string_view closing = U"，。！？；：、）》」』】〕…,.!?;:)";
            std::vector<std::string_view> lines;
            std::string_view rest(source);
            while (!rest.empty())
            {
                const auto end = rest.find('\n');
                auto paragraph = rest.substr(0, end);
                rest.remove_prefix(end == rest.npos ? rest.size() : end + 1);
                if (paragraph.empty())
                    lines.emplace_back();
                while (!paragraph.empty())
                {
                    const auto* cursor = paragraph.data();
                    std::size_t remaining = paragraph.size(), fitted = 0, preferred = 0;
                    int units = 0, previous_glyph = 0;
                    int bitmap_width = 0;
                    Uint32 previous = 0;
                    while (remaining)
                    {
                        const auto before = paragraph.size() - remaining;
                        const auto codepoint = SDL_StepUTF8(&cursor, &remaining);
                        if (codepoint == ' ' ||
                            (previous >= 0x2e80 && previous <= 0x9fff && closing.find(char32_t(codepoint)) == closing.npos))
                            preferred = before;
                        const auto letter = bitmap ? bitmap_letter(codepoint) : BitmapLetter{};
                        if (letter.base)
                        {
                            bitmap_width += bitmap_advance(*bitmap, letter) * letter.repeat;
                            previous_glyph = 0;
                        }
                        else
                        {
                            const auto glyph = stbtt_FindGlyphIndex(font, int(codepoint));
                            int advance = 0;
                            stbtt_GetGlyphHMetrics(font, glyph, &advance, nullptr);
                            units += advance + stbtt_GetGlyphKernAdvance(font, previous_glyph, glyph);
                            previous_glyph = glyph;
                        }
                        if (fitted && float(bitmap_width) + float(units) * scale > float(width))
                            break;
                        fitted = paragraph.size() - remaining;
                        previous = codepoint;
                    }
                    const auto length = fitted < paragraph.size() && preferred ? preferred : fitted;
                    auto line = paragraph.substr(0, length);
                    while (line.ends_with(' '))
                        line.remove_suffix(1);
                    lines.push_back(line);
                    paragraph.remove_prefix(length);
                    while (paragraph.starts_with(' '))
                        paragraph.remove_prefix(1);
                }
            }
            return lines;
        };
        line_height = std::min(line_height, height);
        auto lines = wrap(line_height);
        while (!bitmap && line_height > 8 && lines.size() > std::size_t(height / line_height))
            lines = wrap(--line_height);
        int top = rect.top;
        for (auto line : lines)
        {
            if (top + line_height > rect.bottom)
                break;
            HudRect row{rect.left, top, rect.right, top + line_height};
            top += line_height;
            if (reveal)
            {
                if (line.empty())
                    continue;
                const auto at = std::size_t(line.data() - source.data());
                const auto first = std::max(reveal->first, at), last = std::min(reveal->last, at + line.size());
                if (first >= last)
                    continue;
                const auto hidden = line.substr(0, first - at);
                row.left += bitmap ? LocalizedBitmapLine(*bitmap, *this, hidden).advance()
                                   : text_width(hidden, line_height);
                line = line.substr(first - at, last - first);
            }
            if (bitmap)
                LocalizedBitmapLine(*bitmap, *this, line).draw(target, row, reveal ? std::optional(rgb) : std::nullopt);
            else
                draw(target, line, row, rgb);
        }
    }
} // namespace OpenBlood2
