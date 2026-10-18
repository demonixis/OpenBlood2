#pragma once
#include "game/original/ExplosionSpriteFX.h"
namespace OpenBlood2
{
    // Selected original CExplosionFX detail >=2 sprite branches, not the full composite.
    bool original_explosion_sprites(DDWORD type, DVector position, DVector normal,
                                    std::vector<ExplosionSpriteSettings>&, std::string& unavailable,
                                    std::string& error);
    struct ExplosionSpriteReport
    {
        std::uint64_t id = 0;
        DDWORD type = 0;
        std::string source, unavailable, error;
        std::vector<std::uint64_t> sprites;
    };
    class ExplosionSprites
    {
      public:
        std::uint64_t publish(DDWORD, DVector, DVector, std::string_view, SpriteStore&, double, std::string&);
        bool advance(double now);
        void clear();
        std::vector<CExplosionSpriteFX> active;
        std::vector<ExplosionSpriteReport> reports;
        std::uint64_t started = 0, expired = 0, failed = 0;
        std::string update_error;

      private:
        std::uint64_t next_id_ = 1, next_event_ = 1;
    };
} // namespace OpenBlood2
