#pragma once
#include "engine/particles.h"
#include "engine/resources.h"
#include <random>
class ServerDE;
namespace OpenBlood2
{
    // The original flesh recipe only: one non-bouncing, half-gravity emitter.
    struct BloodSpurtSettings
    {
        DVector position{}, min_velocity{}, max_velocity{};
        float lifetime = 2, fade_time = .5f;
        unsigned texture = 1;
    };
} // namespace OpenBlood2
class CParticleExplosionFX
{
  public:
    bool CreateObject(const OpenBlood2::BloodSpurtSettings&, const OpenBlood2::Resources&, ServerDE&, double,
                      std::minstd_rand&, std::string&);
    bool Update(double, double, ServerDE&, std::minstd_rand&, std::size_t);
    OpenBlood2::BloodSpurtSettings settings;
    std::vector<OpenBlood2::Particle> particles;
    DVector position{}, previous{}, velocity{};
    bool liquid = false, emitting = true, first_update = true;
    float alpha = 1;
    double start_time = 0, last_time = 0, last_emission = 0, next_update = 0;
    std::uint64_t id = 0, event = 0, updates = 0, emitted = 0, capacity_skips = 0, impacts = 0, splat_event = 0;
    std::string texture, error, splat_error, sound_resource, sound_error;
};
