#pragma once
#include "game/legacy/basedefs_de.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>
namespace OpenBlood2
{
    // Source recipe metadata needed by a headless publisher; no random draw or runtime instance.
    inline std::string_view object_particle_texture(unsigned recipe)
    {
        switch (recipe)
        {
            case 1:
            case 2:
            case 21:
            case 36:
            case 37:
                return "SpriteTextures\\smoke64_2.dtx";
            case 3:
            case 24:
                return "SpriteTextures\\Firepart64.dtx";
            case 4:
            case 6:
            case 8:
            case 9:
            case 31:
            case 33:
                return "SpriteTextures\\Lensflare32_1.dtx";
            case 12:
            case 13:
            case 14:
            case 15:
            case 16:
            case 17:
            case 18:
            case 19:
            case 38:
            case 39:
                return "SpriteTextures\\Lensflare_1.dtx";
            case 27:
            case 32:
                return "SpriteTextures\\flames1_09.dtx";
            default:
                return {};
        }
    }
    inline float object_particle_duration(unsigned recipe)
    {
        switch (recipe)
        {
            case 9:
            case 37:
            case 38:
                return 1.5f;
            case 12:
            case 13:
            case 14:
            case 15:
            case 16:
            case 17:
            case 39:
                return .75f;
            case 18:
            case 36:
                return 2.5f;
            case 19:
                return 5;
            case 21:
                return 2;
            default:
                return 0;
        }
    }
    struct ObjectParticleRecipe
    {
        std::uint64_t id = 0;
        double created = 0;
        unsigned type = 0;
        float lifetime_scale = 1;
        DVector offset{};
        std::string source;
    };
    inline bool valid_object_particle_recipe(const ObjectParticleRecipe& p)
    {
        const auto vector = [](DVector v)
        {
            return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z) && std::abs(v.x) <= 1e7f &&
                   std::abs(v.y) <= 1e7f && std::abs(v.z) <= 1e7f;
        };
        return p.id && std::isfinite(p.created) && p.created >= 0 && p.created <= 1e12 &&
               !object_particle_texture(p.type).empty() && std::isfinite(p.lifetime_scale) &&
               p.lifetime_scale >= .25f && p.lifetime_scale <= 2 &&
               ((p.type == 3 || p.type == 4 || p.type == 27 || p.type == 31) || p.lifetime_scale == 1) &&
               vector(p.offset) && ((p.type == 37 || p.type == 38) || (!p.offset.x && !p.offset.y && !p.offset.z)) &&
               p.source.size() <= 100 &&
               std::all_of(p.source.begin(), p.source.end(), [](unsigned char c) { return c >= 32 && c != 127; });
    }
    inline bool same_object_particle(const ObjectParticleRecipe& a, const ObjectParticleRecipe& b)
    {
        return a.id == b.id && a.created == b.created && a.type == b.type && a.lifetime_scale == b.lifetime_scale &&
               a.offset.x == b.offset.x && a.offset.y == b.offset.y && a.offset.z == b.offset.z && a.source == b.source;
    }
    inline bool same_object_particles(const std::vector<ObjectParticleRecipe>& a,
                                      const std::vector<ObjectParticleRecipe>& b)
    {
        return a.size() == b.size() && std::equal(a.begin(), a.end(), b.begin(), same_object_particle);
    }
} // namespace OpenBlood2
