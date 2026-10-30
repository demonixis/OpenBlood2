#include "game/object_particles.h"
#include "engine/collision.h"
#include <algorithm>
#include <cmath>
namespace OpenBlood2
{
    std::uint64_t ObjectParticles::publish(const EffectOwner& owner, unsigned recipe, const Resources* resources,
                                           double now, std::string& error, float lifetime_scale, DVector offset)
    {
        error.clear();
        ObjectParticleReport report;
        report.recipe = recipe;
        const auto& pose = owner.pose;
        if (!owner)
            error = "Object particle owner unavailable";
        else
        {
            report.source = owner.name;
            if (!resources)
                error = "Object particle resources/pose unavailable";
            else if (!physics_vector(offset) || ((offset.x || offset.y || offset.z) && recipe != 37 && recipe != 38))
                error = "Unsupported object particle offset";
            else if (!std::isfinite(lifetime_scale) || lifetime_scale < .25f || lifetime_scale > 2 ||
                     (recipe != 3 && recipe != 4 && recipe != 27 && recipe != 31 && lifetime_scale != 1))
                error = "Unsupported object particle lifetime scale";
            else if (recipe != 9 && recipe != 37 && recipe != 38 && recipe != 39 && recipe != 1 && recipe != 2 &&
                     recipe != 6 && recipe != 8 && recipe != 3 && recipe != 4 && recipe != 27 && recipe != 18 &&
                     recipe != 19 && recipe != 36 && recipe != 21 && recipe != 24 && recipe != 31 && recipe != 32 &&
                     recipe != 33 && (recipe < 12 || recipe > 17))
                error = "Object-bound particle recipe unavailable";
            else if (active.size() >= 128 || next_id_ == UINT64_MAX)
                error = "Object particle system capacity exhausted";
            else
            {
                CObjectParticleFX fx;
                if (fx.Init(recipe, next_id_, random_, error))
                {
                    if (recipe == 37 || recipe == 38)
                        fx.offset = offset;
                    fx.min_life *= lifetime_scale;
                    fx.max_life *= lifetime_scale;
                }
                if (error.empty() && fx.CreateObject(*resources, pose, now, error))
                {
                    report.id = fx.id = next_id_++;
                    active.push_back({owner.id, std::move(fx)});
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
    bool ObjectParticles::advance(double now, const ClientWorld& world)
    {
        bool valid = true;
        std::size_t total = particle_count();
        std::erase_if(active,
                      [&](auto& instance)
                      {
                          auto& fx = instance.effect;
                          const auto previous = fx.particles.size();
                          const auto* owner = world.get(instance.owner);
                          const auto others = total - previous;
                          const std::size_t budget = others <= 8192 ? std::min<std::size_t>(2048, 8192 - others) : 0;
                          if (fx.Update(now, owner ? &owner->pose : nullptr, random_, budget))
                          {
                              total = others + fx.particles.size();
                              return false;
                          }
                          total = others;
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
