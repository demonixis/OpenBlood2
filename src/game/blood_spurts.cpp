#include "game/blood_spurts.h"
#include "game/legacy/server_de.h"
namespace OpenBlood2
{
    std::size_t BloodSpurts::live_particles() const
    {
        std::size_t count = 0;
        for (const auto& fx : active)
            count += fx.particles.size();
        return count;
    }
    void BloodSpurts::record(const CParticleExplosionFX& fx, bool ended)
    {
        for (auto& r : reports)
            if (r.id == fx.event)
            {
                r.emitted = fx.emitted;
                r.capacity_skips = fx.capacity_skips;
                r.impacts = fx.impacts;
                r.splat_event = fx.splat_event;
                r.error = fx.error;
                r.splat_error = fx.splat_error;
                r.sound_resource = fx.sound_resource;
                r.sound_error = fx.sound_error;
                r.ended = ended;
                break;
            }
    }
    std::uint64_t BloodSpurts::publish(const BloodSpurtSettings& s, std::string_view source, bool immediate,
                                       const Resources& resources, ServerDE& server, double now, std::string& error)
    {
        error.clear();
        if (source.size() > 100 || source.find('\0') != std::string_view::npos || next_event_ == UINT64_MAX)
        {
            error = "Invalid blood-spurt source/event";
            return 0;
        }
        BloodSpurtReport r;
        r.id = next_event_++;
        r.settings = s;
        r.source = source;
        r.immediate = immediate;
        CParticleExplosionFX fx;
        const auto live = live_particles();
        if (active.size() >= 128 || live > 16384 - 12 || next_id_ == UINT64_MAX)
            error = "Blood-spurt capacity exhausted";
        else if (fx.CreateObject(s, resources, server, now, random_, error))
        {
            fx.id = next_id_++;
            fx.event = r.id;
            r.effect = fx.id;
            if (immediate && !fx.Update(now, server.GetFrameTime(), server, random_, 16384 - live))
                error = fx.error;
            if (error.empty())
            {
                active.push_back(std::move(fx));
                ++started;
            }
        }
        r.error = error;
        if (!error.empty())
        {
            ++failed;
            r.ended = true;
        }
        if (reports.size() == 64)
            reports.erase(reports.begin());
        const auto id = r.id;
        reports.push_back(std::move(r));
        if (error.empty())
            record(active.back(), false);
        return id;
    }
    bool BloodSpurts::advance(double now, ServerDE& server)
    {
        bool ok = true;
        std::size_t total = live_particles();
        for (std::size_t i = 0; i < active.size();)
        {
            auto& fx = active[i];
            if (now >= fx.last_time && now < fx.next_update)
            {
                ++i;
                continue;
            }
            const auto before = fx.particles.size();
            bool alive = fx.Update(now, now - fx.last_time, server, random_, 16384 - total);
            total = total - before + fx.particles.size();
            record(fx, !alive);
            if (alive)
                ++i;
            else
            {
                if (fx.error.empty())
                    ++expired;
                else
                {
                    update_error = fx.error;
                    ++failed;
                    ok = false;
                }
                total -= fx.particles.size();
                active.erase(active.begin() + i);
            }
        }
        return ok;
    }
    void BloodSpurts::clear()
    {
        active.clear();
        reports.clear();
        update_error.clear();
        started = expired = failed = 0;
        random_.seed(1); // IDs deliberately survive world changes.
    }
} // namespace OpenBlood2
