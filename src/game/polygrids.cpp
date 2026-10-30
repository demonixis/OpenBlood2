#include "polygrids.h"
#include <algorithm>
using namespace OpenBlood2;
std::uint64_t PolyGrids::publish(const EffectOwner& owner, const PolyGridSettings& s, SpriteStore& store, double now,
                                 std::string& error)
{
    error.clear();
    PolyGridReport report;
    CPolyGridFX fx;
    fx.pose = owner.pose;
    if (!owner)
        error = "PolyGrid owner/pose unavailable";
    else if (active.size() >= 128 || next_id_ == UINT64_MAX ||
             std::any_of(active.begin(), active.end(), [&](auto& i) { return i.owner == owner.id; }))
        error = "PolyGrid capacity exhausted or duplicate owner";
    else if (fx.Init(s, store, now, error))
    {
        report.id = fx.id = next_id_++;
        report.source = owner.name;
        fx.visible = (owner.user_flags & 1u) != 0;
        active.push_back({owner.id, std::move(fx)});
        ++started;
    }
    if (!error.empty())
        ++failed;
    report.error = error;
    auto id = report.id;
    if (reports.size() == 128)
        reports.erase(reports.begin());
    reports.push_back(std::move(report));
    return id;
}
bool PolyGrids::advance(double now, const ClientWorld& world)
{
    bool ok = true;
    std::erase_if(active,
                  [&](auto& i)
                  {
                      auto& fx = i.effect;
                      const auto* owner = world.get(i.owner);
                      if (owner)
                          fx.pose.position = owner->pose.position;
                      if (owner && fx.Update(now, (owner->user_flags & 1u) != 0, phases))
                          return false;
                      if (owner)
                      {
                          ++failed;
                          ok = false;
                          if (fx.error.empty())
                              fx.error = "PolyGrid position unavailable";
                          update_error = fx.error;
                      }
                      else
                          ++expired;
                      for (auto& r : reports)
                          if (r.id == fx.id)
                          {
                              r.removed = true;
                              r.error = fx.error;
                          }
                      return true;
                  });
    return ok;
}
void PolyGrids::clear()
{
    active.clear();
    reports.clear();
    update_error.clear();
    phases = {0, 1, 4, 3};
    started = expired = failed = 0;
}
