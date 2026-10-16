#pragma once
#include "game/original/SmokeImpactFX.h"
namespace OpenBlood2
{
    struct WeaponSpriteSettings
    {
        std::optional<ExplosionSpriteSettings> flash;
        std::optional<SmokeImpactSettings> smoke;
        bool bubbles_unavailable = false;
    };
    struct WeaponSpriteReport
    {
        std::uint64_t id = 0, flash = 0, smoke = 0;
        std::string source, error;
        WeaponSpriteSettings settings;
    };
    class WeaponSprites
    {
      public:
        std::uint64_t publish(const WeaponSpriteSettings&, std::string_view, SpriteStore&, double, std::string&);
        bool advance(double);
        void clear();
        std::vector<CExplosionSpriteFX> flashes;
        std::vector<CSmokeImpactFX> smoke;
        std::vector<WeaponSpriteReport> reports;
        std::uint64_t started = 0, expired = 0, failed = 0;
        std::string update_error;

      private:
        std::uint64_t next_id_ = 1, next_event_ = 1;
    };
} // namespace OpenBlood2
