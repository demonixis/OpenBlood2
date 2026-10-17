#include "game/sparks.h"
namespace OpenBlood2
{
    std::size_t Sparks::live_particles() const
    {
        std::size_t n = 0;
        for (const auto& fx : active)
            n += fx.particles.size();
        return n;
    }
    std::uint64_t Sparks::publish(const SparkSettings& s, std::string_view source, const Resources& resources,
                                  double now, std::string& error)
    {
        error.clear();
        if (source.size() > 100 || source.find('\0') != std::string_view::npos || next_event_ == UINT64_MAX)
        {
            error = "Invalid spark source/event";
            return 0;
        }
        SparkReport r;
        r.id = next_event_++;
        r.source = source;
        r.settings = s;
        CSparksFX fx;
        if (active.size() >= 128 || s.count > 16384 - live_particles() || next_id_ == UINT64_MAX)
            error = "Spark capacity exhausted";
        else if (fx.CreateObject(s, resources, now, random_, error))
        {
            fx.id = next_id_++;
            fx.event = r.id;
            r.effect = fx.id;
            active.push_back(std::move(fx));
            ++started;
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
        return id;
    }
    bool Sparks::advance(double now)
    {
        bool ok = true;
        for (std::size_t i = 0; i < active.size();)
        {
            auto& fx = active[i];
            if (now >= fx.last_time && now < fx.next_update)
            {
                ++i;
                continue;
            }
            const bool alive = fx.Update(now);
            // Reclaim only the invisible exhausted system, without truncating particles
            // or emulating the unavailable original shared SFX list eviction policy.
            if (alive && !(now > fx.start_time + fx.settings.duration && fx.particles.empty()))
            {
                ++i;
                continue;
            }
            for (auto& r : reports)
                if (r.id == fx.event)
                {
                    r.ended = true;
                    r.error = fx.error;
                    break;
                }
            if (!fx.error.empty())
            {
                update_error = fx.error;
                ++failed;
                ok = false;
            }
            else
                ++retired;
            active.erase(active.begin() + i);
        }
        return ok;
    }
    void Sparks::clear()
    {
        active.clear();
        reports.clear();
        update_error.clear();
        started = retired = failed = 0;
        random_.seed(1); // IDs survive unload.
    }
} // namespace OpenBlood2
