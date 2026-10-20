#pragma once
#include "game/original/ExplosionModelFX.h"
namespace OpenBlood2
{
    bool original_explosion_model(DDWORD, DVector, DVector, std::minstd_rand&, std::optional<ExplosionModelSettings>&,
                                  std::string&);
    struct ExplosionModelReport
    {
        std::uint64_t id = 0, model = 0;
        DDWORD type = 0;
        std::string source, error;
        std::vector<std::uint64_t> models;
    };
    class ExplosionModels
    {
      public:
        std::uint64_t publish(DDWORD, DVector, DVector, std::string_view, ModelStore&, const Resources&, double,
                              std::string&);
        bool advance(double);
        void clear();
        std::vector<CExplosionModelFX> active;
        std::vector<ExplosionModelReport> reports;
        std::string update_error;
        std::uint64_t started = 0, expired = 0, failed = 0;

      private:
        std::minstd_rand random_{1}; // Client visual stream, separate from original server gameplay RNG.
        std::uint64_t next_id_ = 1, next_event_ = 1;
    };
} // namespace OpenBlood2
