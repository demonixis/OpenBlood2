#pragma once
#include "game/original/ExplosionLightFX.h"
#include <string_view>
#include <vector>
namespace OpenBlood2
{
    bool original_explosion_lights(DDWORD, DVector, DVector, std::vector<ExplosionLightSettings>&, std::string&);
    struct ExplosionLightReport
    {
        std::uint64_t id = 0;
        DDWORD type = 0;
        std::string source, error;
        std::vector<std::uint64_t> lights;
    };
    class ExplosionLights
    {
      public:
        std::uint64_t publish(DDWORD, DVector, DVector, std::string_view, double, std::string&);
        bool advance(double now);
        void clear();
        std::vector<CExplosionLightFX> active;
        std::vector<ExplosionLightReport> reports;
        std::string update_error;
        std::uint64_t started = 0, expired = 0, failed = 0;

      private:
        std::uint64_t next_id_ = 1, next_event_ = 1;
    };
} // namespace OpenBlood2
