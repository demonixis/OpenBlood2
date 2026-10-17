// Selected ClientShellDLL/ParticleExplosionFX.cpp and client_physics.cpp blood path.
#include "ParticleExplosionFX.h"
#include "BloodSplatFX.h"
#include "engine/collision.h"
#include "game/legacy/server_de.h"
#include <cmath>
using namespace OpenBlood2;
namespace
{
    float draw(std::minstd_rand& r, float lo, float hi)
    {
        // Reversed finite velocity endpoints are intentional original recipes.
        return lo + (hi - lo) * float(r() - r.min()) / float(r.max() - r.min());
    }
} // namespace
bool CParticleExplosionFX::CreateObject(const BloodSpurtSettings& s, const Resources& resources, ServerDE& server,
                                        double now, std::minstd_rand& rng, std::string& failure)
{
    failure.clear();
    if (!physics_vector(s.position) || !physics_vector(s.min_velocity) || !physics_vector(s.max_velocity) ||
        !std::isfinite(s.lifetime) || s.lifetime <= 0 || s.lifetime > 10 || !std::isfinite(s.fade_time) ||
        s.fade_time < 0 || s.fade_time >= s.lifetime || s.texture > 3 || !std::isfinite(now) || now < 0)
    {
        failure = "Invalid blood-spurt recipe";
        return false;
    }
    settings = s;
    if (!settings.texture)
        settings.texture = 1 + rng() % 3;
    texture = "spritetextures/particles/blooddrop_" + std::to_string(settings.texture) + ".dtx";
    if (!resources.find(texture))
    {
        failure = "Blood-drop texture not indexed: " + texture;
        return false;
    }
    position = previous = s.position;
    velocity = {draw(rng, s.min_velocity.x, s.max_velocity.x), draw(rng, s.min_velocity.y, s.max_velocity.y),
                draw(rng, s.min_velocity.z, s.max_velocity.z)};
    ContainerCode code = CC_NOTHING;
    if (!server.GetPointContainerCode(position, code))
    {
        failure = "Blood-spurt contents query unavailable";
        return false;
    }
    liquid = IsLiquid(code);
    last_time = next_update = now;
    return true;
}
bool CParticleExplosionFX::Update(double now, double dt, ServerDE& server, std::minstd_rand& rng, std::size_t available)
{
    if (!error.empty())
        return false;
    if (!std::isfinite(now) || now < last_time || !std::isfinite(dt) || dt < 0 || dt > 1)
    {
        error = "Invalid blood-spurt clock";
        return false;
    }
    if (first_update)
    {
        start_time = last_emission = now;
        first_update = false;
    }
    last_time = now;
    next_update = now + .001;
    ++updates;
    if (now > start_time + settings.lifetime)
        return false;
    if (now > start_time + settings.fade_time)
        alpha = float((start_time + settings.lifetime - now) / (settings.lifetime - settings.fade_time));
    if (!advance_particles(particles, -100, dt, error))
        return false;
    if (!emitting)
        return true;
    if (particles.size() + 12 > 2048 || available < 12)
    {
        error = "Blood-spurt particle capacity exhausted";
        return false;
    }
    // Original six samples, two drops each, BEFORE the emitter's current movement.
    const auto delta = vector_scale(vector_subtract(position, previous), 1.f / 6);
    auto point = vector_subtract(previous, settings.position);
    const float life_step = float(now - last_emission) / 6;
    for (int step = 0; step < 6; ++step)
    {
        for (int j = 0; j < 2; ++j)
        {
            Particle p;
            p.velocity = {draw(rng, 0, 0), draw(rng, -10, -5), draw(rng, 0, 0)};
            p.position = vector_add(point, {draw(rng, -.5f, .5f), draw(rng, -.5f, .5f), draw(rng, -.5f, .5f)});
            const float gray = draw(rng, 128, 128) / 255.f;
            p.color = {gray, gray, gray};
            p.lifetime = 10 + step * life_step;
            particles.push_back(p);
        }
        point = vector_add(point, delta);
    }
    emitted += 12;
    last_emission = now;
    if (vector_dot(velocity, velocity) < .01f)
        velocity = {};
    auto next = vector_add(position, vector_scale(velocity, float(dt)));
    // HALFGRAVITY is applied AFTER InitMovingObject for this effect (.20, not .50).
    velocity.y -= float(dt) * (liquid ? 100.f : 200.f);
    SegmentHit hit;
    const auto travel = vector_subtract(next, position);
    auto result = vector_dot(travel, travel) < 1e-12f ? SegmentResult::Miss : server.CastWorldRay(position, next, hit);
    if (result != SegmentResult::Miss && result != SegmentResult::Hit)
    {
        error = "Blood-spurt world collision unavailable";
        return false;
    }
    if (result == SegmentResult::Hit)
    {
        const float dot = -2 * vector_dot(velocity, hit.normal);
        velocity = vector_scale(vector_add(velocity, vector_scale(hit.normal, dot)), liquid ? .20f : .35f);
        if (dot > 0)
            next = vector_add(hit.point, vector_scale(hit.normal, .3f));
        emitting = false; // Original bBounce=false, independent of the low-speed rest test.
        ++impacts;
        SoundRequest sound;
        // Original B2_LITE GetRandom(0,NRES(5)) includes zero; do not alias a missing file.
        sound.resource = "sounds/gibs/flesh/gib_impact" + std::to_string(rng() % 4) + ".wav";
        sound.outer_radius = 300;
        sound.inner_radius = 30;
        server.PlayClientSound(sound, next, sound_error);
        BloodSplatSettings splat;
        splat.resource = "sprites/blood" + std::to_string(1 + rng() % 3) + ".spr";
        splat.position = next;
        splat.scale = .05f + draw(rng, -.05f, .05f);
        splat.grow_scale = hit.normal.y > .5f ? .01f : 0;
        if (align_rotation(hit.normal, hit.normal, splat.rotation))
            splat_event = server.PublishClientBloodSplat(splat, "particle impact", splat_error);
        else
            splat_error = "Invalid blood-spurt impact normal";
    }
    if (!physics_vector(next) || !physics_vector(velocity))
    {
        error = "Blood-spurt motion exceeds bounds";
        return false;
    }
    previous = position;
    position = next;
    return true;
}
