#include "PolyGridFX.h"
#include "engine/collision.h"
#include <algorithm>
#include <cmath>
#include <numbers>
using namespace OpenBlood2;
namespace
{
    bool color(DVector v)
    {
        return physics_vector(v) && v.x >= 0 && v.y >= 0 && v.z >= 0 && v.x <= 255 && v.y <= 255 && v.z <= 255;
    }
    DVector packed(DVector v)
    {
        return {float(unsigned(v.x) & 248), float(unsigned(v.y) & 252), float(unsigned(v.z) & 248)};
    }
    const auto& sine()
    {
        static const auto table = []
        {
            std::array<std::int8_t, 256> t{};
            const float s = (2 * std::numbers::pi_v<float> / 255) * 3;
            for (unsigned i = 0; i < 256; ++i)
            {
                const int n = int(std::sin(double(float(i) * s)) * 128);
                t[i] = std::int8_t(n > 127 ? n - 256 : n);
            }
            return t;
        }();
        return table;
    }
} // namespace
bool OpenBlood2::valid_polygrid(const PolyGridSettings& s)
{
    if (!physics_vector(s.dims) || s.dims.x < 0 || s.dims.z < 0 || s.dims.y < 0 || !color(s.color1) ||
        !color(s.color2) || !std::isfinite(s.alpha) || s.alpha < 0 || s.alpha > 1 || s.polies < 4 || s.plasma > 1 ||
        s.sprite.size() > 499 || s.sprite.find('\0') != std::string::npos)
        return false;
    for (unsigned i = 0; i < 2; ++i)
        if (!std::isfinite(s.scale_min[i]) || !std::isfinite(s.scale_max[i]) || !std::isfinite(s.duration[i]) ||
            !std::isfinite(s.pan[i]) || std::abs(s.pan[i]) > 1000000 || s.scale_min[i] <= 0 ||
            s.scale_max[i] < s.scale_min[i] || s.scale_max[i] > 1000000 || std::abs(s.duration[i]) > 1000000)
            return false;
    for (auto rate : s.rates)
        if (rate > 255)
            return false;
    return true;
}
bool CPolyGridFX::Init(const PolyGridSettings& s, SpriteStore& store, double now, std::string& failure)
{
    failure.clear();
    if (!valid_polygrid(s) || !std::isfinite(now) || now < 0)
    {
        failure = "Invalid PolyGrid settings/clock";
        return false;
    }
    settings = s;
    settings.polies = std::min(s.polies, 32767u);
    settings.color1 = packed(s.color1);
    settings.color2 = packed(s.color2);
    texture_error.clear();
    if (!s.sprite.empty() && !store.load(s.sprite, sprite, failure))
    {
        // A zero-area authored grid cannot draw. Keep its failed texture lookup
        // inspectable and its wave state alive; no substitute image is needed.
        if (s.dims.x != 0 && s.dims.z != 0)
            return false;
        texture_error = failure;
        failure.clear();
    }
    width = settings.polies <= 4 ? 2 : unsigned(std::sqrt(float(settings.polies / 2))) + 1;
    if (settings.plasma == 1 && settings.polies > 4)
        width = width < 4 ? 2 : width <= 6 ? 4 : width <= 12 ? 8 : 16;
    heights.assign(width * width, 0);
    distance.resize(width * width);
    int half = int(width / 2);
    for (unsigned y = 0; y < width; ++y)
        for (unsigned x = 0; x < width; ++x)
        {
            int dx = int(x) - half, dy = int(y) - half;
            distance[y * width + x] = std::uint8_t(float(dx * dx + dy * dy) / float(2 * half * half) * 255);
        }
    for (unsigned i = 0; i < 256; ++i)
        palette[i] =
            vector_scale(vector_add(settings.color1,
                                    vector_scale(vector_subtract(settings.color2, settings.color1), float(i) / 256)),
                         1.f / 255);
    scale = s.sprite.empty() ? std::array<float, 2>{1, 1} : s.scale_min;
    last_time = now;
    return true;
}
bool CPolyGridFX::Update(double now, bool shown, std::array<float, 4>& shared)
{
    if (!std::isfinite(now) || now < last_time || now - last_time > 10)
    {
        error = "Invalid PolyGrid update clock";
        return false;
    }
    const float dt = float(now - last_time);
    last_time = now;
    visible = shown;
    if (!shown)
        return true;
    // Explicit always-update/high-detail policy: rendering/UI never drives simulation.
    const auto& table = sine();
    if (settings.plasma == 0)
    {
        count = std::fmod(count + 50 * dt, 256.f);
        for (unsigned i = 0; i < heights.size(); ++i)
            heights[i] = table[std::uint8_t(distance[i] + unsigned(count))];
    }
    else
    {
        constexpr unsigned offsets[4][2] = {{6, 10}, {13, 3}, {17, 1}, {31, 22}};
        for (unsigned i = 0; i < 4; ++i)
            shared[i] = std::fmod(shared[i] + float(settings.rates[i]) * dt, 256.f);
        for (unsigned y = 0; y < width; ++y)
            for (unsigned dest = 0; dest < width; ++dest)
            {
                unsigned x = width - 1 - dest;
                int sum = 0;
                for (unsigned i = 0; i < 4; ++i)
                {
                    auto d =
                        distance[((y + offsets[i][1]) & (width - 1)) * width + ((x + offsets[i][0]) & (width - 1))];
                    sum += table[std::uint8_t(d + unsigned(shared[i]))];
                }
                heights[y * width + dest] = std::int8_t(sum >= 0 ? sum / 4 : -((-sum + 3) / 4));
            }
    }
    if (sprite)
    {
        if (!advance_sprite(*sprite, track, dt))
        {
            error = "PolyGrid sprite clock failed";
            return false;
        }
        for (unsigned i = 0; i < 2; ++i)
            if (settings.duration[i] > 0)
            {
                const float delta = dt * (settings.scale_max[i] - settings.scale_min[i]) / settings.duration[i];
                scale[i] += scaling_up[i] ? delta : -delta;
                if (scaling_up[i] && scale[i] > settings.scale_max[i])
                {
                    scale[i] = settings.scale_max[i];
                    scaling_up[i] = false;
                }
                else if (!scaling_up[i] && scale[i] < settings.scale_min[i])
                {
                    scale[i] = settings.scale_min[i];
                    scaling_up[i] = true;
                }
            }
    }
    ++updates;
    return true;
}
