#pragma once
#include "game/original/PolyGridFX.h"
class ServerDE;
namespace OpenBlood2
{
    struct PolyGridInstance
    {
        HOBJECT owner = nullptr;
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
        std::uint64_t publish(HOBJECT, const PolyGridSettings&, ServerDE&, SpriteStore&, double, std::string&);
        bool advance(double, ServerDE&);
        void clear();
        std::vector<PolyGridInstance> active;
        std::vector<PolyGridReport> reports;
        std::array<float, 4> phases{0, 1, 4, 3};
        std::uint64_t started = 0, expired = 0, failed = 0;

      private:
        std::uint64_t next_id_ = 1;
    };
} // namespace OpenBlood2
