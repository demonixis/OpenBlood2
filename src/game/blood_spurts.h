#pragma once
#include "game/original/ParticleExplosionFX.h"
namespace OpenBlood2
{
    struct BloodSpurtReport
    {
        std::uint64_t id = 0, effect = 0, emitted = 0, impacts = 0, splat_event = 0;
        BloodSpurtSettings settings;
        bool immediate = false, ended = false;
        std::string source, error, splat_error, sound_resource, sound_error;
    };
    class BloodSpurts
    {
      public:
        std::uint64_t publish(const BloodSpurtSettings&, std::string_view, bool, const Resources&, ServerDE&, double,
                              std::string&);
        bool advance(double, ServerDE&);
        void clear();
        std::size_t live_particles() const;
        std::vector<CParticleExplosionFX> active;
        std::vector<BloodSpurtReport> reports;
        std::uint64_t started = 0, expired = 0, failed = 0;
        std::string update_error;

      private:
        void record(const CParticleExplosionFX&, bool);
        std::minstd_rand random_{1};
        std::uint64_t next_id_ = 1, next_event_ = 1;
    };
} // namespace OpenBlood2
