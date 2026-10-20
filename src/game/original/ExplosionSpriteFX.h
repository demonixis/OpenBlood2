#pragma once
#include "engine/model.h"
#include "engine/sprite.h"
#include <optional>
namespace OpenBlood2
{
    struct ExplosionSpriteSettings
    {
        DVector position{}, normal{0, 1, 0}, scale1{}, scale2{1, 1, 1};
        float duration = 1, alpha = 1;
        DBYTE waveform = 0, fade = 0, align = 0;
        // Null uses source default; an authored empty string is not null.
        std::optional<std::string> resource;
    };
} // namespace OpenBlood2
// Selected original ExplosionFX.cpp sprite Init/CreateObject/Update.
// Typed owned state replaces the separate legacy client object and string handles.
class CExplosionSpriteFX
{
  public:
    bool Init(const OpenBlood2::ExplosionSpriteSettings&, std::string& error);
    bool CreateObject(OpenBlood2::SpriteStore&, double now, std::string& error, float alignment_roll = 0);
    bool Update(double now);
    OpenBlood2::ExplosionSpriteSettings settings;
    std::shared_ptr<const OpenBlood2::Sprite> sprite;
    OpenBlood2::SpriteTrack track;
    DRotation rotation;
    DVector scale{};
    float alpha = 1;
    double start_time = 0, last_time = 0, next_update = 0;
    DDWORD flags = FLAG_VISIBLE | FLAG_SPRITECHROMAKEY;
    std::uint64_t id = 0, event = 0, updates = 0;
    std::string error;

  private:
    bool scale_changes_ = false, initialized_ = false;
};
