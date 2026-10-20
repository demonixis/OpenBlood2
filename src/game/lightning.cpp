#include "lightning.h"
#include <algorithm>
namespace OpenBlood2
{
    std::size_t LightningBolts::live_particles() const
    {
        std::size_t count = 0;
        for (const auto& bolt : active)
            for (const auto& segment : bolt.segments)
                count += segment.particles.size();
        return count;
    }
    std::uint64_t LightningBolts::publish(const LightningSettings& settings, std::string_view source,
                                          const Resources& resources, double now, std::string& error)
    {
        error.clear();
        LightningReport report;
        report.settings = settings;
        CLightningBoltFX fx;
        if (source.size() > 100 || source.find('\0') != std::string_view::npos || next_id_ == UINT64_MAX)
            error = "Invalid lightning source/id";
        else
        {
            report.source = source;
            report.id = next_id_++;
            if (active.size() >= 128)
                error = "Lightning system budget exceeded";
            else if (fx.CreateObject(settings, resources, now, random_, error))
            {
                report.segments = unsigned(fx.segments.size());
                for (const auto& segment : fx.segments)
                    report.particles += unsigned(segment.particles.size());
                if (report.particles > 16384 - live_particles())
                    error = "Lightning particle budget exceeded";
                else
                {
                    fx.id = report.id;
                    active.push_back(std::move(fx));
                    ++started;
                }
            }
        }
        report.error = error;
        if (!error.empty())
        {
            ++failed;
            report.ended = true;
        }
        const auto id = report.id;
        if (reports.size() == 64)
            reports.erase(reports.begin());
        reports.push_back(std::move(report));
        return id;
    }
    bool LightningBolts::advance(double now)
    {
        bool valid = true;
        std::erase_if(active,
                      [&](auto& fx)
                      {
                          if (fx.Update(now))
                              return false;
                          for (auto& report : reports)
                              if (report.id == fx.id)
                              {
                                  report.ended = true;
                                  report.error = fx.error;
                              }
                          if (fx.error.empty())
                              ++expired;
                          else
                          {
                              ++failed;
                              update_error = fx.error;
                              valid = false;
                          }
                          return true;
                      });
        return valid;
    }
    void LightningBolts::clear()
    {
        active.clear();
        reports.clear();
        update_error.clear();
        started = expired = failed = 0;
        random_.seed(1); // Identity counters survive unload.
    }
} // namespace OpenBlood2
