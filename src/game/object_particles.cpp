#include "game/object_particles.h"
#include <algorithm>
namespace OpenBlood2
{
    std::uint64_t ObjectParticles::publish(HOBJECT owner, unsigned recipe, ServerDE& server, const Resources* resources,
                                           double now, std::string& error)
    {
        error.clear();
        ObjectParticleReport report;
        report.recipe = recipe;
        NodePose pose;
        if (!server.IsObjectAlive(owner))
            error = "Object particle owner unavailable";
        else
        {
            report.source = server.GetObjectName(owner);
            if (!resources || server.GetObjectPos(owner, &pose.position) != DE_OK ||
                server.GetObjectRotation(owner, &pose.rotation) != DE_OK)
                error = "Object particle resources/pose unavailable";
            else if (recipe != 19 && recipe != 36 && recipe != 21 && recipe != 24 && recipe != 31 && recipe != 32)
                error = "Object-bound particle recipe unavailable";
            else if (active.size() >= 128 || next_id_ == UINT64_MAX)
                error = "Object particle system capacity exhausted";
            else
            {
                CObjectParticleFX fx;
                if (fx.Init(recipe, next_id_, random_, error) && fx.CreateObject(*resources, pose, now, error))
                {
                    report.id = fx.id = next_id_++;
                    active.push_back({owner, std::move(fx)});
                    ++started;
                }
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
    std::size_t ObjectParticles::particle_count() const
    {
        std::size_t count = 0;
        for (const auto& instance : active)
            count += instance.effect.particles.size();
        return count;
    }
    bool ObjectParticles::advance(double now, ServerDE& server)
    {
        bool valid = true;
        std::size_t total = particle_count();
        std::erase_if(active,
                      [&](auto& instance)
                      {
                          auto& fx = instance.effect;
                          const auto previous = fx.particles.size();
                          const bool live = server.IsObjectAlive(instance.owner);
                          NodePose pose;
                          const bool pose_ok =
                              !live || (server.GetObjectPos(instance.owner, &pose.position) == DE_OK &&
                                        server.GetObjectRotation(instance.owner, &pose.rotation) == DE_OK);
                          const auto others = total - previous;
                          const std::size_t budget = others <= 8192 ? std::min<std::size_t>(2048, 8192 - others) : 0;
                          if (pose_ok && fx.Update(now, live ? &pose : nullptr, random_, budget))
                          {
                              total = others + fx.particles.size();
                              return false;
                          }
                          total = others;
                          if (!pose_ok)
                              fx.error = "Object particle owner pose unavailable";
                          if (fx.error.empty())
                              ++expired;
                          else
                          {
                              ++failed;
                              valid = false;
                              update_error = fx.error;
                              for (auto& report : reports)
                                  if (report.id == fx.id)
                                      report.error = fx.error;
                          }
                          return true;
                      });
        return valid;
    }
    void ObjectParticles::clear()
    {
        active.clear();
        reports.clear();
        started = expired = failed = 0;
        update_error.clear();
        random_.seed(1); // IDs remain monotonic across unloads.
    }
} // namespace OpenBlood2
