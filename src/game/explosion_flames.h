#pragma once
#include "game/original/ExplosionFlameFX.h"
#include <string_view>
namespace OpenBlood2
{
    bool original_explosion_flames(DDWORD, DVector, DVector, std::vector<ExplosionFlameSettings>&, std::string&);
    struct ExplosionFlameReport
    {
        std::uint64_t id = 0;
        DDWORD type = 0;
        std::string source, error;
        std::vector<std::uint64_t> flames;
    };
    class ExplosionFlames
    {
      public:
        std::uint64_t publish(DDWORD, DVector, DVector, std::string_view, const Resources&, double, std::string&);
        bool advance(double);
        void clear();
        std::size_t live_particles() const;
        std::vector<CExplosionFlameFX> active;
        std::vector<ExplosionFlameReport> reports;
        std::string update_error;
        std::uint64_t started = 0, expired = 0, failed = 0;

      private:
        std::minstd_rand random_{1};
        std::uint64_t next_id_ = 1, next_event_ = 1;
    };
} // namespace OpenBlood2
