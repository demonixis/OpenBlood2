// Selected ClientShellDLL/LightFX.cpp wave tables/evaluation and update rules.
#include "LightFX.h"
#include "engine/collision.h"
#include <algorithm>
#include <cmath>
using namespace OpenBlood2;
namespace
{
    constexpr float PI = 3.14159265358979323846f, PIx2 = 2 * PI;
    static constexpr char flicker1[] = {
        0, 0, 1, 0, 1, 1, 1, 1, 0, 0, 0, 0, 1, 0, 1, 0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 1, 1, 0, 1, 0, 0, 1,
        0, 0, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 1, 0, 0, 0, 0, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1,
    };

    // organic flicker -- good for torches
    static constexpr DFLOAT flicker2[] = {
        1, 2, 4, 2, 3, 4, 3, 2, 0, 0, 1, 2, 4, 3, 2, 0, 2, 1, 0, 1, 0, 2, 3, 4, 3, 2, 1, 1, 2, 0, 0, 1,
        1, 2, 3, 4, 4, 3, 2, 1, 2, 3, 4, 4, 2, 1, 0, 1, 0, 0, 0, 0, 1, 2, 3, 4, 3, 2, 1, 2, 3, 4, 3, 2,
    };

    // mostly on flicker -- good for flaky fluourescents
    static constexpr DFLOAT flicker3[] = {
        4, 4, 4, 4, 3, 4, 4, 4, 4, 4, 4, 2, 4, 3, 4, 4, 4, 4, 2, 1, 3, 3, 3, 4, 3, 4, 4, 4, 4, 4, 2, 4,
        4, 4, 3, 4, 4, 4, 4, 4, 4, 4, 4, 4, 2, 1, 0, 1, 0, 1, 0, 1, 0, 2, 3, 4, 4, 4, 4, 4, 4, 4, 3, 4,
    };

    // mostly off flicker -- good for really flaky fluourescents
    static constexpr DFLOAT flicker4[] = {
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 4, 0, 0, 3, 0, 1, 0, 1, 0, 4, 4, 4, 4, 4, 2, 0,
        0, 0, 0, 4, 4, 3, 2, 1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 2, 1, 2, 1, 2, 1, 2, 1, 4, 3, 2,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    };

    static constexpr DFLOAT strobe[] = {
        64, 64, 64, 48, 36, 27, 20, 15, 11, 9, 6, 5, 4, 3, 2, 2, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0,  0,  0,  0,  0,  0,  0,  0,  0,  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    };

    // static int GetWaveValue( int nWave, int theta, int amplitude )
    static DFLOAT GetWaveValue(DBYTE nWaveform, DFLOAT fMin, DFLOAT fMax, DFLOAT fTheta)
    {
        DFLOAT fReturn = fMax;

        fTheta = (DFLOAT)fmod(fTheta, PIx2);
        // Original negative table indexes are undefined. Wrap negative phases safely.
        if (fTheta < 0)
            fTheta += PIx2;
        if (fTheta >= PIx2)
            fTheta = 0;

        switch (nWaveform)
        {
            case WAVE_SQUARE:
                fReturn = (fTheta <= PI) ? fMin : fMax;
                break;

            case WAVE_SAW:
            {
                if (fTheta < PI)
                    fReturn = fMin + (fMax - fMin) * fTheta / PI;
                else
                    fReturn = fMin + (fMax - fMin) * (PIx2 - fTheta) / PI;
                break;
            }

            case WAVE_RAMPUP:
                fReturn = fMin + (fMax - fMin) * fTheta / (PIx2);
                break;

            case WAVE_RAMPDOWN:
                fReturn = fMin + (fMax - fMin) * (PIx2 - fTheta) / (PIx2);
                break;

            case WAVE_SINE:
                fReturn = fMin + (fMax - fMin) * ((DFLOAT)sin(fTheta) / 2.0f + 0.5f);
                break;

            case WAVE_FLICKER1:
            {
                int index = (int)((fTheta / (PIx2)) * 63);
                fReturn = fMin + (fMax - fMin) * (flicker1[index]);
                break;
            }

            case WAVE_FLICKER2:
            {
                int index = (int)((fTheta / (PIx2)) * 63);
                fReturn = fMin + (fMax - fMin) * (flicker2[index] / 4.0f);
                break;
            }

            case WAVE_FLICKER3:
            {
                int index = (int)((fTheta / (PIx2)) * 63);
                fReturn = fMin + (fMax - fMin) * (flicker3[index] / 4.0f);
                break;
            }

            case WAVE_FLICKER4:
            {
                int index = (int)((fTheta / (PIx2)) * 127);
                fReturn = fMin + (fMax - fMin) * (flicker4[index] / 4.0f);
                break;
            }

            case WAVE_STROBE:
            {
                int index = (int)((fTheta / (PIx2)) * 63);
                fReturn = fMin + (fMax - fMin) * (strobe[index] / 64.0f);
                break;
            }

            case WAVE_SEARCH:
            {
                fTheta *= 2.0f;
                if (fTheta > PIx2)
                    fReturn = fMin;
                else
                    fReturn = fMin + (fMax - fMin) * ((DFLOAT)-cos(fTheta) / 2.0f + 0.5f);
                break;
            }
        }

        return fReturn;
    };

    float radius_value(float value)
    {
        return std::clamp(value, 0.0f, 10000.0f);
    }
    DVector color_value(DVector color, float intensity)
    {
        return {std::clamp(.5f + color.x * intensity, 0.0f, 1.0f), std::clamp(.5f + color.y * intensity, 0.0f, 1.0f),
                std::clamp(.5f + color.z * intensity, 0.0f, 1.0f)};
    }
} // namespace
namespace OpenBlood2
{
    bool valid_light_settings(const LightSettings& s)
    {
        auto wave = [](const LightWave& w)
        {
            return w.kind <= WAVE_SEARCH && std::isfinite(w.min) && std::abs(w.min) <= 1e6f && std::isfinite(w.max) &&
                   std::abs(w.max) <= 1e6f && std::isfinite(w.frequency) && std::abs(w.frequency) <= 1e6f &&
                   std::isfinite(w.phase) && std::abs(w.phase) <= 1e6f;
        };
        return physics_vector(s.color) && wave(s.intensity) && wave(s.radius) && !(s.flags & ~0xf8u) &&
               s.ramp_up_sound.size() <= 100 && s.ramp_down_sound.size() <= 100 &&
               s.ramp_up_sound.find('\0') == std::string::npos && s.ramp_down_sound.find('\0') == std::string::npos;
    }
    bool light_wave_value(DBYTE wave, float min, float max, float theta, float& output)
    {
        if (wave > WAVE_SEARCH || !std::isfinite(min) || !std::isfinite(max) || !std::isfinite(theta) ||
            std::abs(min) > 1e6f || std::abs(max) > 1e6f)
            return false;
        output = GetWaveValue(wave, min, max, theta);
        return std::isfinite(output);
    }
} // namespace OpenBlood2
bool CLightFX::Init(HOBJECT object, const LightSettings& value, float time, DVector pos)
{
    if (!object || !valid_light_settings(value) || !std::isfinite(time) || !physics_vector(pos))
        return false;
    source = object;
    settings = value;
    position = pos;
    updates = 0;
    visible = true;
    off_start_time = 0;
    float intensity = 0, r = 0;
    if (!light_wave_value(settings.intensity.kind, settings.intensity.min, settings.intensity.max,
                          time * settings.intensity.frequency + settings.intensity.phase, intensity) ||
        !light_wave_value(settings.radius.kind, settings.radius.min, settings.radius.max,
                          time * settings.radius.frequency + settings.radius.phase, r))
        return false;
    radius = radius_value(r);
    color = color_value(settings.color, intensity);
    return true;
}
bool CLightFX::Update(float time, DVector pos, bool on)
{
    if (!std::isfinite(time) || !physics_vector(pos))
        return false;
    auto next_color = color;
    float next_radius = radius, value = 0;
    if (on)
    {
        if (settings.intensity.kind != WAVE_NONE)
        {
            if (!light_wave_value(settings.intensity.kind, settings.intensity.min, settings.intensity.max,
                                  time * settings.intensity.frequency + settings.intensity.phase, value))
                return false;
            next_color = color_value(settings.color, value);
        }
        if (settings.radius.kind != WAVE_NONE)
        {
            if (!light_wave_value(settings.radius.kind, settings.radius.min, settings.radius.max,
                                  time * settings.radius.frequency + settings.radius.phase, value))
                return false;
            next_radius = radius_value(value);
        }
    }
    else
    {
        off_start_time = time;
        next_radius = radius_value(settings.radius.min);
    }
    position = pos;
    visible = on;
    color = next_color;
    radius = next_radius;
    ++updates;
    return true;
}
