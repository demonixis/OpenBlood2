#pragma once
#include "engine/model_store.h"
#include "game/weapon_messages.h"
#include <optional>
#include <random>
class ServerDE;
namespace OpenBlood2
{
    struct ExplosionFragSettings
    {
        DVector position{}, normal{0, 1, 0}, scale{.1f, .1f, .1f}, rotate_max{};
        float spread = 0, duration = 5, velocity = 1, bounce = .25f, gravity = 10, fade_time = 1, alpha = 1;
        bool random_direction = false;
        unsigned spawn_explosion = 0, spawn_type = 0, trail_type = 0;
        std::optional<std::string> model, skin;
    };
} // namespace OpenBlood2
class CExplosionFragFX
{
  public:
    bool Init(const OpenBlood2::ExplosionFragSettings&, std::string&);
    bool CreateObject(OpenBlood2::ModelStore&, const OpenBlood2::Resources&, double, std::minstd_rand&, std::string&);
    bool Update(double, ServerDE&);
    OpenBlood2::ExplosionFragSettings settings;
    std::shared_ptr<const OpenBlood2::Model> model;
    OpenBlood2::AnimationTrack animation;
    std::string model_path, skin_path, error, trail_error;
    DVector position{}, last_position{}, velocity{}, last_normal{}, angles{}, angular_step{};
    DRotation rotation;
    float alpha = 1;
    double start_time = 0, last_time = 0, next_update = 0;
    std::uint64_t id = 0, event = 0, updates = 0, impacts = 0;
    OpenBlood2::SegmentHit last_hit;
    OpenBlood2::SegmentResult last_query = OpenBlood2::SegmentResult::Miss;
    bool moving = true;
    DDWORD flags = FLAG_VISIBLE | FLAG_MODELGOURAUDSHADE | FLAG_NOLIGHT;

  private:
    bool initialized_ = false, rotate_ = false;
};
