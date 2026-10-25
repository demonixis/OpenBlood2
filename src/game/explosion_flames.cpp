#include "game/explosion_flames.h"
#include "engine/collision.h"
#include <algorithm>
#include <cmath>
namespace OpenBlood2
{
    bool original_explosion_flames(DDWORD type, DVector pos, DVector normal, std::vector<ExplosionFlameSettings>& out,
                                   std::string& error)
    {
        out.clear();
        error.clear();
        if (!physics_vector(pos) || !physics_vector(normal) || vector_dot(normal, normal) < 1e-12f)
        {
            error = "Explosion requires a finite position and nonzero normal";
            return false;
        }
        if (type <= 7 || (type >= 9 && type <= 11))
            return true;
        if (type == 26)
        {
            const auto n = vector_scale(normal, 1 / std::sqrt(vector_dot(normal, normal)));
            ExplosionFlameSettings s;
            s.position = vector_add(pos, vector_scale(n, 20));
            s.normal = n;
            s.colors = {{{128.f / 255, 128.f / 255, 128.f / 255}, {192.f / 255, 192.f / 255, 192.f / 255}, {1, 1, 1}}};
            s.lifetimes = {.7f, .85f, 1};
            s.life_offsets = {.15f, .25f, .35f};
            s.ramp_up = 0;
            s.duration = 1.25f;
            s.ramp_down = .25f;
            s.count = 3;
            s.radius = 2500;
            s.position_radius = 7.5f;
            s.gravity = 200;
            s.delay = .1f;
            s.alpha = .25f;
            s.fade = 1;
            s.texture = "SpriteTextures\\smoke64_2.dtx";
            out.push_back(s);
            return true;
        }
        if (type != 8)
        {
            error = "Explosion flame recipe unavailable";
            return false;
        }
        const float length = std::sqrt(vector_dot(normal, normal));
        normal = {normal.x / length, normal.y / length, normal.z / length};
        ExplosionFlameSettings s;
        s.position = vector_add(pos, {30 * normal.x, 30 * normal.y, 30 * normal.z});
        s.normal = normal;
        s.colors = {{{1, 1, 1}, {200.f / 255, 200.f / 255, 1}, {150.f / 255, 150.f / 255, 1}}};
        s.lifetimes = {.75f, .875f, 1};
        s.life_offsets = {.25f, .25f, .25f};
        s.ramp_up = .5f;
        s.duration = 2.5f;
        s.ramp_down = 0;
        s.count = 4;
        s.radius = 4000;
        s.position_radius = 0;
        s.gravity = 0;
        s.velocity = 125;
        s.delay = .33f;
        s.alpha = .85f;
        s.fade = 1;
        s.ramp_flags = 1;
        s.texture = "SpriteTextures\\teslaprime_3.dtx";
        out.push_back(s);
        return true;
    }
    std::size_t ExplosionFlames::live_particles() const
    {
        std::size_t count = 0;
        for (const auto& fx : active)
            count += fx.particles.size();
        return count;
    }
    std::uint64_t ExplosionFlames::publish(DDWORD type, DVector pos, DVector normal, std::string_view source,
                                           const Resources& resources, double now, std::string& error)
    {
        error.clear();
        if (source.size() > 100 || source.find('\0') != std::string_view::npos || next_event_ == UINT64_MAX)
        {
            error = "Invalid explosion flame source/event";
            return 0;
        }
        ExplosionFlameReport report{next_event_++, type, std::string(source), {}, {}};
        std::vector<ExplosionFlameSettings> settings;
        if (original_explosion_flames(type, pos, normal, settings, report.error))
            for (const auto& setting : settings)
            {
                CExplosionFlameFX fx;
                std::string failure;
                const auto live = live_particles();
                if (active.size() >= 64 || live >= 4096 || next_id_ == UINT64_MAX)
                    failure = "Explosion flame capacity exhausted";
                else if (fx.Init(setting, failure) && fx.CreateObject(resources, now, random_, failure, 4096 - live))
                {
                    fx.id = next_id_++;
                    fx.event = report.id;
                    report.flames.push_back(fx.id);
                    active.push_back(std::move(fx));
                    ++started;
                }
                if (!failure.empty())
                {
                    ++failed;
                    report.error = failure;
                }
            }
        else
            ++failed;
        error = report.error;
        const auto id = report.id;
        if (reports.size() == 64)
            reports.erase(reports.begin());
        reports.push_back(std::move(report));
        return id;
    }
    bool ExplosionFlames::advance(double now)
    {
        bool valid = true;
        // Iterate explicitly: compacting with erase_if would invalidate total-count accounting.
        std::size_t live = live_particles();
        for (auto it = active.begin(); it != active.end();)
        {
            live -= it->particles.size();
            if (it->Update(now, random_, live < 4096 ? 4096 - live : 0))
            {
                live += it->particles.size();
                ++it;
                continue;
            }
            if (it->error.empty())
                ++expired;
            else
            {
                ++failed;
                valid = false;
                update_error = it->error;
                for (auto& report : reports)
                    if (report.id == it->event)
                        report.error = it->error;
            }
            it = active.erase(it);
        }
        return valid;
    }
    void ExplosionFlames::clear()
    {
        active.clear();
        reports.clear();
        update_error.clear();
        started = expired = failed = 0;
        random_.seed(1); // IDs intentionally survive unload.
    }
} // namespace OpenBlood2
