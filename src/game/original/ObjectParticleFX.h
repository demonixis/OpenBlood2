#pragma once
#include "engine/model.h"
#include "engine/particles.h"
#include "engine/resources.h"
#include <random>
// Selected original smoke trail, shield and powerup recipes.
// Other recipes, configurable flags/ramps and multiplayer behavior are not exposed.
class CObjectParticleFX
{
  public:
    bool Init(unsigned type, std::uint64_t parent, std::string&);
    bool Init(unsigned type, std::uint64_t parent, std::minstd_rand&, std::string&);
    bool CreateObject(const OpenBlood2::Resources&, OpenBlood2::NodePose, double, std::string&);
    bool Update(double, const OpenBlood2::NodePose*, std::minstd_rand&, std::size_t particle_budget = 2048);
    std::uint64_t id = 0, parent = 0, event = 0, updates = 0, bursts = 0, emitted = 0;
    std::vector<OpenBlood2::Particle> particles;
    DVector origin{}, position{}, last_position{}, last_object_position{};
    DRotation source_rotation, system_rotation;
    unsigned recipe = 1;
    DVector offset{}, rotations{}, min_color{96, 96, 96}, max_color{192, 192, 192};
    float position_radius = 0, ramp_down = 0, ramp_ratio = 1;
    bool ramping = false;
    double start_time = 0, last_time = 0, next_update = 0, last_add_time = 0;
    float duration = 0;
    bool moving = true, first_update = true;
    // Original recipe fields; alpha is authored but unused without ramp flags.
    float radius = 2000, gravity = 25, density = 1, add_delay = .01f, requested_alpha = .25f, alpha = 1, min_life = .5f,
          max_life = .75f;
    unsigned authored_count = 12, flags = 0xff;
    std::string texture = "SpriteTextures\\smoke64_2.dtx", error;

  private:
    bool initialized_ = false, created_ = false;
    bool AddParticles(std::minstd_rand&, std::size_t);
    bool AddShellParticles(std::minstd_rand&, std::size_t);
    bool AddCylinderParticles(std::minstd_rand&, std::size_t);
};
