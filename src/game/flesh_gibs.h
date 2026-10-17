#pragma once
#include "engine/model_store.h"
#include "game/weapon_messages.h"
#include <random>
class ServerDE;
namespace OpenBlood2
{
    // Selected original CGibFX flesh path. Client-only models never block gameplay.
    struct FleshGib
    {
        std::uint64_t id = 0, impacts = 0;
        std::shared_ptr<const Model> model;
        AnimationTrack animation;
        DVector position{}, velocity{}, scale{1, 1, 1}, angles{}, angular_velocity{};
        DRotation rotation;
        std::string model_path, skin_path, sound_path, error, sound_error;
        int bounces = 0;
        bool resting = false, liquid = false;
        double last_time = 0;
    };
    struct FleshGibReport
    {
        std::uint64_t id = 0;
        DVector position{}, velocity{}, dimensions{};
        float scale = 1;
        unsigned requested = 0, adjusted = 0, created = 0;
        std::string source, error;
        std::string unavailable = "Original blood-spurt particles and impact splats are not rendered";
    };
    class FleshGibs
    {
      public:
        std::uint64_t publish(HOBJECT, DVector, DVector, DVector, float, unsigned, ServerDE&, ModelStore&,
                              const Resources&, double, std::string&);
        bool advance(double, ServerDE&);
        void clear();
        std::vector<FleshGib> active;
        std::vector<FleshGibReport> reports;
        std::string update_error;

      private:
        std::minstd_rand random_{1};
        std::uint64_t next_id_ = 1, next_event_ = 1;
    };
} // namespace OpenBlood2
