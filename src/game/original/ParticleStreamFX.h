#pragma once
#include "engine/model.h"
#include "engine/particles.h"
#include "engine/resources.h"
#include "game/particle_stream_settings.h"
#include "game/rain_settings.h"
#include <optional>
#include <random>
// Selected original ClientShellDLL/ParticleStreamFX, with checked value-owned services.
class CParticleStreamFX
{
  public:
    bool Init(const OpenBlood2::ParticleStreamSettings&, std::string&);
    bool Init(const OpenBlood2::RainSettings&, std::string&);
    bool CreateObject(const OpenBlood2::Resources&, OpenBlood2::NodePose, double, std::minstd_rand&, std::string&,
                      std::size_t budget = 2048);
    bool Update(double, const OpenBlood2::NodePose*, bool visible, std::minstd_rand&, std::size_t budget = 2048);
    OpenBlood2::ParticleStreamSettings settings;
    float alpha = 1;
    std::vector<OpenBlood2::Particle> particles;
    DVector position{}, direction{0, 0, 1};
    DRotation system_rotation{};
    bool system_visible = true;
    std::string texture, error;
    unsigned state = 0; // original 0=off, 1=ramp up, 2=on, 3=ramp down
    float trigger_time = 0, last_add = 0;
    double last_time = 0, next_update = 0;
    std::uint64_t id = 0, updates = 0, bursts = 0, emitted = 0;
    std::uint64_t dropped = 0;

  private:
    bool AddParticles(float, std::minstd_rand&, std::size_t, std::string&);
    bool SystemUpdate(double, double, const OpenBlood2::NodePose&, bool, std::minstd_rand&, std::size_t);
    bool RainUpdate(double, const OpenBlood2::NodePose&, std::minstd_rand&, std::size_t);
    std::optional<OpenBlood2::RainSettings> rain_;
    double rain_start_ = 0;
    std::uint64_t rain_samples_ = 0;
    bool first_system_update_ = true;
    double system_last_ = 0, system_wait_ = .01;
    bool initialized_ = false, created_ = false;
};
