#pragma once
#include "engine/particles.h"
#include "engine/resources.h"
#include <random>
namespace OpenBlood2
{
    // Selected original WeaponFX metal/flesh recipes; no generic fade-color mode.
    struct SparkSettings
    {
        DVector position{}, direction{}, color1{}, color2{};
        unsigned count = 0;
        float duration = 0, emission_radius = 0, radius = 0, gravity = 0;
        std::string texture;
    };
} // namespace OpenBlood2
class CSparksFX
{
  public:
    bool CreateObject(const OpenBlood2::SparkSettings&, const OpenBlood2::Resources&, double, std::minstd_rand&,
                      std::string&);
    bool Update(double);
    OpenBlood2::SparkSettings settings;
    std::vector<OpenBlood2::Particle> particles;
    DVector direction{}, min_velocity{}, max_velocity{};
    double start_time = 0, last_time = 0, next_update = 0;
    float alpha = 1;
    bool want_remove = false;
    std::uint64_t id = 0, event = 0, updates = 0;
    std::string error;
};
