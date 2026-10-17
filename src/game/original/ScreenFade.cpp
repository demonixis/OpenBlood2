#include "ScreenFade.h"
#include <cmath>

void CScreenFade::Begin(bool in)
{
    fade_in = in;
    fade_out = !in;
    value = in ? 0.f : 255.f;
    ++messages;
}
bool CScreenFade::Update(double seconds)
{
    if (!std::isfinite(seconds) || seconds < 0 || seconds > 60)
        return false;
    if (fade_in)
    {
        value += 100.f * static_cast<float>(seconds);
        if (value > 255.f)
        {
            value = 255.f;
            fade_in = false;
        }
    }
    else if (fade_out)
    {
        value -= 100.f * static_cast<float>(seconds);
        if (value < 0)
            value = 0;
        // Source deliberately retains fade_out: black holds until a new message/world.
    }
    return true;
}
float CScreenFade::Scale() const
{
    return fade_in || fade_out ? value / 255.f : 1.f;
}
