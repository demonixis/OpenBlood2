// LaserBeamFX.cpp Init/CreateObject/Update/SetupBeam/UpdateBeam: red-small branch.
#include "LaserBeamFX.h"
#include "engine/collision.h"
#include "engine/objects.h"
#include <cmath>
using namespace OpenBlood2;
bool CLaserBeamFX::CreateObject(ModelStore& models, const Resources& resources, DVector from, DVector to, double now,
                                std::string& failure)
{
    failure.clear();
    if (model || !physics_vector(from) || !physics_vector(to) || !std::isfinite(now) || now < 0 ||
        std::sqrt(vector_dot(vector_subtract(to, from), vector_subtract(to, from))) > 1e7f ||
        !align_rotation(vector_subtract(to, from), rotation))
    {
        failure = "Invalid/zero-length laser endpoints or creation state";
        return false;
    }
    if (!models.load("Models\\Ammo\\beam.abc", model, failure))
        return false;
    if (!resources.find(skin_path))
    {
        model.reset();
        failure = "Laser skin is not indexed: " + skin_path;
        return false;
    }
    source = from;
    destination = to;
    last_time = now;
    return true;
}
bool CLaserBeamFX::Update(double now)
{
    error.clear();
    if (!model || !std::isfinite(now) || now < last_time)
    {
        error = "Invalid laser clock/state";
        return false;
    }
    std::vector<std::uint32_t> keys;
    if (!advance_animation(*model, animation, now - last_time, keys, error))
        return false;
    last_time = now;
    if (!updates)
    {
        start_time = now; // Source starts the lifetime on first Update, not publication.
        position = source;
        const auto delta = vector_subtract(destination, source);
        scale = {.05f, .05f, std::sqrt(vector_dot(delta, delta))};
    }
    ++updates;
    const auto time = now - start_time;
    const float width = time <= .25 ? .05f + .95f * static_cast<float>(time / .25) : 1.f;
    scale.x = scale.y = width;
    alpha = time >= .25 ? .5f - .5f * static_cast<float>((time - .25) / .25) : .5f;
    return time < .5; // Expired state is removed before presentation; never render negative alpha.
}
