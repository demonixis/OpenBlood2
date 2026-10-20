#pragma once
#include "game/original/LightningFX.h"
namespace OpenBlood2
{
    struct LightningReport
    {
        std::uint64_t id = 0;
        std::string source, error;
        LightningSettings settings;
        unsigned segments = 0, particles = 0;
        bool ended = false;
    };
    class LightningBolts
    {
      public:
        std::uint64_t publish(const LightningSettings&, std::string_view, const Resources&, double, std::string&);
        bool advance(double);
        void clear();
        std::size_t live_particles() const;
        std::vector<CLightningBoltFX> active;
        std::vector<LightningReport> reports;
        std::string update_error;
        std::uint64_t started = 0, expired = 0, failed = 0;

      private:
        std::uint64_t next_id_ = 1;
        std::minstd_rand random_{1};
    };
} // namespace OpenBlood2
