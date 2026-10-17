#pragma once
#include <cstdint>

// Selected BloodClientShell SMSG_SCREENFADE/update rules; see screen_fade_contracts.md.
class CScreenFade
{
  public:
    void Begin(bool in);
    bool Update(double seconds);
    float Scale() const;
    bool fade_in = false, fade_out = false;
    float value = 0;
    std::uint64_t messages = 0;
};
