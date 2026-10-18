// LaserBeamFX.cpp Init/CreateObject/Update/SetupBeam/UpdateBeam: red-small branch.
#include "LaserBeamFX.h"
#include "engine/collision.h"
#include "engine/objects.h"
#include <cmath>
using namespace OpenBlood2;
void CLaserBeamFX::SetupBeam(DBYTE value)
{
    type = value;
    const bool large = type <= 13 && type % 2;
    min_scale = large ? .1f : .05f; // The default branch differs only in min scale 0.
    if (type > 13)
        min_scale = 0;
    max_scale = large ? 2.5f : 1;
    duration = large ? 1 : .5f;
    scale_time = fade_time = large ? .35f : .25f;
    full_alpha = alpha = .5f;
    constexpr const char* colors[]{"red", "green", "blue", "white", "yellow", "orange", "purple"};
    skin_path = std::string("Skins\\Ammo\\beam") + (type <= 13 ? colors[type / 2] : "red") + ".dtx";
}
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
        scale = {min_scale, min_scale, std::sqrt(vector_dot(delta, delta))};
    }
    ++updates;
    const auto time = now - start_time;
    const float width =
        time <= scale_time ? min_scale + (max_scale - min_scale) * static_cast<float>(time / scale_time) : max_scale;
    scale.x = scale.y = width;
    alpha = time >= duration - fade_time
                ? full_alpha - full_alpha * static_cast<float>((time - (duration - fade_time)) / fade_time)
                : full_alpha;
    return time < duration; // Expired state is removed before presentation; never render negative alpha.
}
