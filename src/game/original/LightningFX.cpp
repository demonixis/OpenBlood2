#include "LightningFX.h"
#include "engine/collision.h"
#include "engine/objects.h"
#include <algorithm>
#include <cmath>
#include <numbers>
using namespace OpenBlood2;
bool CLightningBoltFX::CreateObject(const LightningSettings& value, const Resources& resources, double now,
                                    std::minstd_rand& rng, std::string& failure)
{
    failure.clear();
    DRotation rotation;
    const auto delta = vector_subtract(value.destination, value.source);
    const float distance = std::sqrt(vector_dot(delta, delta));
    if (!physics_vector(value.source) || !physics_vector(value.destination) || value.type > 2 || !std::isfinite(now) ||
        now < 0 || distance > 1e7f || !align_rotation(delta, rotation))
    {
        failure = "Invalid lightning endpoints, type or clock";
        return false;
    }
    if (!resources.find(texture))
    {
        failure = "Lightning texture is not indexed: " + texture;
        return false;
    }
    settings = value;
    start_time = last_time = now;
    segments.clear();
    lights.clear();
    auto random = [&](float lo, float hi)
    { return lo + (hi - lo) * float(rng() - rng.min()) / float(rng.max() - rng.min()); };
    const unsigned count = std::clamp(unsigned(distance / (value.type == 0   ? 500
                                                           : value.type == 1 ? 250
                                                                             : 100)),
                                      5u, 25u);
    const float length = distance / float(count), max_radius = 3000.f * float(count / 5); // Original integer quotient.
    const float min_offset = distance / 100, max_offset = distance / 10;
    const float radius_step = (max_radius - 300) / float(count), offset_step = (max_offset - min_offset) / float(count);
    if (!settings.shape)
        settings.shape = DBYTE(std::min(4u, 1u + unsigned(random(0, 4))));
    // Original typo: random FORM changes TYPE after segment count is already calculated.
    if (!settings.form)
        settings.type = DBYTE(std::min(4u, 1u + unsigned(random(0, 4))));
    DVector previous = value.source;
    std::size_t particles = 0;
    for (unsigned i = 0; i < count; ++i)
    {
        LightningSegment segment;
        float j = float(i);
        if (settings.form == 3 || settings.form == 4)
            j = float(i < (count >> 1) ? (i << 1) : ((count - 1 - i) << 1));
        const bool narrow = settings.form == 2 || settings.form == 3;
        segment.radius = narrow ? 300 + j * radius_step : max_radius - j * radius_step;
        const float offset = narrow ? min_offset + j * offset_step : max_offset - j * offset_step;
        const float channel = narrow ? 100 + j * (155 / float(count)) : 255 - j * (155 / float(count));
        segment.color = {channel / 255, channel / 255, 1};
        if (settings.shape == 3 || settings.shape == 4)
        {
            const float angle = settings.shape == 3 ? float(i) * (4 * std::numbers::pi_v<float> / float(count - 1))
                                                    : float(i % 5) * (4 * std::numbers::pi_v<float> / 5);
            rotation = multiply_rotation(rotation, rotation_from_euler({0, 0, angle})); // Cumulative, not absolute.
        }
        const auto forward = rotate_vector({0, 0, 1}, rotation), right = rotate_vector({1, 0, 0}, rotation),
                   up = rotate_vector({0, 1, 0}, rotation);
        float x = offset, y = 0;
        if (settings.shape == 1)
            x = random(-offset, offset);
        else if (settings.shape != 3 && settings.shape != 4)
        {
            y = random(-offset, offset); // Source draws up before right.
            x = random(-offset, offset);
        }
        segment.from = previous;
        segment.to = vector_add(value.source, vector_add(vector_scale(forward, length * float(i + 1)),
                                                         vector_add(vector_scale(right, x), vector_scale(up, y))));
        if (i == count - 1)
            segment.to = value.destination;
        const auto direction = vector_subtract(segment.to, segment.from);
        const float magnitude = std::sqrt(vector_dot(direction, direction));
        const auto amount = unsigned(magnitude / (2.5f * segment.radius / 1000));
        if (amount > 16384 - particles)
        {
            failure = "Lightning particle budget exceeded";
            segments.clear();
            return false;
        }
        particles += amount;
        // Original divides by zero when count is zero, but emits no particle. Keep the empty segment finite.
        segment.increment = amount ? magnitude / float(amount) : 0;
        for (unsigned p = 0; p < amount; ++p)
        {
            Particle particle;
            // Source local tempPos is uninitialized; deterministic zero is the explicit engine adaptation.
            particle.position = vector_add(segment.from, vector_scale(direction, float(p) / float(amount)));
            particle.color = segment.color;
            particle.lifetime = .25f;
            segment.particles.push_back(particle);
        }
        previous = segment.to;
        segments.push_back(std::move(segment));
    }
    for (unsigned i = 0; i < 3; ++i)
    {
        ExplosionLightSettings light;
        light.position = vector_add(value.source, vector_scale(delta, float(i) / 2));
        light.color1 = light.color2 = {1, 1, 1};
        light.duration = .5f;
        light.radius1 = 200;
        light.radius2 = 100;
        CExplosionLightFX fx;
        if (!fx.Init(light, failure) || !fx.CreateObject(now, failure))
            return false;
        lights.push_back(std::move(fx));
    }
    return true;
}
bool CLightningBoltFX::Update(double now)
{
    if (!std::isfinite(now) || now < last_time)
    {
        error = "Invalid lightning clock";
        return false;
    }
    last_time = now;
    const auto elapsed = now - start_time;
    alpha = elapsed >= .1 ? std::max(0.f, .75f * (1 - float((elapsed - .1) / .15))) : .75f;
    if (elapsed >= .25)
        segments.clear();
    bool valid = true;
    std::erase_if(lights,
                  [&](auto& light)
                  {
                      const bool alive = light.Update(now);
                      if (!light.error.empty())
                      {
                          error = light.error;
                          valid = false;
                      }
                      return !alive;
                  });
    return valid && (!segments.empty() || !lights.empty());
}
