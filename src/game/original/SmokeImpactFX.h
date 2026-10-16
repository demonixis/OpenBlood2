#pragma once
#include "ExplosionSpriteFX.h"
namespace OpenBlood2
{
    struct SmokeImpactSettings
    {
        DVector position{}, normal{}, velocity{}, deceleration{}, gravity{}, scale1{}, scale2{}, color{};
        float offset = 0, duration = .3f, delay = 0, alpha = 0;
        bool fade = false, rotate = false, move = false, update_scale = true;
        std::optional<std::string> resource;
    };
} // namespace OpenBlood2
// Selected original SmokeImpactFX.cpp lifecycle; owned sprite replaces client object/string handles.
class CSmokeImpactFX
{
  public:
    bool Init(const OpenBlood2::SmokeImpactSettings&, std::string&);
    bool CreateObject(OpenBlood2::SpriteStore&, double, std::string&);
    bool Update(double);
    OpenBlood2::SmokeImpactSettings settings;
    std::shared_ptr<const OpenBlood2::Sprite> sprite;
    OpenBlood2::SpriteTrack track;
    DVector position{}, velocity{}, scale{}, color{1, 1, 1};
    float alpha = 1, delay = 0;
    double start_delay = 0, start_time = 0, last_time = 0, next_update = 0;
    bool first_update = true, scale_changes = false;
    DDWORD flags = FLAG_VISIBLE | FLAG_SPRITECHROMAKEY;
    std::uint64_t id = 0, event = 0, updates = 0;
    std::string error;

  private:
    bool initialized_ = false;
};
