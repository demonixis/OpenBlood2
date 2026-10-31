#pragma once
#include "engine/particles.h"
#include "engine/resources.h"
#include "game/impact_settings.h"
#include <random>
namespace OpenBlood2
{
    class EffectQueries;
    struct EffectServices;

} // namespace OpenBlood2
class CParticleExplosionFX
{
  public:
    bool CreateObject(const OpenBlood2::BloodSpurtSettings&, const OpenBlood2::Resources&, OpenBlood2::EffectQueries&,
                      double, std::minstd_rand&, std::string&);
    bool Update(double, double, const OpenBlood2::EffectServices&, std::minstd_rand&, std::size_t);
    OpenBlood2::BloodSpurtSettings settings;
    std::vector<OpenBlood2::Particle> particles;
    DVector position{}, previous{}, velocity{};
    bool liquid = false, emitting = true, first_update = true;
    float alpha = 1;
    double start_time = 0, last_time = 0, last_emission = 0, next_update = 0;
    std::uint64_t id = 0, event = 0, updates = 0, emitted = 0, capacity_skips = 0, impacts = 0, splat_event = 0;
    std::string texture, error, splat_error, sound_resource, sound_error;
    // Replacement policy: at most one burst per interval (the source era's 30 Hz) instead of one
    // per update; see Update.
    static constexpr double emission_interval = 1.0 / 30;
};
