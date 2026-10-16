#include "ExplosionFragFX.h"
#include "engine/collision.h"
#include "game/legacy/server_de.h"
#include <cmath>
#include <numbers>
using namespace OpenBlood2;
namespace
{
    float random_float(std::minstd_rand& random, float lo, float hi)
    {
        return lo + (hi - lo) * float(random() - std::minstd_rand::min()) /
                        float(std::minstd_rand::max() - std::minstd_rand::min());
    }
    bool normalize(DVector& v)
    {
        const float length = std::sqrt(vector_dot(v, v));
        if (!std::isfinite(length) || length < 1e-6f)
            return false;
        v = vector_scale(v, 1 / length);
        return true;
    }
} // namespace
bool CExplosionFragFX::Init(const ExplosionFragSettings& s, std::string& failure)
{
    failure.clear();
    *this = {};
    auto bound = [](float v, float lo, float hi) { return std::isfinite(v) && v >= lo && v <= hi; };
    auto positive = [](DVector v) { return physics_vector(v) && v.x >= 0 && v.y >= 0 && v.z >= 0; };
    auto path = [](const std::optional<std::string>& v)
    { return !v || (v->size() <= 1000 && v->find('\0') == std::string::npos); };
    if (!physics_vector(s.position) || !physics_vector(s.normal) ||
        (!s.random_direction && vector_dot(s.normal, s.normal) < 1e-12f) || !positive(s.scale) ||
        !positive(s.rotate_max) || !bound(s.spread, 0, 10000) || !bound(s.duration, .001f, 600) ||
        !bound(s.velocity, 0, 10000) || !bound(s.bounce, 0, 1) || !bound(s.gravity, -10000, 10000) ||
        !bound(s.fade_time, 0, s.duration) || !bound(s.alpha, 0, 1) || s.spawn_explosion || s.spawn_type ||
        s.trail_type > 1 || !path(s.model) || !path(s.skin))
    {
        failure = "Invalid or unsupported explosion fragment descriptor (spawn/trail modes are bounded)";
        return false;
    }
    settings = s;
    position = last_position = s.position;
    rotate_ = s.rotate_max.x || s.rotate_max.y || s.rotate_max.z;
    initialized_ = true;
    return true;
}
bool CExplosionFragFX::CreateObject(ModelStore& models, const Resources& resources, double now,
                                    std::minstd_rand& random, std::string& failure)
{
    failure.clear();
    if (!initialized_ || model || !std::isfinite(now) || now < 0)
    {
        failure = "Invalid explosion fragment creation state/time";
        return false;
    }
    if (settings.random_direction)
    {
        // Explicit x/y/z draws replace unspecified argument evaluation order.
        const float x = random_float(random, -10, 10), y = random_float(random, -10, 10),
                    z = random_float(random, -10, 10);
        velocity = {x, y, z};
    }
    else
    {
        DRotation basis;
        if (!align_rotation(settings.normal, basis))
        {
            failure = "Fragment normal cannot be aligned";
            return false;
        }
        auto axis = settings.normal;
        if (!normalize(axis))
        {
            failure = "Fragment axis cannot be normalized";
            return false;
        }
        const float angle = random_float(random, -std::numbers::pi_v<float>, std::numbers::pi_v<float>);
        DRotation turn;
        turn.m_Vec = vector_scale(axis, std::sin(angle / 2));
        turn.m_Spin = std::cos(angle / 2);
        basis = compose_rotation(turn, basis);
        velocity = vector_add(vector_scale(settings.normal, settings.velocity),
                              vector_scale(rotate_vector(basis, {1, 0, 0}), settings.spread));
    }
    if (!normalize(velocity))
    {
        failure = "Fragment direction has zero length";
        return false;
    }
    velocity = vector_scale(velocity, settings.velocity);
    if (rotate_)
    {
        angular_step.x = random_float(random, -settings.rotate_max.x, settings.rotate_max.x);
        angular_step.y = random_float(random, -settings.rotate_max.y, settings.rotate_max.y);
        angular_step.z = random_float(random, -settings.rotate_max.z, settings.rotate_max.z);
    }
    model_path = settings.model.value_or("Models\\Explosions\\exp_sphere.abc");
    skin_path = settings.skin.value_or("Skins\\Explosions\\Explosion_1.dtx");
    if (!models.load(model_path, model, failure))
        return false;
    if (!resources.find(skin_path))
    {
        model.reset();
        failure = "Fragment skin is not indexed: " + skin_path;
        return false;
    }
    if (settings.trail_type)
        trail_error = "Original OBJFX_SMOKETRAIL_1 particle trail unavailable";
    alpha = settings.alpha;
    start_time = last_time = now;
    return true;
}
bool CExplosionFragFX::Update(double now, ServerDE& server)
{
    error.clear();
    if (!model || !std::isfinite(now) || now < last_time)
    {
        error = "Invalid explosion fragment clock/state";
        return false;
    }
    std::vector<std::uint32_t> keys;
    if (!advance_animation(*model, animation, now - last_time, keys, error))
        return false;
    last_time = now;
    if (now < next_update)
        return true;
    ++updates;
    next_update = now + .001;
    const float time = static_cast<float>(now - start_time);
    if (!std::isfinite(time))
    {
        error = "Fragment elapsed time overflow";
        return false;
    }
    moving = !(std::sqrt(vector_dot(velocity, velocity)) < 1 && last_normal.y > .9f);
    if (rotate_ && moving)
    {
        angles = vector_add(angles, angular_step);
        if (!physics_vector(angles))
        {
            error = "Fragment rotation exceeds bounds";
            return false;
        }
        rotation = rotation_from_euler(angles);
    }
    if (time > settings.duration - settings.fade_time && time < settings.duration)
        alpha = settings.alpha - (time - (settings.duration - settings.fade_time)) / settings.fade_time;
    if (moving)
    {
        auto next_velocity = vector_add(velocity, {0, -settings.gravity, 0});
        auto next_position = vector_add(position, next_velocity);
        const auto ray_end = vector_add(position, vector_scale(next_velocity, 1.1f));
        if (!physics_vector(next_velocity) || !physics_vector(next_position) || !physics_vector(ray_end))
        {
            error = "Fragment motion exceeds bounds";
            return false;
        }
        SegmentHit hit;
        last_query = server.IntersectSegment(position, ray_end, nullptr, hit, false, SegmentObjects::SolidOrRayHit);
        if (last_query == SegmentResult::Invalid || last_query == SegmentResult::Unavailable)
        {
            error = "Fragment collision query invalid/unavailable";
            return false;
        }
        if (last_query == SegmentResult::Hit)
        {
            auto normal = hit.normal, direction = next_velocity;
            const float speed = std::sqrt(vector_dot(next_velocity, next_velocity));
            if (!normalize(normal) || !normalize(direction))
            {
                error = "Invalid fragment reflection normal/velocity";
                return false;
            }
            direction = vector_add(direction, vector_scale(normal, -2 * vector_dot(normal, direction)));
            next_velocity = vector_scale(direction, speed * settings.bounce);
            next_position = vector_add(hit.point, normal);
            if (!physics_vector(next_velocity) || !physics_vector(next_position))
            {
                error = "Fragment reflection exceeds bounds";
                return false;
            }
            last_normal = hit.normal;
            last_hit = hit;
            ++impacts;
            angular_step = vector_scale(angular_step, .5f);
        }
        last_position = position;
        position = next_position;
        velocity = next_velocity;
    }
    return time < settings.duration; // Source still rotates/moves on the terminal update.
}
