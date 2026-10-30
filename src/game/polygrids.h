#pragma once
#include "client/effect_owner.h"
#include "game/original/PolyGridFX.h"
namespace OpenBlood2
{
    struct PolyGridInstance
    {
        ClientObjectId owner;
        CPolyGridFX effect;
    };
    struct PolyGridReport
    {
        std::uint64_t id = 0;
        std::string source, error;
        bool removed = false;
    };
    class PolyGrids
    {
      public:
        std::uint64_t publish(const EffectOwner&, const PolyGridSettings&, SpriteStore&, double, std::string&);
        bool advance(double, const ClientWorld&);
        void clear();
        std::vector<PolyGridInstance> active;
        std::vector<PolyGridReport> reports;
        std::array<float, 4> phases{0, 1, 4, 3};
        std::uint64_t started = 0, expired = 0, failed = 0;
        std::string update_error;

      private:
        std::uint64_t next_id_ = 1;
    };
} // namespace OpenBlood2
