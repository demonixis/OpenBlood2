#include "game/localized_signs.h"
#include "engine/hud_surface.h"
#include "engine/localization.h"
#include "engine/normal_maps.h"
#include "engine/resources.h"
#include "game/localized_font.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <initializer_list>
#include <numbers>
#include <utility>

namespace OpenBlood2
{
    namespace
    {
        struct Texel
        {
            int x, y;
        };

        struct SignText
        {
            std::string_view key;
            HudRect bounds;
            Texel backdrop;
            std::uint32_t ink;
            bool erase_all = false;
            bool erase_only = false;
            double degrees_clockwise = 0;
            unsigned threshold_percent = 25;
            Texel backdrop_offset{};
            std::uint32_t secondary_ink = 0;
            unsigned secondary_threshold_percent = 25;
            unsigned palette_tolerance = 0;
            std::string_view clean_path{};
            std::uint64_t clean_hash = 0;
        };

        struct SignRecipe
        {
            std::string_view path;
            std::uint64_t hash;
            std::initializer_list<SignText> labels;
        };

        struct SignTile
        {
            std::string_view path;
            std::uint64_t hash;
            Texel origin;
        };

        struct SignMural
        {
            std::initializer_list<SignTile> tiles;
            std::initializer_list<SignText> labels;
        };

        // Coordinates are in decoded texels, with exclusive right and bottom edges.
        // Match the complete decoded image, including mip levels, before changing any pixels.
        constexpr std::initializer_list<SignRecipe> recipes{
#include "game/localized_sign_recipes.inc"
        };
        constexpr std::initializer_list<SignMural> murals{
#include "game/localized_sign_murals.inc"
        };

        std::size_t pixel_offset(unsigned width, int x, int y)
        {
            return (std::size_t(y) * width + unsigned(x)) * 4;
        }

        bool contains(const HudRect& bounds, int x, int y)
        {
            return x >= bounds.left && x < bounds.right && y >= bounds.top && y < bounds.bottom;
        }

        struct LabelRegion
        {
            HudRect bounds;
            int width, height;
            double cosine, sine;

            Texel upright(int x, int y) const
            {
                if (!sine)
                    return {x - bounds.left, y - bounds.top};
                const double dx = x + .5 - (bounds.left + bounds.right) / 2.0;
                const double dy = y + .5 - (bounds.top + bounds.bottom) / 2.0;
                return {int(std::floor(dx * cosine + dy * sine + width / 2.0)),
                        int(std::floor(-dx * sine + dy * cosine + height / 2.0))};
            }

            bool covers(int x, int y) const
            {
                const auto [sx, sy] = upright(x, y);
                return sx >= 0 && sx < width && sy >= 0 && sy < height;
            }
        };

        // Separate the original lettering from the paint with its contrast against a clean texel.
        // Work in the direction of that contrast so scratches of unrelated colors remain visible.
        void erase_letters(HudSurface& canvas, const Texture& source, const SignText& label,
                           const LabelRegion& region, bool opaque_skin)
        {
            const auto& bounds = label.bounds;
            const auto* backdrop = source.rgba.data() + pixel_offset(source.width, label.backdrop.x, label.backdrop.y);
            const auto [clone_x, clone_y] = label.backdrop_offset;
            const auto color_contrast = [&](std::uint32_t color, unsigned threshold)
            {
                const std::array delta{int((color >> 16) & 255) - backdrop[0],
                                       int((color >> 8) & 255) - backdrop[1], int(color & 255) - backdrop[2]};
                const int strength = delta[0] * delta[0] + delta[1] * delta[1] + delta[2] * delta[2];
                return std::pair{delta, strength * int(threshold)};
            };
            const auto [ink, ink_cutoff] = color_contrast(label.ink, label.threshold_percent);
            const auto [stroke, stroke_cutoff] = color_contrast(label.secondary_ink, label.secondary_threshold_percent);
            const auto matches = [&](const std::uint8_t* pixel, const std::array<int, 3>& contrast, int cutoff)
            {
                int projection = 0;
                int distance_squared = 0, contrast_squared = 0;
                for (unsigned channel = 0; channel < 3; ++channel)
                {
                    const int delta = int(pixel[channel]) - backdrop[channel];
                    projection += delta * contrast[channel];
                    distance_squared += delta * delta;
                    contrast_squared += contrast[channel] * contrast[channel];
                }
                if (!cutoff || projection * 100 < cutoff)
                    return false;
                // Where an illustration touches the letters, retain colors off the paint-to-background axis.
                return !label.palette_tolerance ||
                       std::int64_t(distance_squared) * contrast_squared - std::int64_t(projection) * projection <=
                           std::int64_t(label.palette_tolerance) * label.palette_tolerance * contrast_squared;
            };
            const auto is_ink = [&](const std::uint8_t* pixel)
            {
                return matches(pixel, ink, ink_cutoff) ||
                       (label.secondary_ink && matches(pixel, stroke, stroke_cutoff));
            };
            constexpr Texel directions[]{{-1, 0}, {1, 0}, {0, -1}, {0, 1}};
            const int sample_distance = label.secondary_ink ? 16 : 8;
            for (int y = bounds.top; y < bounds.bottom; ++y)
                for (int x = bounds.left; x < bounds.right; ++x)
                {
                    if (!region.covers(x, y))
                        continue;
                    const auto offset = pixel_offset(source.width, x, y);
                    if (label.erase_all)
                    {
                        std::copy_n(backdrop, 3, canvas.rgba.data() + offset);
                        continue;
                    }
                    if (!is_ink(source.rgba.data() + offset))
                        continue;
                    if (clone_x || clone_y)
                    {
                        // Some atlases repeat exactly the same artwork in a nearby unlettered panel.
                        const auto* sample = source.rgba.data() + pixel_offset(source.width, x + clone_x, y + clone_y);
                        std::copy_n(sample, 3, canvas.rgba.data() + offset);
                        continue;
                    }
                    std::array<unsigned, 3> color{};
                    unsigned total_weight = 0;
                    for (const auto& [dx, dy] : directions)
                        for (int distance = 1; distance <= sample_distance; ++distance)
                        {
                            const int sx = x + dx * distance, sy = y + dy * distance;
                            if (!contains(bounds, sx, sy) || !region.covers(sx, sy))
                                break;
                            const auto* sample = source.rgba.data() + pixel_offset(source.width, sx, sy);
                            if ((!sample[3] && !opaque_skin) || is_ink(sample))
                                continue;
                            const unsigned weight = 24 / unsigned(distance);
                            for (unsigned channel = 0; channel < 3; ++channel)
                                color[channel] += sample[channel] * weight;
                            total_weight += weight;
                            break;
                        }
                    for (unsigned channel = 0; channel < 3; ++channel)
                        canvas.rgba[offset + channel] = total_weight ? std::uint8_t(color[channel] / total_weight)
                                                                    : backdrop[channel];
                }
        }

        bool draw_text_mask(HudSurface& mask, std::span<const std::string_view> lines, const LocalizedFont& font)
        {
            const int width = int(mask.width);
            const int height = int(mask.height) / int(lines.size());
            const int top = (int(mask.height) - height * int(lines.size())) / 2;
            const int max_advance = width + width / 2;
            for (std::size_t i = 0; i < lines.size(); ++i)
            {
                // Painted sign lettering is often condensed. Fit width independently before
                // reducing the height, so a longer translation still uses the original line area.
                const auto line = lines[i];
                const int advance = font.text_width(line, height);
                const int fitted_height = advance > max_advance ? height * max_advance / advance : height;
                if (fitted_height < 3)
                    return false;
                const int glyph_width = font.text_width(line, fitted_height) + 2;
                const int fitted_width = std::min(width, glyph_width);
                HudSurface glyph{unsigned(glyph_width), unsigned(height),
                                 std::vector<std::uint8_t>(std::size_t(glyph_width) * height * 4)};
                const int offset = (height - fitted_height) / 2;
                font.draw(glyph, line, {0, offset, glyph_width, offset + fitted_height}, 0xffffff);
                const int x_offset = (width - fitted_width) / 2;
                const int y_offset = top + int(i) * height;
                bool visible = false;
                for (int y = 0; y < height; ++y)
                    for (int x = 0; x < fitted_width; ++x)
                    {
                        const int sx = (2 * x + 1) * glyph_width / (2 * fitted_width);
                        unsigned coverage = glyph.rgba[pixel_offset(glyph.width, sx, y)];
                        if (fitted_height >= 7 && sx > 0)
                        {
                            const unsigned neighbor = glyph.rgba[pixel_offset(glyph.width, sx - 1, y)];
                            coverage = std::max(coverage, neighbor / 2);
                        }
                        mask.rgba[pixel_offset(mask.width, x + x_offset, y + y_offset)] = std::uint8_t(coverage);
                        visible |= coverage != 0;
                    }
                if (!visible)
                    return false;
            }
            return true;
        }

        void replace_label(HudSurface& canvas, const Texture& source, const SignText& label,
                           const Localization& catalog, const LocalizedFont& font, bool opaque_skin,
                           const Texture* clean = nullptr)
        {
            const auto& bounds = label.bounds;
            const HudRect image{0, 0, int(source.width), int(source.height)};
            const auto [clone_x, clone_y] = label.backdrop_offset;
            if (!contains(image, bounds.left, bounds.top) || !contains(image, bounds.right - 1, bounds.bottom - 1) ||
                !contains(image, bounds.left + clone_x, bounds.top + clone_y) ||
                !contains(image, bounds.right - 1 + clone_x, bounds.bottom - 1 + clone_y) ||
                !contains(image, label.backdrop.x, label.backdrop.y))
                return;
            auto text = catalog.text(std::string("signs.") + std::string(label.key));
            std::array<std::string_view, 8> lines{};
            std::size_t count = 0;
            while (true)
            {
                const auto end = text.find('|');
                const auto line = text.substr(0, end);
                if (count == lines.size() || line.empty() || !font.supports(line))
                    return;
                lines[count++] = line;
                if (end == text.npos)
                    break;
                text.remove_prefix(end + 1);
            }
            const int box_width = bounds.right - bounds.left, box_height = bounds.bottom - bounds.top;
            if (box_width < 2 || box_height < 2)
                return;
            const double radians = label.degrees_clockwise * std::numbers::pi / 180;
            const double cosine = std::cos(radians), sine = std::sin(radians);
            const double diagonal = cosine * cosine - sine * sine;
            if (cosine <= 0 || std::abs(diagonal) < .25)
                return;
            // Undo the projected bounding box to get the dimensions of the upright text area.
            const int width = int((box_width * cosine - box_height * std::abs(sine)) / diagonal);
            const int text_height = int((box_height * cosine - box_width * std::abs(sine)) / diagonal);
            if (width < 2 || text_height < 3 * int(count))
                return;
            const LabelRegion region{bounds, width, text_height, cosine, sine};

            HudSurface mask;
            if (!label.erase_only)
            {
                mask = {unsigned(width), unsigned(text_height),
                        std::vector<std::uint8_t>(std::size_t(width) * text_height * 4)};
                if (!draw_text_mask(mask, std::span(lines).first(count), font))
                    return;
            }
            if (clean)
            {
                for (int y = bounds.top; y < bounds.bottom; ++y)
                    for (int x = bounds.left; x < bounds.right; ++x)
                    {
                        const auto offset = pixel_offset(source.width, x, y);
                        std::copy_n(clean->rgba.data() + offset, 3, canvas.rgba.data() + offset);
                    }
            }
            else
                erase_letters(canvas, source, label, region, opaque_skin);
            if (label.erase_only)
                return;
            const auto blend = [](std::uint8_t* pixel, std::uint32_t ink, unsigned coverage)
            {
                for (unsigned channel = 0; channel < 3; ++channel)
                {
                    const auto tint = (ink >> (16 - 8 * channel)) & 255;
                    const unsigned color = pixel[channel] * (255 - coverage) + tint * coverage;
                    pixel[channel] = std::uint8_t((color + 127) / 255);
                }
            };
            const int outline = label.secondary_ink ? std::clamp(text_height / int(count) / 24, 1, 3) : 0;
            for (int y = bounds.top; y < bounds.bottom; ++y)
                for (int x = bounds.left; x < bounds.right; ++x)
                {
                    const auto [sx, sy] = region.upright(x, y);
                    if (sx < 0 || sx >= width || sy < 0 || sy >= text_height)
                        continue;
                    const unsigned coverage = mask.rgba[pixel_offset(mask.width, sx, sy)];
                    auto* pixel = canvas.rgba.data() + pixel_offset(canvas.width, x, y);
                    if (outline)
                    {
                        unsigned stroke = 0;
                        for (int dy = std::max(0, sy - outline); dy < std::min(text_height, sy + outline + 1); ++dy)
                            for (int dx = std::max(0, sx - outline); dx < std::min(width, sx + outline + 1); ++dx)
                                stroke = std::max(stroke, unsigned(mask.rgba[pixel_offset(mask.width, dx, dy)]));
                        blend(pixel, label.secondary_ink, stroke);
                    }
                    blend(pixel, label.ink, coverage);
                }
        }

        void update_pixels(Texture& image, HudSurface& canvas, bool opaque_skin)
        {
            // Propagate changes through the authored mip chain. Leave every original alpha byte
            // and RGB texels outside the affected footprint as authored at each level.
            auto& base = canvas.rgba;
            std::vector<std::uint8_t> changed(base.size() / 4);
            bool any_changed = false;
            for (std::size_t i = 0; i < changed.size(); ++i)
            {
                const auto* original = image.rgba.data() + i * 4;
                auto* pixel = base.data() + i * 4;
                if (!original[3] && !opaque_skin)
                    std::copy_n(original, 3, pixel);
                changed[i] = !std::equal(original, original + 3, pixel);
                any_changed |= changed[i] != 0;
            }
            if (!any_changed)
                return;
            image.rgba = std::move(base);
            auto previous = std::span(image.rgba);
            unsigned previous_width = image.width, previous_height = image.height;
            auto mips = std::span(image.mip_rgba);
            for (unsigned level = 1; level < image.mipmaps; ++level)
            {
                const unsigned step_x = previous_width > 1 ? 2 : 1, step_y = previous_height > 1 ? 2 : 1;
                const unsigned width = previous_width / step_x, height = previous_height / step_y;
                const auto current = mips.first(std::size_t(width) * height * 4);
                for (unsigned y = 0; y < height; ++y)
                    for (unsigned x = 0; x < width; ++x)
                    {
                        std::array<unsigned, 3> color{};
                        unsigned coverage = 0;
                        bool touched = false;
                        for (unsigned dy = 0; dy < step_y; ++dy)
                            for (unsigned dx = 0; dx < step_x; ++dx)
                            {
                                const auto row = std::size_t(y * step_y + dy) * previous_width;
                                const auto index = row + x * step_x + dx;
                                const auto* sample = previous.data() + index * 4;
                                const unsigned opacity = opaque_skin ? 255 : sample[3];
                                for (unsigned channel = 0; channel < 3; ++channel)
                                    color[channel] += sample[channel] * opacity;
                                coverage += opacity;
                                touched |= changed[index] != 0;
                            }
                        // A row-major destination index never overtakes its input texels.
                        const std::size_t index = std::size_t(y) * width + x;
                        auto* pixel = current.data() + index * 4;
                        changed[index] = false;
                        if (touched && coverage && (pixel[3] || opaque_skin))
                            for (unsigned channel = 0; channel < 3; ++channel)
                            {
                                const auto value = std::uint8_t((color[channel] + coverage / 2) / coverage);
                                changed[index] |= pixel[channel] != value;
                                pixel[channel] = value;
                            }
                    }
                previous = current;
                previous_width = width;
                previous_height = height;
                mips = mips.subspan(current.size());
            }
        }

        void localize_mural_tile(const Resources& resources, const SignMural& mural, const SignTile& selected,
                                 Texture& image, const Localization& catalog, const LocalizedFont& font)
        {
            const auto intersects = [&](const SignText& label)
            {
                const auto [x, y] = selected.origin;
                return x < label.bounds.right && x + int(image.width) > label.bounds.left &&
                       y < label.bounds.bottom && y + int(image.height) > label.bounds.top;
            };
            if (!std::ranges::any_of(mural.labels, intersects))
                return;
            Texture original;
            for (const auto& tile : mural.tiles)
            {
                const auto [x, y] = tile.origin;
                if (x < 0 || y < 0)
                    return;
                original.width = std::max(original.width, unsigned(x) + image.width);
                original.height = std::max(original.height, unsigned(y) + image.height);
            }
            if (original.width > 1024 || original.height > 1024)
                return;
            original.rgba.resize(std::size_t(original.width) * original.height * 4);
            for (const auto& tile : mural.tiles)
            {
                // A modified or mismatched tile keeps the whole billboard in its mounted form.
                if (localization_resource_overridden(resources, tile.path))
                    return;
                Texture decoded;
                std::string ignored;
                if (&tile != &selected && !read_texture_resource(resources, tile.path, decoded, ignored))
                    return;
                const auto& part = &tile == &selected ? image : decoded;
                if (part.width != image.width || part.height != image.height || texture_content_hash(part) != tile.hash)
                    return;
                for (unsigned y = 0; y < image.height; ++y)
                    std::copy_n(part.rgba.data() + pixel_offset(image.width, 0, int(y)), image.width * 4,
                                original.rgba.data() + pixel_offset(original.width, tile.origin.x,
                                                                    tile.origin.y + int(y)));
            }
            // Set the lettering on the combined artwork before clipping to this tile.
            HudSurface canvas{original.width, original.height, original.rgba};
            for (const auto& label : mural.labels)
                if (intersects(label))
                    replace_label(canvas, original, label, catalog, font, false);
            HudSurface cropped{image.width, image.height, image.rgba};
            for (unsigned y = 0; y < image.height; ++y)
                std::copy_n(canvas.rgba.data() + pixel_offset(canvas.width, selected.origin.x,
                                                              selected.origin.y + int(y)),
                            image.width * 4, cropped.rgba.data() + pixel_offset(image.width, 0, int(y)));
            update_pixels(image, cropped, false);
        }
    } // namespace

    void localize_sign_texture(const Resources& resources, std::string_view path, Texture& image,
                               const Localization& catalog, const LocalizedFont& font)
    {
        if (catalog.language() == Language::English || !font)
            return;
        std::string normalized;
        if (!resource_path(path, normalized))
            return;
        const auto first = std::ranges::find(recipes, normalized, &SignRecipe::path);
        if (first == recipes.end())
        {
            for (const auto& mural : murals)
            {
                const auto tile = std::ranges::find(mural.tiles, normalized, &SignTile::path);
                if (tile != mural.tiles.end())
                {
                    localize_mural_tile(resources, mural, *tile, image, catalog, font);
                    return;
                }
            }
            return;
        }
        if (localization_resource_overridden(resources, normalized))
            return;
        const auto hash = texture_content_hash(image);
        const auto recipe = std::find_if(first, recipes.end(), [&](const SignRecipe& candidate)
        { return candidate.path == normalized && candidate.hash == hash; });
        if (recipe == recipes.end())
            return;
        // The model view treats old skins with zero palette alpha throughout as opaque.
        // Decorate their RGB with the same assumption while leaving the decoder's alpha intact.
        bool opaque_skin = normalized.starts_with("skins/") || normalized.starts_with("skins_ao/");
        for (std::size_t a = 3; a < image.rgba.size() && opaque_skin; a += 4)
            opaque_skin = image.rgba[a] == 0;
        HudSurface canvas{image.width, image.height, image.rgba};
        for (const auto& label : recipe->labels)
        {
            // Repeated prop artwork can provide a clean patch with the same wood or metal grain.
            Texture clean;
            std::string ignored;
            const bool use_clean =
                !label.clean_path.empty() && !localization_resource_overridden(resources, label.clean_path) &&
                read_texture_resource(resources, label.clean_path, clean, ignored) && clean.width == image.width &&
                clean.height == image.height && texture_content_hash(clean) == label.clean_hash;
            replace_label(canvas, image, label, catalog, font, opaque_skin, use_clean ? &clean : nullptr);
        }
        update_pixels(image, canvas, opaque_skin);
    }

    TextureDecoration sign_texture_decoration(const Localization& catalog, const LocalizedFont& font)
    {
        if (catalog.language() == Language::English || !font)
            return {};
        return [&catalog, &font](const Resources& resources, std::string_view path, Texture& image)
        { localize_sign_texture(resources, path, image, catalog, font); };
    }
} // namespace OpenBlood2
