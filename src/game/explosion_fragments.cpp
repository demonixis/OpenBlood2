#include "game/explosion_fragments.h"
#include "engine/collision.h"
#include <algorithm>
#include <cmath>
namespace OpenBlood2
{
    std::uint64_t ExplosionFragments::publish(DDWORD type, DVector pos, DVector normal, std::string_view source,
                                              ModelStore& models, const Resources& resources, double now,
                                              std::string& error)
    {
        error.clear();
        if (source.size() > 100 || source.find('\0') != std::string_view::npos || next_event_ == UINT64_MAX)
        {
            error = "Invalid explosion fragment source/event";
            return 0;
        }
        ExplosionFragmentReport report{next_event_++, type, std::string(source), {}, {}, {}, {}};
        if (!physics_vector(pos) || !physics_vector(normal) || vector_dot(normal, normal) < 1e-12f ||
            (type > 4 && type != 6 && type != 8 && type != 21))
        {
            report.error = "Invalid explosion or unavailable fragment recipe";
            ++failed;
        }
        else if (type == 3 || type == 21)
        {
            normal = vector_scale(normal, 1 / std::sqrt(vector_dot(normal, normal)));
            auto draw = [&](float lo, float hi)
            {
                return lo + (hi - lo) * float(random_() - std::minstd_rand::min()) /
                                float(std::minstd_rand::max() - std::minstd_rand::min());
            };
            // Original loop draws a NEW bound at every condition, not one count.
            for (unsigned i = 0; (type == 21 ? i < 10 : i < 2 + (random_() - std::minstd_rand::min()) % 4); ++i)
            {
                ExplosionFragSettings s;
                s.position = pos;
                s.normal = normal;
                s.scale = type == 21 ? DVector{.25f, .25f, .25f} : DVector{1, 1, 1};
                s.rotate_max = {1, 1, 1};
                s.bounce = type == 21 ? .5f : .25f;
                s.duration = type == 21 ? 7 : 10;
                s.gravity = type == 21 ? .125f : 1;
                s.fade_time = 2;
                s.trail_type = type == 21 ? 34 : 1;
                s.model = "Models\\Gibs\\gibmetal2.abc";
                s.skin = "Skins\\Gibs\\gibmetal3.dtx";
                s.spread = type == 21 ? draw(2, 15) : draw(40, 60);
                s.velocity = type == 21 ? draw(3, 6) : draw(20, 40);
                CExplosionFragFX fx;
                std::string failure;
                if (active.size() >= 128 || next_id_ == UINT64_MAX)
                    failure = "Explosion fragment capacity exhausted";
                else if (fx.Init(s, failure) && fx.CreateObject(models, resources, now, random_, failure))
                {
                    fx.id = next_id_++;
                    fx.event = report.id;
                    CObjectParticleFX trail;
                    std::string trail_failure;
                    if (trails.size() >= 128 || next_trail_ == UINT64_MAX)
                        trail_failure = "Smoke-trail system capacity exceeded";
                    else if (trail.Init(fx.settings.trail_type, fx.id, trail_failure) &&
                             trail.CreateObject(resources, {fx.position, fx.rotation}, now, trail_failure))
                    {
                        trail.id = next_trail_++;
                        trail.event = report.id;
                        report.trails.push_back(trail.id);
                        trails.push_back(std::move(trail));
                        ++trail_started;
                    }
                    fx.trail_error = trail_failure;
                    if (!trail_failure.empty())
                    {
                        ++trail_failed;
                        report.unavailable = trail_failure;
                        if (type == 21)
                        {
                            if (!report.error.empty())
                                report.error += "; ";
                            report.error += trail_failure;
                        }
                    }
                    report.fragments.push_back(fx.id);
                    active.push_back(std::move(fx));
                    ++started;
                }
                if (!failure.empty())
                {
                    ++failed;
                    if (!report.error.empty())
                        report.error += "; ";
                    report.error += failure;
                }
            }
        }
        error = report.error;
        const auto id = report.id;
        if (reports.size() == 64)
            reports.erase(reports.begin());
        reports.push_back(std::move(report));
        return id;
    }
    bool ExplosionFragments::advance(double now, ServerDE& server)
    {
        bool valid = true;
        std::erase_if(active,
                      [&](auto& fx)
                      {
                          if (fx.Update(now, server))
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
        // Original SFX order: fragments (36) move/remove before object particles (52).
        std::size_t total = trail_particles();
        std::erase_if(trails,
                      [&](auto& trail)
                      {
                          const auto previous = trail.particles.size();
                          const auto parent = std::find_if(active.begin(), active.end(),
                                                           [&](const auto& f) { return f.id == trail.parent; });
                          NodePose pose;
                          if (parent != active.end())
                              pose = {parent->position, parent->rotation};
                          // The authored staggered train exit exceeds the old 2048-particle
                          // per-trail cap. Keep a bounded pool without dropping source emissions.
                          const std::size_t budget =
                              total - previous <= 16384
                                  ? std::min<std::size_t>(max_particle_capacity, 16384 - (total - previous))
                                  : 0;
                          if (trail.Update(now, parent == active.end() ? nullptr : &pose, trail_random_, budget))
                          {
                              total = total - previous + trail.particles.size();
                              return false;
                          }
                          total -= previous;
                          if (trail.error.empty())
                              ++trail_expired;
                          else
                          {
                              ++trail_failed;
                              valid = false;
                              update_error = trail.error;
                              for (auto& report : reports)
                                  if (report.id == trail.event)
                                      report.unavailable = trail.error;
                              if (parent != active.end())
                                  parent->trail_error = trail.error;
                          }
                          return true;
                      });
        return valid;
    }
    std::size_t ExplosionFragments::trail_particles() const
    {
        std::size_t count = 0;
        for (const auto& t : trails)
            count += t.particles.size();
        return count;
    }
    void ExplosionFragments::clear()
    {
        active.clear();
        trails.clear();
        trail_started = trail_expired = trail_failed = 0;
        trail_random_.seed(1);
        reports.clear();
        update_error.clear();
        started = expired = failed = 0;
        random_.seed(1);
    }
} // namespace OpenBlood2
