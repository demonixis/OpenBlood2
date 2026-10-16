#pragma once
#include "basedefs_de.h"
#include <cstdint>
#include <string>
namespace OpenBlood2
{
    struct ExplosionLightSettings
    {
        DVector position{}, color1{1, 1, 0}, color2{1, 1, 1};
        float duration = 1, delay = 0, radius1 = 50, radius2 = 100;
    };
} // namespace OpenBlood2
// Selected original ExplosionFX.cpp methods; owned client point-light state.
class CExplosionLightFX
{
  public:
    bool Init(const OpenBlood2::ExplosionLightSettings&, std::string&);
    bool CreateObject(double now, std::string&);
    bool Update(double now);
    OpenBlood2::ExplosionLightSettings settings;
    DVector color{};
    float radius = 0, delay = 0;
    double start_time = 0, last_time = 0, next_update = 0;
    std::uint64_t id = 0, event = 0, updates = 0;
    DDWORD flags = FLAG_VISIBLE;
    std::string error;

  private:
    bool initialized_ = false, created_ = false, change_color_ = false, scale_ = false;
};
