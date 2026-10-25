#pragma once
#include "engine/model_store.h"
#include "engine/particles.h"
#include "engine/sprite.h"
#include "game/original/SurfaceTypes.h"
#include <random>
class ServerDE;
namespace OpenBlood2
{
    // Original WeaponFX payload after UpdateWeaponFX has consumed the one-shot ejection bit.
    struct ShotgunEffectSettings
    {
        DVector source{}, destination{}, forward{}, normal{}, color1{}, color2{};
        SurfaceType surface = SURFTYPE_UNKNOWN;
        bool eject = false, left = false;
    };
    struct ShotgunCasing
    {
        std::shared_ptr<const Model> model;
        AnimationTrack animation;
        DVector position{}, velocity{};
        DRotation rotation{};
        float pitch = 0, yaw = 0, pitch_velocity = 0, yaw_velocity = 0;
        double last_time = 0, expires = 0;
        int bounces = 2;
        bool liquid = false, resting = false, visible = false;
        std::uint64_t id = 0, impacts = 0;
        std::string error, sound_error;
    };
    enum class ShotgunSpriteKind
    {
        Mark,
        Fragment,
        Ripple
    };
    struct ShotgunSprite
    {
        ShotgunSpriteKind kind = ShotgunSpriteKind::Mark;
        std::shared_ptr<const Sprite> sprite;
        SpriteTrack track;
        DVector position{}, velocity{}, gravity{}, scale{.035f, .035f, .035f}, color{1, 1, 1};
        DRotation rotation{};
        float pitch = 0, yaw = 0, pitch_velocity = 0, yaw_velocity = 0, alpha = 1, duration = 0;
        double start = -1, last_time = 0;
        std::uint64_t id = 0;
        std::string resource;
    };
    struct ShotgunSplash
    {
        DVector position{}, normal{}, color1{}, color2{};
        std::vector<Particle> particles;
        double start = 0, last_time = 0;
        std::uint64_t id = 0;
        std::string error;
    };
    struct ShotgunEffectReport
    {
        std::uint64_t id = 0;
        ShotgunEffectSettings settings;
        unsigned casings = 0, fragments = 0, marks = 0, splashes = 0, bubbles = 0;
        std::uint64_t smoke = 0;
        std::string error;
    };
    class ShotgunEffects
    {
      public:
        std::uint64_t publish(HOBJECT, const ShotgunEffectSettings&, ModelStore&, SpriteStore&, const Resources&,
                              double, ServerDE&, std::string&);
        void advance(double, ServerDE&);
        void clear();
        std::vector<ShotgunCasing> casings;
        std::vector<ShotgunSprite> sprites;
        std::vector<ShotgunSplash> splashes;
        std::vector<ShotgunEffectReport> reports;
        std::string update_error;
        std::uint64_t started = 0, failed = 0;

      private:
        float random(float, float);
        unsigned integer(unsigned, unsigned);
        void emit(ShotgunSplash&);
        std::minstd_rand random_{1};
        std::uint64_t next_id_ = 1, next_event_ = 1;
    };
} // namespace OpenBlood2
