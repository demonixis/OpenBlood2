#pragma once
#include "game/original/ExplosionRingFX.h"
#include <random>
#include <string_view>
#include <vector>
namespace OpenBlood2
{
    bool original_explosion_rings(DDWORD, DVector, DVector, std::minstd_rand&, std::vector<ExplosionRingSettings>&,
                                  std::string&);
    struct ExplosionRingReport
    {
        std::uint64_t id = 0;
        DDWORD type = 0;
        std::string source, error;
        std::vector<std::uint64_t> rings;
    };
    class ExplosionRings
    {
      public:
        std::uint64_t publish(DDWORD, DVector, DVector, std::string_view, const Resources&, double, std::string&);
        bool advance(double now);
        void clear();
        std::size_t reserved_particles() const;
        std::vector<CExplosionRingFX> active;
        std::vector<ExplosionRingReport> reports;
        std::string update_error;
        std::uint64_t started = 0, expired = 0, failed = 0;

      private:
        std::minstd_rand random_{1};
        std::uint64_t next_id_ = 1, next_event_ = 1;
    };
} // namespace OpenBlood2
