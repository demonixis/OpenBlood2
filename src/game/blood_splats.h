#pragma once
#include "game/original/BloodSplatFX.h"
namespace OpenBlood2
{
    struct BloodSplatReport
    {
        std::uint64_t id = 0, effect = 0;
        BloodSplatSettings settings;
        bool reused = false;
        std::string source, error;
    };
    class BloodSplats
    {
      public:
        std::uint64_t publish(const BloodSplatSettings&, std::string_view, SpriteStore&, double, std::string&);
        bool advance(double);
        bool terminate(std::uint64_t);
        void clear();
        std::vector<CBloodSplatFX> active;
        std::vector<BloodSplatReport> reports;
        std::uint64_t started = 0, reused = 0, expired = 0, failed = 0;
        std::string update_error;

      private:
        std::minstd_rand random_{1};
        std::uint64_t next_id_ = 1, next_event_ = 1;
    };
} // namespace OpenBlood2
