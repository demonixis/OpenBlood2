#pragma once
#include "ExplosionLightFX.h"
#include "engine/particles.h"
#include "engine/resources.h"
#include <random>
namespace OpenBlood2
{
    struct LightningSettings
    {
        DVector source{}, destination{};
        DBYTE shape = 2, form = 1, type = 2;
    };
    struct LightningSegment
    {
        DVector from{}, to{}, color{};
        float radius = 0, increment = 0;
        std::vector<Particle> particles; // World positions, zero velocity.
    };
} // namespace OpenBlood2
// Selected original LightningFX/LightningSegmentFX. Detached client ownership.
class CLightningBoltFX
{
  public:
    bool CreateObject(const OpenBlood2::LightningSettings&, const OpenBlood2::Resources&, double, std::minstd_rand&,
                      std::string&);
    bool Update(double);
    OpenBlood2::LightningSettings settings;
    std::vector<OpenBlood2::LightningSegment> segments;
    std::vector<CExplosionLightFX> lights;
    std::string texture = "SpriteTextures\\lightn32.dtx", error;
    double start_time = 0, last_time = 0;
    float alpha = .75f;
    std::uint64_t id = 0;
};
