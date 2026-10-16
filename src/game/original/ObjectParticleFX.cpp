#include "ObjectParticleFX.h"
#include "engine/collision.h"
#include <cmath>
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
    if (type != 1 || !owner)
    {
        failure = "Unsupported object particle recipe or absent parent";
        return false;
    }
    parent = owner;
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
        failure = "Smoke-trail texture is not indexed: " + texture;
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
        const float speed = draw(random, 20, 50);
        const float ratio = draw(random, 0, 1); // One shared color ratio, not three channel draws.
        const float color = (96 + (192 - 96) * ratio) / 255.f;
        const float life = draw(random, min_life, max_life);
        particles.push_back({start, vector_scale(forward, speed), life, {color, color, color}});
        start = vector_add(start, step);
    }
    emitted += count;
    return true;
}
bool CObjectParticleFX::Update(double now, const NodePose* owner, std::minstd_rand& random, std::size_t budget)
{
    error.clear();
    if (!created_ || !std::isfinite(now) || now < last_time || (owner && !pose_valid(*owner)))
    {
        error = "Invalid smoke-trail state, parent pose or clock";
        return false;
    }
    if (!owner)
        return false; // Checked equivalent of WantRemove clearing m_hServerObject.
    if (!advance_particles(particles, gravity, now - last_time, error))
        return false;
    last_time = now;
    if (now < next_update)
        return true;
    next_update = now + .001;
    ++updates;
    const float elapsed = static_cast<float>(now - start_time);
    if (!std::isfinite(elapsed))
    {
        error = "Smoke-trail elapsed time overflow";
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
    else
    {
        const auto difference = vector_subtract(owner->position, last_object_position);
        if (difference.x == 0 && difference.y == 0 && difference.z == 0)
        {
            moving = false;
            duration = elapsed + max_life;
        } // Zero ramp-down time; never resumes in single player.
        last_object_position = owner->position;
    }
    if (moving)
        position = owner->position;    // Recipe offset is zero.
    source_rotation = owner->rotation; // ALIGNROTATION influences forward emission, not the fixed system transform.
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
