#pragma once
#include "game/object_particle_recipe.h"
#include "net/codec.h"
namespace OpenBlood2::Net
{
    inline void write_object_particles(Writer& w, const std::vector<ObjectParticleRecipe>& particles)
    {
        w.u8(std::uint8_t(particles.size())); // Caller validates <=128 before narrowing.
        for (const auto& p : particles)
        {
            w.u64(p.id);
            w.f64(p.created);
            w.u8(std::uint8_t(p.type));
            w.f32(p.lifetime_scale);
            w.f32(p.offset.x);
            w.f32(p.offset.y);
            w.f32(p.offset.z);
            w.text(p.source, 100);
        }
    }
    inline bool read_object_particles(Reader& r, std::vector<ObjectParticleRecipe>& particles)
    {
        const auto count = r.u8();
        if (count > 128)
            return false;
        particles.reserve(count);
        for (unsigned i = 0; i < count; ++i)
        {
            ObjectParticleRecipe p;
            p.id = r.u64();
            p.created = r.f64();
            p.type = r.u8();
            p.lifetime_scale = r.f32();
            p.offset = {r.f32(), r.f32(), r.f32()};
            p.source = r.text(100);
            if (!r.good() || !valid_object_particle_recipe(p) || (!particles.empty() && p.id <= particles.back().id))
                return false;
            particles.push_back(std::move(p));
        }
        return r.good();
    }
} // namespace OpenBlood2::Net
