#pragma once
#include "engine/model.h"
#include "engine/sprite.h"
#include "game/impact_settings.h"
#include <optional>
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
