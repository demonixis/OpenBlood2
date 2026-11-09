#pragma once
#include "client/effects.h"
#include "engine/resources.h"
#include <map>
#include <random>

namespace OpenBlood2
{
    struct LegacySmokeSource
    {
        DVector position{}, velocity{};
        bool small = false, liquid = false;
    };

    // Local client simulation. Segments continue to fade after their source disappears.
    class LegacySmokeTrails
    {
      public:
        static constexpr std::size_t source_limit = 64;
        void append(const std::map<std::uint64_t, LegacySmokeSource>&, const Resources&, ClientEffectsFrame&);

      private:
        struct Segment
        {
            std::uint64_t owner = 0;
            std::uint64_t id = 0;
            double start = 0, last_add = 0, last_update = 0;
            float radius = 1000, lifetime = .75f, interval = .025f, gravity = 15, alpha = .5f;
            float drift = 10;
            unsigned puff = 10;
            DVector origin{}, last_position{};
            std::string texture;
            std::vector<Particle> particles;
        };
        std::map<std::uint64_t, double> last_segment_;
        std::vector<Segment> segments_;
        std::minstd_rand random_{1};
        std::uint64_t next_id_ = 1;
        void emit(Segment&, DVector position, double now);
    };
}
