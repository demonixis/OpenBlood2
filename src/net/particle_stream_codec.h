#pragma once
#include "game/particle_stream_settings.h"
#include "net/codec.h"
namespace OpenBlood2::Net
{
    inline void write_particle_stream(Writer& w, const ParticleStreamSettings& s)
    {
        w.u8(s.system);
        w.f32(s.particles_per_second);
        w.f32(s.burst_wait);
        w.f32(s.rotation_velocity);
        w.f32(s.radius);
        w.f32(s.position_radius);
        w.f32(s.min_velocity);
        w.f32(s.max_velocity);
        w.f32(s.spread);
        w.u32(s.count);
        for (auto v : {s.min_color, s.max_color})
        {
            w.f32(v.x);
            w.f32(v.y);
            w.f32(v.z);
        }
        w.f32(s.alpha);
        w.f32(s.min_life);
        w.f32(s.max_life);
        w.f32(s.ramp_time);
        w.f32(s.delay);
        w.f32(s.gravity);
        w.u8(s.ramp_flags);
        w.u8(s.on);
        w.text(s.texture, 1000);
        w.f32(s.sound_radius);
        for (const auto& sound : s.sounds)
            w.text(sound, 1000);
    }
    inline bool read_particle_stream(Reader& r, ParticleStreamSettings& s)
    {
        const auto system = r.u8();
        s.system = system != 0;
        s.particles_per_second = r.f32();
        s.burst_wait = r.f32();
        s.rotation_velocity = r.f32();
        s.radius = r.f32();
        s.position_radius = r.f32();
        s.min_velocity = r.f32();
        s.max_velocity = r.f32();
        s.spread = r.f32();
        s.count = r.u32();
        s.min_color = {r.f32(), r.f32(), r.f32()};
        s.max_color = {r.f32(), r.f32(), r.f32()};
        s.alpha = r.f32();
        s.min_life = r.f32();
        s.max_life = r.f32();
        s.ramp_time = r.f32();
        s.delay = r.f32();
        s.gravity = r.f32();
        s.ramp_flags = r.u8();
        const auto on = r.u8();
        s.on = on != 0;
        s.texture = r.text(1000);
        s.sound_radius = r.f32();
        for (auto& sound : s.sounds)
            sound = r.text(1000);
        return r.good() && system <= 1 && on <= 1 && valid_particle_stream(s);
    }
} // namespace OpenBlood2::Net
