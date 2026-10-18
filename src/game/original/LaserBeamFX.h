#pragma once
#include "engine/model_store.h"

// Original CLaserBeamFX burst beams (LASER_* types 0..13; Naga red-small, Singularity
// blue-large); source-owned endpoints, detached client lifetime.
inline constexpr DBYTE LASER_RED_SMALL = 0, LASER_BLUE_LARGE = 5;
class CLaserBeamFX
{
  public:
    void SetupBeam(DBYTE type); // Original per-type scale, timing, alpha and texture.
    bool CreateObject(OpenBlood2::ModelStore&, const OpenBlood2::Resources&, DVector, DVector, double, std::string&);
    bool Update(double);
    std::shared_ptr<const OpenBlood2::Model> model;
    OpenBlood2::AnimationTrack animation;
    std::string skin_path = "Skins\\Ammo\\beamred.dtx", error;
    DVector source{}, destination{}, position{}, scale{};
    DRotation rotation;
    DBYTE type = LASER_RED_SMALL;
    float min_scale = .05f, max_scale = 1, duration = .5f, scale_time = .25f, fade_time = .25f, full_alpha = .5f;
    float alpha = .5f;
    double start_time = 0, last_time = 0;
    std::uint64_t id = 0, updates = 0;
};
