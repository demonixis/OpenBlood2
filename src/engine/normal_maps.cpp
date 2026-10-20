#include "engine/normal_maps.h"
#include <SDL3/SDL_timer.h>
#include <algorithm>
#include <cmath>
#include <cstring>

namespace OpenBlood2
{
    namespace
    {
        // Slope per unit strength: a full black-to-white step across one texel tilts ~72 degrees.
        constexpr float relief = 3.f;
        std::uint8_t encode(float value)
        {
            return static_cast<std::uint8_t>(std::clamp(std::lround(value * 255.f), 0l, 255l));
        }
    } // namespace
    std::uint64_t texture_content_hash(const Texture& image)
    {
        std::uint64_t hash = 1469598103934665603ull;
        auto mix = [&](const std::uint8_t* data, std::size_t size)
        {
            for (std::size_t i = 0; i < size; ++i)
                hash = (hash ^ data[i]) * 1099511628211ull;
        };
        const std::uint32_t header[] = {image.width, image.height, image.mipmaps};
        mix(reinterpret_cast<const std::uint8_t*>(header), sizeof(header));
        mix(image.rgba.data(), image.rgba.size());
        mix(image.mip_rgba.data(), image.mip_rgba.size());
        return hash;
    }
    bool generate_normal_map(const Texture& albedo, float strength, Texture& output, std::string& error)
    {
        output = {};
        const std::uint32_t w = albedo.width, h = albedo.height;
        if (!w || !h || w > 1024 || h > 1024 || albedo.mipmaps > 11 ||
            albedo.rgba.size() != std::size_t(w) * h * 4 || albedo.mip_rgba.size() != texture_mip_bytes(albedo) ||
            !std::isfinite(strength) || strength < 0 || strength > 4)
        {
            error = "Invalid normal-map source texture or strength";
            return false;
        }
        std::vector<float> height(std::size_t(w) * h);
        for (std::size_t i = 0; i < height.size(); ++i)
        {
            const auto* p = &albedo.rgba[i * 4];
            height[i] = p[3] ? (.299f * p[0] + .587f * p[1] + .114f * p[2]) / 255.f : .5f;
        }
        // Unnormalized normals of the current level, summed into the next level.
        std::vector<float> normals(std::size_t(w) * h * 3), next;
        output.width = w;
        output.height = h;
        output.mipmaps = albedo.mipmaps;
        output.flags = albedo.flags;
        output.rgba.resize(albedo.rgba.size());
        const float scale = strength * relief / 8.f; // Sobel weights sum to 8 per side.
        auto at = [&](std::int64_t x, std::int64_t y)
        { return height[std::size_t((y + h) % h) * w + std::size_t((x + w) % w)]; };
        for (std::uint32_t y = 0; y < h; ++y)
            for (std::uint32_t x = 0; x < w; ++x)
            {
                const std::int64_t X = x, Y = y;
                const float gx = at(X + 1, Y - 1) + 2 * at(X + 1, Y) + at(X + 1, Y + 1) - at(X - 1, Y - 1) -
                                 2 * at(X - 1, Y) - at(X - 1, Y + 1);
                const float gy = at(X - 1, Y + 1) + 2 * at(X, Y + 1) + at(X + 1, Y + 1) - at(X - 1, Y - 1) -
                                 2 * at(X, Y - 1) - at(X + 1, Y - 1);
                float n[3]{-gx * scale, -gy * scale, 1};
                const float length = std::sqrt(n[0] * n[0] + n[1] * n[1] + n[2] * n[2]);
                const std::size_t i = std::size_t(y) * w + x;
                for (int k = 0; k < 3; ++k)
                {
                    n[k] /= length;
                    normals[i * 3 + k] = n[k];
                    output.rgba[i * 4 + k] = encode(n[k] * .5f + .5f);
                }
                output.rgba[i * 4 + 3] = encode(height[i]);
            }
        output.mip_rgba.resize(albedo.mip_rgba.size());
        std::size_t offset = 0;
        for (std::uint32_t level = 1, pw = w, ph = h; level < std::max(1u, albedo.mipmaps); ++level)
        {
            const std::uint32_t lw = std::max(1u, w >> level), lh = std::max(1u, h >> level);
            next.assign(std::size_t(lw) * lh * 3, 0.f);
            std::vector<float> lower(std::size_t(lw) * lh, 0.f);
            for (std::uint32_t y = 0; y < lh; ++y)
                for (std::uint32_t x = 0; x < lw; ++x)
                {
                    const std::size_t o = std::size_t(y) * lw + x;
                    int samples = 0;
                    for (std::uint32_t dy = 0; dy < 2; ++dy)
                        for (std::uint32_t dx = 0; dx < 2; ++dx)
                        {
                            const std::uint32_t sx = std::min(pw - 1, x * 2 + dx), sy = std::min(ph - 1, y * 2 + dy);
                            const std::size_t s = std::size_t(sy) * pw + sx;
                            for (int k = 0; k < 3; ++k)
                                next[o * 3 + k] += normals[s * 3 + k];
                            lower[o] += height[s];
                            ++samples;
                        }
                    lower[o] /= float(samples);
                    float* n = &next[o * 3];
                    const float length = std::sqrt(n[0] * n[0] + n[1] * n[1] + n[2] * n[2]);
                    for (int k = 0; k < 3; ++k)
                    {
                        n[k] = length > 1e-6f ? n[k] / length : (k == 2 ? 1.f : 0.f);
                        output.mip_rgba[offset + o * 4 + k] = encode(n[k] * .5f + .5f);
                    }
                    output.mip_rgba[offset + o * 4 + 3] = encode(lower[o]);
                }
            offset += std::size_t(lw) * lh * 4;
            normals.swap(next);
            height.swap(lower);
            pw = lw;
            ph = lh;
        }
        return true;
    }
    std::shared_ptr<const Texture> NormalMapCache::get(const Texture& albedo, float strength, std::string& error)
    {
        std::uint32_t bits = 0;
        static_assert(sizeof(bits) == sizeof(strength));
        std::memcpy(&bits, &strength, sizeof(bits));
        const std::uint64_t key = (texture_content_hash(albedo) ^ bits) * 1099511628211ull;
        if (auto found = entries_.find(key); found != entries_.end())
        {
            order_.splice(order_.begin(), order_, found->second.order);
            ++stats.hits;
            return found->second.map;
        }
        const auto start = SDL_GetTicksNS();
        auto map = std::make_shared<Texture>();
        if (!generate_normal_map(albedo, strength, *map, error))
            return nullptr;
        stats.generation_ns += SDL_GetTicksNS() - start;
        ++stats.generated;
        const auto bytes = map->rgba.size() + map->mip_rgba.size();
        while (!order_.empty() && stats.bytes + bytes > byte_limit)
        {
            const auto victim = entries_.find(order_.back());
            stats.bytes -= victim->second.map->rgba.size() + victim->second.map->mip_rgba.size();
            entries_.erase(victim);
            order_.pop_back();
            ++stats.evicted;
        }
        order_.push_front(key);
        entries_.emplace(key, Entry{map, order_.begin()});
        stats.bytes += bytes;
        stats.entries = entries_.size();
        return map;
    }
    void NormalMapCache::clear()
    {
        entries_.clear();
        order_.clear();
        stats.entries = stats.bytes = 0;
    }
} // namespace OpenBlood2
