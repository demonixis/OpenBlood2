#pragma once
#include "engine/model_store.h"
#include <optional>
#include <random>
namespace OpenBlood2
{
    struct ExplosionModelSettings
    {
        DVector position{}, normal{0, 1, 0}, scale1{}, scale2{1, 1, 1}, rotations{};
        float duration = 1, alpha = 1;
        DBYTE waveform = 0, fade = 0, random_rotation = 0;
        std::optional<std::string> model, skin;
    };
} // namespace OpenBlood2
// Selected original ExplosionFX.cpp model methods; typed client visual ownership.
class CExplosionModelFX
{
  public:
    bool Init(const OpenBlood2::ExplosionModelSettings&, std::string&);
    bool CreateObject(OpenBlood2::ModelStore&, const OpenBlood2::Resources&, double, std::minstd_rand&, std::string&);
    bool Update(double);
    OpenBlood2::ExplosionModelSettings settings;
    std::shared_ptr<const OpenBlood2::Model> model;
    OpenBlood2::AnimationTrack animation;
    std::string model_path, skin_path, error;
    DRotation rotation;
    DVector scale{};
    float alpha = 1;
    double start_time = 0, last_time = 0, next_update = 0;
    std::uint64_t id = 0, event = 0, updates = 0;
    DDWORD flags = FLAG_VISIBLE | FLAG_MODELGOURAUDSHADE | FLAG_NOLIGHT;

  private:
    bool initialized_ = false, scale_changes_ = false;
};
