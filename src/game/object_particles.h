#pragma once
#include "game/legacy/server_de.h"
#include "game/original/ObjectParticleFX.h"
namespace OpenBlood2
{
    struct ObjectParticleInstance
    {
        HOBJECT owner = nullptr; // Opaque, non-owning; resolved before every update.
        CObjectParticleFX effect;
    };
    struct ObjectParticleReport
    {
        std::uint64_t id = 0;
        unsigned recipe = 0;
        std::string source, error;
    };
    class ObjectParticles
    {
      public:
        std::uint64_t publish(HOBJECT, unsigned, ServerDE&, const Resources*, double, std::string&,
                              float lifetime_scale = 1, DVector offset = {});
        bool advance(double, ServerDE&);
        void clear();
        std::size_t particle_count() const;
        std::vector<ObjectParticleInstance> active;
        std::vector<ObjectParticleReport> reports;
        std::uint64_t started = 0, expired = 0, failed = 0;
        std::string update_error;

      private:
        std::uint64_t next_id_ = 1;
        std::minstd_rand random_{1};
    };
} // namespace OpenBlood2
