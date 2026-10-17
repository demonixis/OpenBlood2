#include "blood_splats.h"
#include "engine/collision.h"
#include <algorithm>
#include <cmath>
namespace OpenBlood2
{
    std::uint64_t BloodSplats::publish(const BloodSplatSettings& settings, std::string_view source, SpriteStore& store,
                                       double now, std::string& error)
    {
        error.clear();
        if (source.size() > 100 || source.find('\0') != std::string_view::npos || next_event_ == UINT64_MAX)
        {
            error = "Invalid blood splat source/event identity";
            return 0;
        }
        BloodSplatReport report;
        report.id = next_event_++;
        report.source = source;

        CBloodSplatFX effect;
        if (!std::isfinite(now) || now < 0)
            report.error = "Invalid blood splat publication clock";
        else if (effect.Init(settings, report.error))
        {
            report.settings = settings;
            // Original CreateObject list scan: stop at the eleventh EXISTING nearby
            // object; move only the nearest seen so far, preserving all other state.
            CBloodSplatFX* closest = nullptr;
            float closest_distance = 100;
            unsigned nearby = 0;
            for (auto& old : active)
            {
                const auto d = vector_subtract(old.position, settings.position);
                const float distance = vector_dot(d, d);
                if (distance >= 100)
                    continue;
                if (distance < closest_distance)
                {
                    closest = &old;
                    closest_distance = distance;
                }
                if (++nearby > 10)
                {
                    closest->position = settings.position;
                    report.effect = closest->id;
                    report.reused = true;
                    ++reused;
                    break;
                }
            }
            if (!report.reused)
            {
                if (active.size() >= 256 || next_id_ == UINT64_MAX)
                    report.error = "Blood splat capacity exhausted";
                else if (effect.CreateObject(store, now, random_, report.error))
                {
                    effect.id = report.effect = next_id_++;
                    effect.event = report.id;
                    active.push_back(std::move(effect));
                    ++started;
                }
            }
        }
        if (!report.error.empty())
            ++failed;
        error = report.error;
        const auto event = report.id;
        if (reports.size() == 64)
            reports.erase(reports.begin());
        reports.push_back(std::move(report));
        return event; // Retained request, not an unconditional successful service.
    }
    bool BloodSplats::advance(double now)
    {
        bool ok = true;
        std::erase_if(active,
                      [&](auto& effect)
                      {
                          if (effect.Update(now, random_))
                              return false;
                          if (effect.error.empty())
                              ++expired;
                          else
                          {
                              ++failed;
                              ok = false;
                              update_error = effect.error;
                          }
                          return true;
                      });
        return ok;
    }
    bool BloodSplats::terminate(std::uint64_t id)
    {
        for (auto& effect : active)
            if (effect.id == id)
            {
                effect.Term();
                return true;
            }
        return false;
    }
    void BloodSplats::clear()
    {
        active.clear();
        reports.clear();
        update_error.clear();
        started = reused = expired = failed = 0; // IDs deliberately survive unload.
    }
} // namespace OpenBlood2
