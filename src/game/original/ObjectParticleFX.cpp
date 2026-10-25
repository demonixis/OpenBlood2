#include "ObjectParticleFX.h"
#include "engine/collision.h"
#include <cmath>
#include <numbers>
using namespace OpenBlood2;
namespace
{
    bool pose_valid(NodePose pose)
    {
        return physics_vector(pose.position) && physics_vector(pose.rotation.m_Vec) &&
               std::isfinite(pose.rotation.m_Spin) &&
               std::abs(vector_dot(pose.rotation.m_Vec, pose.rotation.m_Vec) +
                        pose.rotation.m_Spin * pose.rotation.m_Spin - 1) < .001f;
    }
    void around(DRotation& rotation, DVector axis, float angle)
    {
        const float sine = std::sin(angle / 2) / std::sqrt(vector_dot(axis, axis));
        DRotation turn;
        turn.m_Vec = vector_scale(axis, sine);
        turn.m_Spin = std::cos(angle / 2);
        rotation = compose_rotation(turn, rotation);
    }
    float draw(std::minstd_rand& random, float lo, float hi)
    {
        return lo + (hi - lo) * float(random() - std::minstd_rand::min()) /
                        float(std::minstd_rand::max() - std::minstd_rand::min());
    }
} // namespace
bool CObjectParticleFX::Init(unsigned type, std::uint64_t owner, std::string& failure)
{
    failure.clear();
    *this = {};
    if ((type != 1 && type != 34) || !owner)
    {
        failure = "Unsupported object particle recipe or absent parent";
        return false;
    }
    recipe = type;
    if (type == 34)
    {
        radius = 200;
        gravity = 0;
        position_radius = 3;
        density = 5;
        add_delay = .025f;
        flags = 0x3f;
        requested_alpha = 1;
        min_life = .15f;
        max_life = .20f;
        min_color = {128, 128, 255};
        max_color = {255, 255, 255};
        texture = "SpriteTextures\\Lensflare32_1.dtx";
    }
    parent = owner;
    initialized_ = true;
    return true;
}
bool CObjectParticleFX::Init(unsigned type, std::uint64_t owner, std::minstd_rand& random, std::string& failure)
{
    if (type == 1 || type == 34)
        return Init(type, owner, failure);
    failure.clear();
    *this = {};
    if ((type != 6 && type != 8 && type != 3 && type != 18 && type != 19 && type != 36 && type != 21 && type != 24 &&
         type != 31 && type != 32 && type != 33 && (type < 12 || type > 17)) ||
        !owner)
    {
        failure = "Unsupported object particle recipe or absent parent";
        return false;
    }
    recipe = type;
    parent = owner;
    if (type == 6 || type == 8)
    {
        // Original OBJFX_SPARKS_1/3: filled sphere, MOVINGSOURCE but no MOVINGSTREAM.
        radius = type == 6 ? 350.f : 400.f;
        position_radius = 5;
        gravity = -250;
        ramp_down = .25f;
        add_delay = .01f;
        authored_count = type == 6 ? 4 : 2;
        density = 2.5f;
        requested_alpha = 1;
        min_life = .5f;
        max_life = .75f;
        min_color = {192, 192, 0};
        max_color = {255, 255, 0};
        flags = 0x07;
        texture = "SpriteTextures\\Lensflare32_1.dtx";
        initialized_ = true;
        return true;
    }
    if (type == 3)
    {
        radius = 200;
        position_radius = 6;
        gravity = 0;
        add_delay = .025f;
        authored_count = 1;
        density = 10;
        requested_alpha = .5f;
        min_life = .25f;
        max_life = .75f;
        min_color = {255, 192, 0};
        max_color = {255, 255, 0};
        flags = 0x0f;
        texture = "SpriteTextures\\Firepart64.dtx";
        initialized_ = true;
        return true;
    }
    if (type == 31 || type == 32 || type == 33)
    {
        radius = type == 33 ? 200.f : type == 31 ? 400.f : 1500.f;
        position_radius = 3;
        gravity = type == 33 ? 0.f : type == 31 ? -75.f : 100.f;
        add_delay = type != 32 ? .025f : .01f;
        authored_count = type != 32 ? 3 : 1;
        density = 7.5f;
        requested_alpha = type != 32 ? 1.f : .5f;
        min_life = type != 32 ? .25f : .35f;
        max_life = type != 32 ? .35f : .5f;
        min_color = type == 33 ? DVector{128, 128, 255} : type == 31 ? DVector{0, 32, 0} : DVector{192, 192, 0};
        max_color = type == 33 ? DVector{255, 255, 255} : type == 31 ? DVector{128, 196, 0} : DVector{255, 255, 255};
        flags = 0x0f;
        texture = type != 32 ? "SpriteTextures\\Lensflare32_1.dtx" : "SpriteTextures\\flames1_09.dtx";
        initialized_ = true;
        return true;
    }
    if (type >= 12 && type <= 17)
    {
        // CObjectGeneralFX OBJFX_VOODOO_1..6, original high-detail recipes.
        offset = {type == 12 ? 5.f : 20.f, 0, 0};
        ramp_down = .25f;
        duration = .75f;
        add_delay = .1f;
        radius = 1000;
        position_radius = 20;
        gravity = -150;
        rotations = {0, .025f, 0};
        authored_count = type == 12 ? 12 : 8;
        density = 10;
        requested_alpha = 1;
        min_life = .5f;
        max_life = 1.5f;
        flags = 0;
        constexpr DVector colors[]{{1, 0, 0}, {1, 0, 0}, {0, 0, 1}, {1, 1, 1}, {0, 1, 0}, {1, 0, 1}};
        min_color = vector_scale(colors[type - 12], 192);
        max_color = vector_scale(colors[type - 12], 255);
        texture = "SpriteTextures\\Lensflare_1.dtx";
        initialized_ = true;
        return true;
    }
    if (type == 21 || type == 24)
    {
        // CCorpse requests scale=3, SCALENUMPARTICLES only. Other fields stay unscaled.
        radius = 2500;
        position_radius = 12.5f;
        ramp_down = .25f;
        duration = type == 21 ? 2.f : 0.f;
        add_delay = type == 21 ? .1f : .05f;
        gravity = type == 21 ? 125.f : 275.f;
        density = type == 21 ? 10.f : 7.5f;
        authored_count = type == 21 ? 9 : 15;
        requested_alpha = type == 21 ? .25f : .75f;
        min_life = type == 21 ? .5f : .25f;
        max_life = type == 21 ? 1.5f : .75f;
        min_color = type == 21 ? DVector{192, 192, 192} : DVector{192, 0, 0};
        max_color = {255, 255, 255};
        flags = type == 21 ? 0x0cu : 0x04u;
        texture = type == 21 ? "SpriteTextures\\smoke64_2.dtx" : "SpriteTextures\\Firepart64.dtx";
        initialized_ = true;
        return true;
    }
    const bool shield = type == 19;
    radius = shield || type == 18 ? 500.f : 1750.f;
    gravity = shield ? 0.f : 150.f;
    duration = shield ? 5.f : 2.5f;
    density = 0;
    add_delay = .1f;
    requested_alpha = shield || type == 18 ? 1.f : .5f;
    min_life = shield ? .5f : .75f;
    max_life = shield ? .75f : type == 18 ? .9f : 1.f;
    authored_count = shield ? 24 : 18;
    flags = 0;
    ramp_down = .25f;
    position_radius = shield ? 45.f : type == 18 ? 20.f : 25.f;
    offset = shield ? DVector{} : DVector{-25, 0, 0};
    rotations =
        shield ? DVector{draw(random, 0, .01f), draw(random, 0, .015f), draw(random, 0, .01f)} : DVector{0, .025f, 0};
    min_color = shield ? DVector{192, 192, 0} : DVector{0, 0, 255};
    max_color = shield || type == 18 ? DVector{255, 255, 255} : DVector{196, 196, 255};
    texture = shield || type == 18 ? "SpriteTextures\\Lensflare_1.dtx" : "SpriteTextures\\smoke64_2.dtx";
    initialized_ = true;
    return true;
}
bool CObjectParticleFX::CreateObject(const Resources& resources, NodePose pose, double now, std::string& failure)
{
    failure.clear();
    if (!initialized_ || created_ || !pose_valid(pose) || !std::isfinite(now) || now < 0)
    {
        failure = "Invalid object particle creation state/pose/time";
        return false;
    }
    if (!resources.find(texture))
    {
        failure = "Object particle texture is not indexed: " + texture;
        return false;
    }
    origin = position = last_position = last_object_position = pose.position;
    start_time = last_time = now;
    // BaseParticleSystemFX creates an identity-rotation, white/opaque particle system.
    // MOVINGSOURCE and ROTATINGSOURCE keep that system transform fixed forever.
    created_ = true;
    return true;
}
bool CObjectParticleFX::AddParticles(std::minstd_rand& random, std::size_t budget)
{
    if (recipe >= 12 && recipe <= 17)
        return AddCylinderParticles(random, budget);
    if (recipe != 1 && recipe != 34)
        return AddShellParticles(random, budget);
    const auto delta = vector_subtract(last_position, position);
    const float distance = std::sqrt(vector_dot(delta, delta));
    const float samples = distance / (density * radius / 1000.f);
    if (!std::isfinite(samples) || samples > 2048 || particles.size() > budget ||
        std::size_t(samples) > budget - particles.size())
    {
        error = "Smoke-trail particle capacity exceeded";
        return false;
    }
    const auto count = static_cast<unsigned>(samples);
    // Source divides by zero here for short/stopped travel, but emits no particles.
    // Skip the unused division without manufacturing a sample or consuming RNG.
    if (!count)
        return true;
    const auto step = vector_scale(delta, 1.f / count);
    auto start = vector_subtract(position, origin);
    auto forward = rotate_vector(source_rotation, {0, 0, 1});
    const float length = std::sqrt(vector_dot(forward, forward));
    if (!std::isfinite(length) || length < 1e-6f || !physics_vector(start) ||
        !physics_vector(vector_subtract(last_position, origin)))
    {
        error = "Invalid smoke-trail emission direction/offset";
        return false;
    }
    forward = vector_scale(forward, 1 / length);
    particles.reserve(particles.size() + count);
    for (unsigned i = 0; i < count; ++i)
    {
        const float speed = draw(random, 20, recipe == 34 ? 40 : 50);
        const float ratio = draw(random, 0, 1); // One shared color ratio, not three channel draws.
        const float color = (96 + (192 - 96) * ratio) / 255.f;
        const float life = draw(random, min_life, max_life);
        particles.push_back({start, vector_scale(forward, speed), life,
                             recipe == 34 ? DVector{(128 + 127 * ratio) / 255.f, (128 + 127 * ratio) / 255.f, 1}
                                          : DVector{color, color, color}});
        start = vector_add(start, step);
    }
    emitted += count;
    return true;
}
bool CObjectParticleFX::AddCylinderParticles(std::minstd_rand& random, std::size_t budget)
{
    // Original EMITCYLINDER spans twice the owner-to-system offset, not owner travel.
    const auto delta = vector_subtract(last_object_position, position);
    const float length = std::sqrt(vector_dot(delta, delta)), magnitude = length * 2;
    const float samples = magnitude / (density * radius / 1000.f);
    if (!std::isfinite(samples) || samples > 2048)
    {
        error = "Voodoo cylinder capacity exceeded";
        return false;
    }
    const auto copies = std::max(1u, static_cast<unsigned>(samples));
    const auto count = copies * authored_count;
    if (particles.size() > budget || count > budget - particles.size() || particles.size() + count > 2048)
    {
        error = "Voodoo particle burst capacity exceeded";
        return false;
    }
    const auto forward = length > 1e-6f ? vector_scale(delta, 1 / length) : DVector{0, 0, 1};
    // Stable AlignRotation basis, including the vertical offset used by these recipes.
    DVector right{forward.z, 0, -forward.x};
    const float right_length = std::sqrt(vector_dot(right, right));
    right = right_length > 1e-6f ? vector_scale(right, 1 / right_length) : DVector{1, 0, 0};
    DVector offset{};
    const auto step = vector_scale(forward, magnitude / copies);
    particles.reserve(particles.size() + count);
    for (unsigned c = 0; c < copies; ++c)
    {
        for (unsigned i = 0; i < authored_count; ++i)
        {
            const auto previous_right = right;
            const float angle = draw(random, -std::numbers::pi_v<float>, std::numbers::pi_v<float>);
            const DVector cross{forward.y * right.z - forward.z * right.y, forward.z * right.x - forward.x * right.z,
                                forward.x * right.y - forward.y * right.x};
            right = vector_add(vector_scale(right, std::cos(angle)), vector_scale(cross, std::sin(angle)));
            const float speed = draw(random, 25, 25); // Original constant-speed draw still advances RNG.
            const auto color = vector_scale(
                vector_add(min_color, vector_scale(vector_subtract(max_color, min_color), draw(random, 0, 1))),
                1 / 255.f);
            particles.push_back({vector_add(vector_scale(previous_right, position_radius), offset),
                                 vector_scale(previous_right, speed), draw(random, min_life, max_life), color});
        }
        offset = vector_add(offset, step);
    }
    emitted += count;
    return true;
}
bool CObjectParticleFX::AddShellParticles(std::minstd_rand& random, std::size_t budget)
{
    const auto delta = vector_subtract(last_position, position);
    const float distance = std::sqrt(vector_dot(delta, delta));
    const float samples = (recipe == 3 || recipe == 21 || recipe == 31 || recipe == 32 || recipe == 33)
                              ? distance / (density * radius / 1000.f)
                              : 1.f;
    if (!std::isfinite(samples) || samples > 2048)
    {
        error = "Object particle stream capacity exceeded";
        return false;
    }
    const unsigned copies = static_cast<unsigned>(samples), count = copies * authored_count;
    if (particles.size() > budget || count > budget - particles.size() || particles.size() + count > 2048)
    {
        error = "Object particle burst capacity exceeded";
        return false;
    }
    auto basis = source_rotation;
    if (!count)
        return true; // Source's unused zero-count division is skipped.
    const auto step = vector_scale(delta, 1.f / copies);
    particles.reserve(particles.size() + count);
    for (unsigned i = 0; i < count; ++i)
    {
        // Cache axes BEFORE rotating. The source uses the preceding right axis
        // for this particle, and carries the rotated basis into the next one.
        auto right = rotate_vector(basis, {1, 0, 0});
        const auto forward = rotate_vector(basis, {0, 0, 1});
        around(basis, forward, draw(random, -std::numbers::pi_v<float>, std::numbers::pi_v<float>));
        if (recipe == 6 || recipe == 8 || recipe == 3 || recipe == 19 || recipe == 21 || recipe == 24 || recipe == 31 ||
            recipe == 32 || recipe == 33)
            around(basis, right, draw(random, -std::numbers::pi_v<float>, std::numbers::pi_v<float>));
        right = vector_scale(right, 1 / std::sqrt(vector_dot(right, right)));
        const float speed = draw(random,
                                 recipe == 6                      ? 25.f
                                 : recipe == 8                    ? 50.f
                                 : (recipe == 31 || recipe == 33) ? -5.f
                                                                  : 0.f,
                                 recipe == 6                      ? 50.f
                                 : recipe == 8                    ? 75.f
                                 : (recipe == 31 || recipe == 33) ? 5.f
                                 : recipe == 21 || recipe == 24   ? 25.f
                                                                : 0.f); // Original zero-speed draw still consumes RNG.
        float comet_distance = 0;
        auto start = vector_scale(right, recipe == 6 || recipe == 8 || recipe == 3 || recipe == 21 || recipe == 24 ||
                                                 recipe == 31 || recipe == 32 || recipe == 33
                                             ? (comet_distance = draw(random, 0, position_radius))
                                             : position_radius);
        if (recipe == 6 || recipe == 8 || recipe == 3 || recipe == 21 || recipe == 24 || recipe == 31 || recipe == 32 ||
            recipe == 33)
            start = vector_add(
                start, vector_add(vector_subtract(position, origin), vector_scale(step, float(i / authored_count))));
        const auto color = vector_scale(
            vector_add(min_color, vector_scale(vector_subtract(max_color, min_color), draw(random, 0, 1))), 1 / 255.f);
        particles.push_back({start, vector_scale(right, speed),
                             recipe == 3 ? max_life - (max_life - min_life) * (comet_distance / position_radius)
                                         : draw(random, min_life, max_life),
                             color});
    }
    emitted += count;
    return true;
}
bool CObjectParticleFX::Update(double now, const NodePose* owner, std::minstd_rand& random, std::size_t budget)
{
    error.clear();
    if (!created_ || !std::isfinite(now) || now < last_time || (owner && !pose_valid(*owner)))
    {
        error = "Invalid object particle state, parent pose or clock";
        return false;
    }
    if (!owner)
        return false; // Checked equivalent of WantRemove clearing m_hServerObject.
    if (!advance_particles(particles, gravity, now - last_time, error, recipe == 1 ? max_particle_capacity : 2048))
        return false;
    last_time = now;
    if (now < next_update)
        return true;
    next_update = now + .001;
    ++updates;
    const float elapsed = static_cast<float>(now - start_time);
    if (!std::isfinite(elapsed))
    {
        error = "Object particle elapsed time overflow";
        return false;
    }
    if (first_update)
    {
        last_add_time = 0;
        first_update = false;
        return true;
    }
    if (duration)
    {
        if (duration < elapsed)
            return false;
    }
    else if (recipe == 6 || recipe == 8 || recipe == 1 || recipe == 34 || recipe == 3 || recipe == 31 || recipe == 32 ||
             recipe == 33)
    {
        const auto difference = vector_subtract(owner->position, last_object_position);
        if (difference.x == 0 && difference.y == 0 && difference.z == 0)
        {
            moving = false;
            duration = elapsed + max_life + ((recipe == 6 || recipe == 8) ? ramp_down : 0.f);
        } // Never resumes in single player; only sparks request an alpha ramp.
        last_object_position = owner->position;
    }
    if (recipe == 1 || recipe == 34)
    {
        if (moving)
            position = owner->position;
        // ALIGNROTATION affects emission, not the fixed smoke system transform.
        source_rotation = owner->rotation;
    }
    else if (recipe == 6 || recipe == 8 || recipe == 3 || recipe == 21 || recipe == 24 || recipe == 31 ||
             recipe == 32 || recipe == 33)
    {
        if (moving)
            position = owner->position;
        // MOVINGSOURCE keeps the system fixed; stream recipes interpolate backward.
        // Only sparks enable the alpha ramp evaluated below.
    }
    else
    {
        last_object_position = owner->position;
        position = vector_add(owner->position, rotate_vector(owner->rotation, {offset.y, offset.x, offset.z}));
        if (!physics_vector(position))
        {
            error = "Object particle offset exceeds bounds";
            return false;
        }
        source_rotation = system_rotation;
        // Later-engine EulerRotate reference: recalculate each local axis in XYZ order.
        if (rotations.x)
            around(source_rotation, rotate_vector(source_rotation, {1, 0, 0}), rotations.x * std::numbers::pi_v<float>);
        if (rotations.y)
            around(source_rotation, rotate_vector(source_rotation, {0, 1, 0}), rotations.y * std::numbers::pi_v<float>);
        if (rotations.z)
            around(source_rotation, rotate_vector(source_rotation, {0, 0, 1}), rotations.z * std::numbers::pi_v<float>);
        system_rotation = source_rotation;
        ramping = elapsed >= duration - ramp_down;
        ramp_ratio = ramping ? std::max(0.f, (duration - elapsed) / ramp_down) : 1.f;
        if (ramping)
            alpha = requested_alpha * ramp_ratio;
    }
    if (recipe == 6 || recipe == 8)
    {
        ramping = duration && elapsed >= duration - ramp_down;
        ramp_ratio = ramping ? std::max(0.f, (duration - elapsed) / ramp_down) : 1.f;
        if (ramping)
            alpha = requested_alpha * ramp_ratio;
    }
    if (now - last_add_time >= add_delay)
    {
        last_add_time = now;
        if (!AddParticles(random, budget))
            return false;
        ++bursts;
        last_position = position;
    }
    return true;
}
