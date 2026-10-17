#include "polygrids.h"
#include "server_de.h"
#include <algorithm>
using namespace OpenBlood2;
std::uint64_t PolyGrids::publish(HOBJECT owner, const PolyGridSettings& s, ServerDE& server, SpriteStore& store,
                                 double now, std::string& error)
{
    error.clear();
    PolyGridReport report;
    CPolyGridFX fx;
    if (!server.IsObjectAlive(owner) || server.GetObjectPos(owner, &fx.pose.position) != DE_OK ||
        server.GetObjectRotation(owner, &fx.pose.rotation) != DE_OK)
        error = "PolyGrid owner/pose unavailable";
    else if (active.size() >= 128 || next_id_ == UINT64_MAX ||
             std::any_of(active.begin(), active.end(), [&](auto& i) { return i.owner == owner; }))
        error = "PolyGrid capacity exhausted or duplicate owner";
    else if (fx.Init(s, store, now, error))
    {
        report.id = fx.id = next_id_++;
        report.source = server.GetObjectName(owner);
        fx.visible = (server.GetObjectUserFlags(owner) & 1u) != 0;
        active.push_back({owner, std::move(fx)});
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
bool PolyGrids::advance(double now, ServerDE& server)
{
    bool ok = true;
    std::erase_if(active,
                  [&](auto& i)
                  {
                      auto& fx = i.effect;
                      bool live = server.IsObjectAlive(i.owner);
                      if (live && server.GetObjectPos(i.owner, &fx.pose.position) == DE_OK &&
                          fx.Update(now, (server.GetObjectUserFlags(i.owner) & 1u) != 0, phases))
                          return false;
                      if (live)
                      {
                          ++failed;
                          ok = false;
                          if (fx.error.empty())
                              fx.error = "PolyGrid position unavailable";
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
    phases = {0, 1, 4, 3};
    started = expired = failed = 0;
}
