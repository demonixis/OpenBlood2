#pragma once
#include "game/original/LaserBeamFX.h"
namespace OpenBlood2
{
    struct LaserBeamReport
    {
        std::uint64_t id = 0;
        std::string source, error;
        DVector from{}, to{};
    };
    class LaserBeams
    {
      public:
        std::uint64_t publish(DVector, DVector, std::string_view, ModelStore&, const Resources&, double, std::string&);
        bool advance(double);
        void clear();
        std::vector<CLaserBeamFX> active;
        std::vector<LaserBeamReport> reports;
        std::uint64_t started = 0, expired = 0, failed = 0;
        std::string update_error;

      private:
        std::uint64_t next_id_ = 1;
    };
} // namespace OpenBlood2
