#include "engine/particles.h"
#include "engine/collision.h"
#include <algorithm>
#include <cmath>
namespace OpenBlood2
{
    bool advance_particles(std::vector<Particle>& particles, float gravity, double seconds, std::string& error)
    {
        error.clear();
        if (!std::isfinite(seconds) || seconds < 0 || seconds > 60 || !std::isfinite(gravity) ||
            std::abs(gravity) > 10000 || particles.size() > 2048)
        {
            error = "Invalid particle step/budget";
            return false;
        }
        const float dt = static_cast<float>(seconds);
        // Preflight the complete step before changing any particle.
        for (const auto& v : particles)
        {
            if (!physics_vector(v.position) || !physics_vector(v.velocity) || !std::isfinite(v.lifetime) ||
                v.lifetime < 0)
            {
                error = "Invalid particle state";
                return false;
            }
            if (v.lifetime - dt < 0)
                continue;
            if (!physics_vector({v.position.x + v.velocity.x * dt, v.position.y + v.velocity.y * dt,
                                 v.position.z + v.velocity.z * dt}) ||
                !physics_vector({v.velocity.x, v.velocity.y + gravity * dt, v.velocity.z}))
            {
                error = "Particle motion exceeds bounds";
                return false;
            }
        }
        // Later LithTech reference: lifetime first, position before gravity, strict <0 removal.
        std::erase_if(particles,
                      [&](auto& v)
                      {
                          v.lifetime -= dt;
                          if (v.lifetime < 0)
                              return true;
                          v.position = {v.position.x + v.velocity.x * dt, v.position.y + v.velocity.y * dt,
                                        v.position.z + v.velocity.z * dt};
                          v.velocity.y += gravity * dt;
                          return false;
                      });
        return true;
    }
} // namespace OpenBlood2
