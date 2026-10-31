#pragma once
#include "game/legacy/basedefs_de.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <string>
namespace OpenBlood2
{
    struct ParticleStreamSettings
    {
        // Original CParticleSystemFX has a different emission clock and local-axis velocity.
        bool system = false;
        float particles_per_second = 0, burst_wait = 0, rotation_velocity = 0;
        float radius = 1000, position_radius = 1, min_velocity = 0, max_velocity = 25, spread = 0;
        unsigned count = 1;
        DVector min_color{255, 255, 255}, max_color{255, 255, 255};
        float alpha = 1, min_life = 1, max_life = 2, ramp_time = 0, delay = .1f, gravity = 15;
        DBYTE ramp_flags = 0;
        bool on = false;
        std::string texture = "SpriteTextures\\drop32_1.dtx";
        // Sent by the original server, but never read by its SFXMgr client branch.
        float sound_radius = 200;
        std::array<std::string, 3> sounds;
    };
    inline bool valid_particle_stream(const ParticleStreamSettings& s)
    {
        auto bound = [](float v, float lo, float hi) { return std::isfinite(v) && v >= lo && v <= hi; };
        auto color = [&](DVector v) { return bound(v.x, 0, 255) && bound(v.y, 0, 255) && bound(v.z, 0, 255); };
        auto path = [](const std::string& p) { return p.size() <= 1000 && p.find('\0') == p.npos; };
        return bound(s.particles_per_second, 0, 10000) && bound(s.burst_wait, 0, 600) &&
               bound(s.rotation_velocity, -10000, 10000) && bound(s.radius, 0, 640000) &&
               bound(s.position_radius, 0, 10000) && bound(s.min_velocity, s.system ? -10000 : 0, 10000) &&
               bound(s.max_velocity, s.min_velocity, 10000) && bound(s.spread, 0, 10000) && s.count <= 256 &&
               color(s.min_color) && color(s.max_color) && std::isfinite(s.alpha) && bound(s.min_life, 0, 600) &&
               bound(s.max_life, s.min_life, 600) && bound(s.ramp_time, 0, 600) && bound(s.delay, 0, 600) &&
               bound(s.gravity, -10000, 10000) && s.ramp_flags <= 15 && path(s.texture) &&
               bound(s.sound_radius, 0, 100000) && std::all_of(s.sounds.begin(), s.sounds.end(), path);
    }

    inline bool same_particle_stream(const ParticleStreamSettings& a, const ParticleStreamSettings& b)
    {
        return a.system == b.system && a.particles_per_second == b.particles_per_second &&
               a.burst_wait == b.burst_wait && a.rotation_velocity == b.rotation_velocity && a.radius == b.radius &&
               a.position_radius == b.position_radius && a.min_velocity == b.min_velocity &&
               a.max_velocity == b.max_velocity && a.spread == b.spread && a.count == b.count && a.alpha == b.alpha &&
               a.min_life == b.min_life && a.max_life == b.max_life && a.ramp_time == b.ramp_time &&
               a.delay == b.delay && a.gravity == b.gravity && a.ramp_flags == b.ramp_flags && a.on == b.on &&
               a.texture == b.texture && a.sound_radius == b.sound_radius && a.sounds == b.sounds &&
               a.min_color.x == b.min_color.x && a.min_color.y == b.min_color.y && a.min_color.z == b.min_color.z &&
               a.max_color.x == b.max_color.x && a.max_color.y == b.max_color.y && a.max_color.z == b.max_color.z;
    }
} // namespace OpenBlood2
