#pragma once
#include "engine/model_store.h"

// Selected original LASER_RED_SMALL; source-owned endpoints, detached client lifetime.
class CLaserBeamFX
{
  public:
    bool CreateObject(OpenBlood2::ModelStore&, const OpenBlood2::Resources&, DVector, DVector, double, std::string&);
    bool Update(double);
    std::shared_ptr<const OpenBlood2::Model> model;
    OpenBlood2::AnimationTrack animation;
    std::string skin_path = "Skins\\Ammo\\beamred.dtx", error;
    DVector source{}, destination{}, position{}, scale{};
    DRotation rotation;
    float alpha = .5f;
    double start_time = 0, last_time = 0;
    std::uint64_t id = 0, updates = 0;
};
