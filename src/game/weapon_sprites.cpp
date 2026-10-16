#include "weapon_sprites.h"
#include <algorithm>
namespace OpenBlood2
{
    std::uint64_t WeaponSprites::publish(const WeaponSpriteSettings& settings, std::string_view source,
                                         SpriteStore& store, double now, std::string& error)
    {
        error.clear();
        if (source.size() > 100 || source.find('\0') != std::string_view::npos || next_event_ == UINT64_MAX)
        {
            error = "Invalid weapon sprite source/event identity";
            return 0;
        }
        WeaponSpriteReport report;
        report.id = next_event_++;
        report.settings = settings;
        report.source = source;
        auto create = [&](const auto& setting, auto& active, std::uint64_t& id)
        {
            if (!setting)
                return;
            typename std::decay_t<decltype(active)>::value_type fx;
            std::string failure;
            if (flashes.size() + smoke.size() >= 256 || next_id_ == UINT64_MAX)
                failure = "Weapon sprite capacity exhausted";
            else if (fx.Init(*setting, failure) && fx.CreateObject(store, now, failure))
            {
                fx.id = id = next_id_++;
                fx.event = report.id;
                active.push_back(std::move(fx));
                ++started;
                return;
            }
            ++failed;
            if (!report.error.empty())
                report.error += "; ";
            report.error += setting->resource.value_or("default") + ": " + failure;
        };
        create(settings.flash, flashes, report.flash);
        create(settings.smoke, smoke, report.smoke); // Independent CreateSFX: failed flash never cancels smoke.
        if (settings.bubbles_unavailable)
        {
            ++failed;
            report.error += "Underwater impact bubbles unavailable";
        }
        error = report.error;
        const auto id = report.id;
        if (reports.size() == 64)
            reports.erase(reports.begin());
        reports.push_back(std::move(report));
        return id; // Report identity, not success.
    }
    bool WeaponSprites::advance(double now)
    {
        bool ok = true;
        auto advance = [&](auto& active)
        {
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
                                  update_error = fx.error;
                                  ok = false;
                              }
                              return true;
                          });
        };
        advance(flashes);
        advance(smoke);
        return ok;
    }
    void WeaponSprites::clear()
    {
        flashes.clear();
        smoke.clear();
        reports.clear();
        update_error.clear();
        started = expired = failed = 0; // Identities deliberately survive world reload.
    }
} // namespace OpenBlood2
