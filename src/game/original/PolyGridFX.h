#pragma once
#include "engine/model.h"
#include "engine/sprite.h"
#include <array>
namespace OpenBlood2
{
    struct PolyGridSettings
    {
        DVector dims{32, 32, 0}, color1{50, 50, 200}, color2{0, 0, 220};
        std::array<float, 2> scale_min{15, 15}, scale_max{15, 15}, duration{10, 10}, pan{10, 10};
        float alpha = .7f;
        unsigned polies = 160, plasma = 1;
        std::array<unsigned, 4> rates{50, 10, 30, 20};
        std::string sprite;
    };
    bool valid_polygrid(const PolyGridSettings&);
} // namespace OpenBlood2
// Selected original CPolyGridFX high-detail rules, with owned storage and checked clocks.
class CPolyGridFX
{
  public:
    bool Init(const OpenBlood2::PolyGridSettings&, OpenBlood2::SpriteStore&, double, std::string&);
    bool Update(double, bool, std::array<float, 4>&);
    OpenBlood2::PolyGridSettings settings;
    OpenBlood2::NodePose pose;
    std::shared_ptr<const OpenBlood2::Sprite> sprite;
    OpenBlood2::SpriteTrack track;
    std::array<DVector, 256> palette{};
    std::vector<std::int8_t> heights;
    std::vector<std::uint8_t> distance;
    std::array<float, 2> scale{1, 1};
    std::array<bool, 2> scaling_up{true, true};
    unsigned width = 0;
    float count = 0;
    double last_time = 0;
    bool visible = false;
    std::uint64_t id = 0, updates = 0;
    std::string error;
};
