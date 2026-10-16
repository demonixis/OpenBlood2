#pragma once
#include "engine/particles.h"
#include "engine/resources.h"
#include <array>
#include <optional>
#include <random>
namespace OpenBlood2
{
    struct ExplosionFlameSettings
    {
        DVector position{}, normal{0, 1, 0};
        std::array<DVector, 3> colors{{{1, 1, 0}, {1, .75f, 0}, {1, 0, 0}}};
        std::array<float, 3> lifetimes{.5f, 1, 1.5f}, life_offsets{};
        float ramp_up = 1, duration = 3, ramp_down = 1;
        float radius = 100, position_radius = 0, gravity = 0, velocity = 0, delay = .25f, alpha = 1;
        unsigned count = 20;
        DBYTE fade = 0, ramp_flags = 0;
        std::optional<std::string> texture;
    };
} // namespace OpenBlood2
class CExplosionFlameFX
{
  public:
    bool Init(const OpenBlood2::ExplosionFlameSettings&, std::string&);
    bool CreateObject(const OpenBlood2::Resources&, double, std::minstd_rand&, std::string&, std::size_t budget = 2048);
    bool Update(double, std::minstd_rand&, std::size_t budget = 2048);
    OpenBlood2::ExplosionFlameSettings settings;
    std::vector<OpenBlood2::Particle> particles;
    std::string texture, error;
    float alpha = 1, add_time = 0;
    double start_time = 0, last_time = 0, next_update = 0;
    std::uint64_t id = 0, event = 0, updates = 0, bursts = 0, emitted = 0;

  private:
    bool AddParticles(std::minstd_rand&, std::size_t budget, std::string&);
    bool initialized_ = false, created_ = false;
};
