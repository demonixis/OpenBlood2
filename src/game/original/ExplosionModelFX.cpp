#include "ExplosionModelFX.h"
#include "engine/collision.h"
#include "engine/objects.h"
#include <cmath>
#include <numbers>
using namespace OpenBlood2;
namespace
{
    float random_float(std::minstd_rand& random, float low, float high)
    {
        // Original ClientUtilities formula and one draw, on a private portable client stream.
        const float fraction = static_cast<float>(random() - std::minstd_rand::min()) /
                               static_cast<float>(std::minstd_rand::max() - std::minstd_rand::min());
        return low + (high - low) * fraction;
    }
    void around(DRotation& rotation, DVector axis, float angle)
    {
        const float sine = std::sin(angle / 2);
        DRotation turn;
        turn.m_Vec = {axis.x * sine, axis.y * sine, axis.z * sine};
        turn.m_Spin = std::cos(angle / 2);
        rotation = compose_rotation(turn, rotation);
    }
} // namespace
bool CExplosionModelFX::Init(const ExplosionModelSettings& input, std::string& failure)
{
    failure.clear();
    *this = {};
    auto valid_scale = [](DVector v) { return physics_vector(v) && v.x >= 0 && v.y >= 0 && v.z >= 0; };
    auto valid_path = [](const std::optional<std::string>& v)
    { return !v || (v->size() <= 1000 && v->find('\0') == std::string::npos); };
    if (!physics_vector(input.position) || !physics_vector(input.normal) || !physics_vector(input.rotations) ||
        !valid_scale(input.scale1) || !valid_scale(input.scale2) || !std::isfinite(input.duration) ||
        input.duration <= 0 || input.duration > 600 || !std::isfinite(input.alpha) || input.alpha < 0 ||
        input.alpha > 1 || input.fade > 5 || input.random_rotation > 2 || !valid_path(input.model) ||
        !valid_path(input.skin))
    {
        failure = "Invalid explosion model descriptor";
        return false;
    }
    settings = input;
    scale_changes_ =
        input.scale1.x != input.scale2.x || input.scale1.y != input.scale2.y || input.scale1.z != input.scale2.z;
    initialized_ = true;
    return true;
}
bool CExplosionModelFX::CreateObject(ModelStore& models, const Resources& resources, double now,
                                     std::minstd_rand& random, std::string& failure)
{
    failure.clear();
    if (!initialized_ || model || !std::isfinite(now) || now < 0)
    {
        failure = "Invalid explosion model creation state/time";
        return false;
    }
    constexpr auto pi = std::numbers::pi_v<float>;
    if (settings.random_rotation == 1)
    {
        const float pitch = random_float(random, -pi, pi);
        const float yaw = random_float(random, -pi, pi);
        const float roll = random_float(random, -pi, pi);
        rotation = rotation_from_euler({pitch, yaw, roll});
    }
    else if (settings.random_rotation == 2)
    {
        if (!align_rotation(settings.normal, rotation))
        {
            failure = "Cannot align explosion model to a zero/invalid normal";
            return false;
        }
        around(rotation, rotate_vector(rotation, {0, 0, 1}), random_float(random, 0, pi));
    }
    model_path = settings.model.value_or("Models\\Explosions\\exp_sphere.abc");
    skin_path = settings.skin.value_or("Skins\\Explosions\\Explosion_1.dtx");
    if (!models.load(model_path, model, failure))
        return false;
    if (!resources.find(skin_path))
    {
        model.reset();
        failure = "Explosion model skin is not indexed: " + skin_path;
        return false;
    }
    scale = settings.scale1;
    alpha = settings.alpha;
    animation = {};
    start_time = last_time = now;
    return true;
}
bool CExplosionModelFX::Update(double now)
{
    error.clear();
    if (!model || !std::isfinite(now) || now < last_time)
    {
        error = "Invalid explosion model clock/state";
        return false;
    }
    const float time = static_cast<float>(now - start_time), ratio = time / settings.duration;
    std::vector<std::uint32_t> keys;
    if (!std::isfinite(ratio) || !advance_animation(*model, animation, now - last_time, keys, error))
    {
        if (error.empty())
            error = "Explosion model animation clock failed";
        return false;
    }
    last_time = now;
    if (now < next_update)
        return true;
    ++updates;
    if (scale_changes_)
    {
        float amount = ratio;
        const float triangle = time < settings.duration / 2 ? ratio * 2 : (1 - ratio) * 2;
        switch (settings.waveform)
        {
            case 1:
                amount = std::sqrt(ratio);
                break;
            case 2:
                amount = ratio * ratio;
                break;
            case 3:
                amount = triangle;
                break;
            case 4:
                amount = std::sqrt(triangle);
                break;
            case 5:
                amount = triangle * triangle;
                break;
        }
        scale = {settings.scale1.x + (settings.scale2.x - settings.scale1.x) * amount,
                 settings.scale1.y + (settings.scale2.y - settings.scale1.y) * amount,
                 settings.scale1.z + (settings.scale2.z - settings.scale1.z) * amount};
    }
    if (settings.rotations.x || settings.rotations.y || settings.rotations.z)
    {
        // Source caches all three axes before any rotation and does NOT multiply by frame time.
        const auto up = rotate_vector(rotation, {0, 1, 0}), right = rotate_vector(rotation, {1, 0, 0}),
                   forward = rotate_vector(rotation, {0, 0, 1});
        around(rotation, up, settings.rotations.x);
        around(rotation, right, settings.rotations.y);
        around(rotation, forward, settings.rotations.z);
    }
    switch (settings.fade)
    {
        case 1:
            alpha = settings.alpha * (1 - ratio);
            break;
        case 2:
            alpha = 1 - (1 - settings.alpha) * ratio;
            break;
        case 3:
            alpha = settings.alpha * ratio;
            break;
        case 4:
            alpha = settings.alpha + (1 - settings.alpha) * ratio;
            break;
        case 5:
            alpha = time < settings.duration / 2 ? settings.alpha * ratio * 2 : settings.alpha * 2 * (1 - ratio);
            break;
    }
    if (time < settings.duration)
    {
        next_update = now + .001;
        return true;
    }
    return false;
}
