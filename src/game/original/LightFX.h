#pragma once
#include "game/light_messages.h"
// Selected original client CLightFX: real evaluated state, no generic SFX/packet ABI.
class CLightFX
{
  public:
    bool Init(HOBJECT, const OpenBlood2::LightSettings&, float time, DVector position);
    bool Update(float time, DVector position, bool on);
    HOBJECT source = nullptr;
    OpenBlood2::LightSettings settings;
    DVector position{}, color{};
    float radius = 0, off_start_time = 0;
    bool visible = true;
    std::uint64_t updates = 0;
};
namespace OpenBlood2
{
    // Checked source waveform, including original sample tables and phase thresholds.
    bool light_wave_value(DBYTE wave, float min, float max, float theta, float& output);
} // namespace OpenBlood2
