#pragma once
#include "engine/texture.h"
#include <list>
#include <memory>
#include <unordered_map>

namespace OpenBlood2
{
    // Replacement tangent-space normal map derived from an albedo texture: height is the
    // texel luminance (transparent texels count as flat), slopes come from a wrapping 3x3
    // Sobel filter scaled by `strength`, and every level is rebuilt from the averaged
    // finer-level normals so distant surfaces flatten instead of aliasing. RGB holds the
    // normal (x right along +u, y along +v, z out of the surface), A holds the height used
    // as specular cavity. Same dimensions and level count as the albedo.
    bool generate_normal_map(const Texture& albedo, float strength, Texture& output, std::string& error);
    struct NormalMapCacheStats
    {
        std::size_t entries = 0, bytes = 0;
        std::uint64_t generated = 0, hits = 0, evicted = 0;
        std::uint64_t generation_ns = 0; // CPU time spent generating, cumulative.
    };
    // Content-addressed in-memory cache of generated maps, kept across world changes so a
    // revisited level or a shared skin never regenerates. Bounded by bytes (least recently
    // used entries are evicted); entries are immutable and shared.
    class NormalMapCache
    {
      public:
        // Returns the cached or newly generated map for this albedo and strength.
        std::shared_ptr<const Texture> get(const Texture& albedo, float strength, std::string& error);
        void clear();
        NormalMapCacheStats stats;
        std::size_t byte_limit = 192u * 1024 * 1024;

      private:
        struct Entry
        {
            std::shared_ptr<const Texture> map;
            std::list<std::uint64_t>::iterator order;
        };
        std::unordered_map<std::uint64_t, Entry> entries_;
        std::list<std::uint64_t> order_; // Most recent first.
    };
    // FNV-1a over dimensions and every decoded level, the registry's content identity.
    std::uint64_t texture_content_hash(const Texture&);
} // namespace OpenBlood2
