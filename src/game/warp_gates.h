#pragma once
#include "game/legacy/server_de.h"
#include "game/original/WarpGateFX.h"
namespace OpenBlood2
{
    struct WarpGateInstance
    {
        std::uint64_t id = 0;
        HOBJECT owner = nullptr;
        CWarpGateSpriteFX sprite;
        std::array<CWarpGateParticleFX, 2> particles;
        bool sprite_ready = false;
        std::array<bool, 2> particle_ready{};
    };
    struct WarpGateReport
    {
        std::uint64_t id = 0;
        std::string source, error;
        bool ended = false;
    };
    class WarpGates
    {
      public:
        std::uint64_t publish(HOBJECT, const WarpGateSettings&, ServerDE&, SpriteStore&, const Resources&, double,
                              std::string&);
        bool advance(double, ServerDE&);
        void clear();
        std::size_t particle_count() const;
        std::vector<WarpGateInstance> active;
        std::vector<WarpGateReport> reports;
        std::string update_error;
        std::uint64_t started = 0, expired = 0, failed = 0;

      private:
        std::uint64_t next_id_ = 1;
        std::minstd_rand random_{1};
    };
} // namespace OpenBlood2
