#include "game/laser_beams.h"
#include <algorithm>
namespace OpenBlood2
{
    std::uint64_t LaserBeams::publish(DVector from, DVector to, std::string_view source, ModelStore& models,
                                      const Resources& resources, double now, std::string& error, DBYTE type)
    {
        error.clear();
        LaserBeamReport report;
        if (source.size() > 100 || source.find('\0') != std::string_view::npos)
            error = "Invalid laser source name";
        else
        {
            report.source = source;
            report.from = from;
            report.to = to;
            report.type = type;
            CLaserBeamFX fx;
            fx.SetupBeam(type);
            if (active.size() >= 128 || next_id_ == UINT64_MAX)
                error = "Laser capacity exhausted";
            else if (fx.CreateObject(models, resources, from, to, now, error))
            {
                report.id = fx.id = next_id_++;
                active.push_back(std::move(fx));
                ++started;
            }
        }
        if (!error.empty())
            ++failed;
        report.error = error;
        const auto id = report.id;
        if (reports.size() == 64)
            reports.erase(reports.begin());
        reports.push_back(std::move(report));
        return id;
    }
    bool LaserBeams::advance(double now)
    {
        bool valid = true;
        std::erase_if(active,
                      [&](auto& fx)
                      {
                          if (fx.Update(now))
                              return false;
                          if (fx.error.empty())
                              ++expired;
                          else
                          {
                              ++failed;
                              valid = false;
                              update_error = fx.error;
                          }
                          return true;
                      });
        return valid;
    }
    void LaserBeams::clear()
    {
        active.clear();
        reports.clear();
        started = expired = failed = 0;
        update_error.clear(); // IDs are never reused across a world unload.
    }
} // namespace OpenBlood2
