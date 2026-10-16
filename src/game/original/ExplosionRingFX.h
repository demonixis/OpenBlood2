#pragma once
#include "engine/particles.h"
#include "engine/resources.h"
#include <optional>
namespace OpenBlood2
{
    struct ExplosionRingSettings
    {
        DVector position{}, normal{0, 1, 0}, color{1, 1, 1};
        float radius = 100, position_radius = 0, velocity = 1, gravity = 0, duration = 1, rotation = 0, alpha = 1,
              delay = 0;
        unsigned count = 8;
        DBYTE fade = 0, rotate = 0, align = 0;
        std::optional<std::string> texture;
    };
} // namespace OpenBlood2
class CExplosionRingFX
{
  public:
    bool Init(const OpenBlood2::ExplosionRingSettings&, std::string&);
    bool CreateObject(const OpenBlood2::Resources&, double now, std::string&);
    bool Update(double now);
    OpenBlood2::ExplosionRingSettings settings;
    std::vector<OpenBlood2::Particle> particles;
    DRotation rotation;
    std::string texture, error;
    float alpha = 1, delay = 0;
    double start_time = 0, last_time = 0, next_update = 0;
    std::uint64_t id = 0, event = 0, updates = 0;

  private:
    bool AddParticles();
    bool initialized_ = false, created_ = false;
};
