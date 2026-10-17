#pragma once
#include "engine/model.h"
#include "engine/particles.h"
#include "engine/resources.h"
#include <array>
#include <random>
namespace OpenBlood2
{
    struct ParticleStreamSettings
    {
        float radius = 1000, position_radius = 1, min_velocity = 0, max_velocity = 25, spread = 0;
        unsigned count = 1;
        DVector min_color{255, 255, 255}, max_color{255, 255, 255};
        float alpha = 1, min_life = 1, max_life = 2, ramp_time = 0, delay = .1f, gravity = 15;
        DBYTE ramp_flags = 0;
        bool on = false;
        std::string texture = "SpriteTextures\\drop32_1.dtx";
        // Sent by the original server, but never read by its SFXMgr client branch.
        float sound_radius = 200;
        std::array<std::string, 3> sounds;
    };
    bool valid_particle_stream(const ParticleStreamSettings&);
} // namespace OpenBlood2
// Selected original ClientShellDLL/ParticleStreamFX, with checked value-owned services.
class CParticleStreamFX
{
  public:
    bool Init(const OpenBlood2::ParticleStreamSettings&, std::string&);
    bool CreateObject(const OpenBlood2::Resources&, OpenBlood2::NodePose, double, std::minstd_rand&, std::string&,
                      std::size_t budget = 2048);
    bool Update(double, const OpenBlood2::NodePose*, bool visible, std::minstd_rand&, std::size_t budget = 2048);
    OpenBlood2::ParticleStreamSettings settings;
    std::vector<OpenBlood2::Particle> particles;
    DVector position{}, direction{0, 0, 1};
    std::string texture, error;
    unsigned state = 0; // original 0=off, 1=ramp up, 2=on, 3=ramp down
    float trigger_time = 0, last_add = 0;
    double last_time = 0, next_update = 0;
    std::uint64_t id = 0, updates = 0, bursts = 0, emitted = 0;

  private:
    bool AddParticles(float, std::minstd_rand&, std::size_t, std::string&);
    bool initialized_ = false, created_ = false;
};
