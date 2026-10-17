#pragma once
#include "engine/model.h"
#include "engine/sprite.h"
#include <optional>
#include <random>
namespace OpenBlood2
{
    struct BloodSplatSettings
    {
        DVector position{};
        DRotation rotation{};
        float scale = .1f, grow_scale = .02f;
        std::optional<std::string> resource;
    };
} // namespace OpenBlood2
// Selected original ClientShellDLL/BloodSplatFX.cpp, owned client state.
class CBloodSplatFX
{
  public:
    bool Init(const OpenBlood2::BloodSplatSettings&, std::string&);
    bool CreateObject(OpenBlood2::SpriteStore&, double, std::minstd_rand&, std::string&);
    bool Update(double, std::minstd_rand&);
    bool Term();
    OpenBlood2::BloodSplatSettings settings;
    std::shared_ptr<const OpenBlood2::Sprite> sprite;
    OpenBlood2::SpriteTrack track;
    DVector position{}, scale{};
    DRotation rotation{};
    float alpha = 1, grow_time = 0;
    bool shrinking = false;
    double last_time = 0, next_update = 0;
    std::uint64_t id = 0, event = 0, updates = 0;
    std::string resource, error;

  private:
    bool initialized_ = false;
};
