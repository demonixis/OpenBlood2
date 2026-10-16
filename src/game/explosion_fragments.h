#pragma once
#include "game/original/ExplosionFragFX.h"
namespace OpenBlood2
{
    struct ExplosionFragmentReport
    {
        std::uint64_t id = 0;
        DDWORD type = 0;
        std::string source, error, unavailable;
        std::vector<std::uint64_t> fragments;
    };
    class ExplosionFragments
    {
      public:
        std::uint64_t publish(DDWORD, DVector, DVector, std::string_view, ModelStore&, const Resources&, double,
                              std::string&);
        bool advance(double, ServerDE&);
        void clear();
        std::vector<CExplosionFragFX> active;
        std::vector<ExplosionFragmentReport> reports;
        std::string update_error;
        std::uint64_t started = 0, expired = 0, failed = 0;

      private:
        std::minstd_rand random_{1};
        std::uint64_t next_id_ = 1, next_event_ = 1;
    };
} // namespace OpenBlood2
