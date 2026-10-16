#pragma once
#include "game/original/ExplosionFragFX.h"
#include "game/original/ObjectParticleFX.h"
namespace OpenBlood2
{
    struct ExplosionFragmentReport
    {
        std::uint64_t id = 0;
        DDWORD type = 0;
        std::string source, error, unavailable;
        std::vector<std::uint64_t> fragments, trails;
    };
    class ExplosionFragments
    {
      public:
        std::uint64_t publish(DDWORD, DVector, DVector, std::string_view, ModelStore&, const Resources&, double,
                              std::string&);
        bool advance(double, ServerDE&);
        void clear();
        std::vector<CExplosionFragFX> active;
        std::vector<CObjectParticleFX> trails;
        std::uint64_t trail_started = 0, trail_expired = 0, trail_failed = 0;
        std::size_t trail_particles() const;
        std::vector<ExplosionFragmentReport> reports;
        std::string update_error;
        std::uint64_t started = 0, expired = 0, failed = 0;

      private:
        std::minstd_rand random_{1}, trail_random_{1};
        std::uint64_t next_id_ = 1, next_event_ = 1, next_trail_ = 1;
    };
} // namespace OpenBlood2
