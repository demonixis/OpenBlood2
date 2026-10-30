#pragma once
#include "client/effect_owner.h"
#include "game/original/ParticleStreamFX.h"
namespace OpenBlood2
{
    struct ParticleStreamInstance
    {
        ClientObjectId owner;
        CParticleStreamFX effect;
    };
    struct ParticleStreamReport
    {
        std::uint64_t id = 0;
        std::string source, error;
        bool removed = false;
    };
    class ParticleStreams
    {
      public:
        std::uint64_t publish(const EffectOwner&, const ParticleStreamSettings&, const Resources*, double,
                              std::string&);
        bool advance(double, const ClientWorld&);
        void clear();
        std::size_t particle_count() const;
        std::vector<ParticleStreamInstance> active;
        std::vector<ParticleStreamReport> reports;
        std::uint64_t started = 0, expired = 0, failed = 0;
        std::string update_error;

      private:
        std::uint64_t next_id_ = 1;
        std::minstd_rand random_{1};
    };
} // namespace OpenBlood2
