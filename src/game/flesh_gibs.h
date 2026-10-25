#pragma once
#include "engine/model_store.h"
#include "game/brush_messages.h"
#include "game/weapon_messages.h"
#include <random>
class ServerDE;
namespace OpenBlood2
{
    // Shared original CGibFX motion; flesh and destruction use independent client pools.
    struct FleshGib
    {
        std::uint64_t id = 0, impacts = 0, spurt_event = 0, splat_event = 0;
        std::shared_ptr<const Model> model;
        AnimationTrack animation;
        DVector position{}, velocity{}, scale{1, 1, 1}, angles{}, angular_velocity{};
        DRotation rotation;
        std::string model_path, skin_path, sound_path, error, sound_error, spurt_error, splat_error;
        int bounces = 0;
        bool resting = false, liquid = false, blood = true, fading = false;
        unsigned fade_updates = 0;
        float alpha = 1;
        unsigned surface = 100;
        double last_time = 0;
    };
    struct FleshGibReport
    {
        std::uint64_t id = 0;
        DVector position{}, velocity{}, dimensions{};
        float scale = 1;
        unsigned requested = 0, adjusted = 0, created = 0;
        std::string source, error;
        std::string unavailable = "Stone/plastic/terrain debris and recursive templates remain unavailable";
    };
    class FleshGibs
    {
      public:
        std::uint64_t publish(HOBJECT, DVector, DVector, DVector, float, unsigned, ServerDE&, ModelStore&,
                              const Resources&, double, std::string&);
        std::uint64_t publish_debris(const DebrisEffect&, int detail, ServerDE&, ModelStore&, const Resources&, double,
                                     std::string&);
        bool advance(double, ServerDE&);
        void clear();
        std::vector<FleshGib> active;
        std::vector<FleshGibReport> reports;
        std::string update_error;

      private:
        std::uint64_t publish_impl(HOBJECT, DVector, DVector, DVector, float, unsigned, ServerDE&, ModelStore&,
                                   const Resources&, double, std::string&, const DebrisEffect* = nullptr);
        std::minstd_rand random_{1};
        std::uint64_t next_id_ = 1, next_event_ = 1;
    };
} // namespace OpenBlood2
