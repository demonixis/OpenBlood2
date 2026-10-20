#include "game/explosion_rings.h"
#include "engine/collision.h"
#include <algorithm>
#include <cmath>
namespace OpenBlood2
{
    bool original_explosion_rings(DDWORD type, DVector pos, DVector normal, std::minstd_rand& random,
                                  std::vector<ExplosionRingSettings>& out, std::string& error)
    {
        out.clear();
        error.clear();
        if (!physics_vector(pos) || !physics_vector(normal) || vector_dot(normal, normal) < 1e-12f)
        {
            error = "Explosion requires a finite position and nonzero normal";
            return false;
        }
        if (type == 6 || type == 8)
            return true; // No ring branch in these high-detail recipes.
        if (type == 17)
        {
            const auto n = vector_scale(normal, 1 / std::sqrt(vector_dot(normal, normal)));
            ExplosionRingSettings s;
            s.position = vector_add(pos, vector_scale(n, 30));
            s.normal = n;
            s.color = {1, .85f, 0};
            s.radius = 7500;
            s.position_radius = 25;
            s.velocity = 1000;
            s.count = 32;
            s.duration = .5f;
            s.alpha = .75f;
            s.fade = 1;
            s.align = 1;
            s.texture = "SpriteTextures\\lensflare_1.dtx";
            out.push_back(s);
            return true;
        }
        if (type == 27)
        {
            const auto n = vector_scale(normal, 1 / std::sqrt(vector_dot(normal, normal)));
            ExplosionRingSettings s;
            s.position = vector_add(pos, vector_scale(n, 15));
            s.normal = n;
            s.color = {1, 1, .5f};
            s.radius = 4500;
            s.position_radius = 1;
            s.velocity = 75;
            s.count = 16;
            s.duration = .5f;
            s.alpha = .75f;
            s.fade = 1;
            s.align = 1;
            s.texture = "SpriteTextures\\flames1_09.dtx";
            s.delay = .1f;
            out.push_back(s);
            s.delay = .2f;
            out.push_back(s);
            return true;
        }
        if (type > 3)
        {
            error = "Explosion ring recipe unavailable";
            return false;
        }
        const float length = std::sqrt(vector_dot(normal, normal));
        normal = {normal.x / length, normal.y / length, normal.z / length};
        auto random_float = [&](float lo, float hi)
        {
            return lo + (hi - lo) * float(random() - std::minstd_rand::min()) /
                            float(std::minstd_rand::max() - std::minstd_rand::min());
        };
        auto position = [&](float offset)
        { return DVector{pos.x + normal.x * offset, pos.y + normal.y * offset, pos.z + normal.z * offset}; };
        ExplosionRingSettings s;
        s.position = position(25);
        s.normal = normal;
        s.radius = type == 0 ? 4000 : type == 2 ? 12000 : 8000;
        s.position_radius = 10;
        s.velocity = 400;
        s.count = 32;
        s.duration = .5f;
        s.fade = 1;
        s.texture = "SpriteTextures\\Firepart64.dtx";
        out.push_back(s);
        if (type == 3)
            for (unsigned i = 0; i < 3; ++i)
            {
                s.position = position(10 + 15 * i);
                s.radius = 5000;
                s.velocity = random_float(100, 150);
                s.gravity = random_float(20 * i, 30 + 20 * i);
                s.duration = 1.5f;
                s.alpha = .5f;
                s.delay = random_float(0, .5f);
                s.texture = "SpriteTextures\\Smoke32_" + std::to_string(4 + i) + ".dtx";
                out.push_back(s);
            }
        return true;
    }
    std::size_t ExplosionRings::reserved_particles() const
    {
        std::size_t count = 0;
        for (const auto& fx : active)
            count += fx.settings.count;
        return count;
    }
    std::uint64_t ExplosionRings::publish(DDWORD type, DVector position, DVector normal, std::string_view source,
                                          const Resources& resources, double now, std::string& error)
    {
        error.clear();
        if (source.size() > 100 || source.find('\0') != std::string_view::npos || next_event_ == UINT64_MAX)
        {
            error = "Invalid explosion ring source/event";
            return 0;
        }
        ExplosionRingReport report{next_event_++, type, std::string(source), {}, {}};
        std::vector<ExplosionRingSettings> settings;
        if (original_explosion_rings(type, position, normal, random_, settings, report.error))
            for (const auto& setting : settings)
            {
                CExplosionRingFX fx;
                std::string failure;
                if (active.size() >= 128 || reserved_particles() + setting.count > 2048 || next_id_ == UINT64_MAX)
                    failure = "Explosion ring capacity exhausted";
                else if (fx.Init(setting, failure) && fx.CreateObject(resources, now, failure))
                {
                    fx.id = next_id_++;
                    fx.event = report.id;
                    report.rings.push_back(fx.id);
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
        else
            ++failed;
        error = report.error;
        const auto id = report.id;
        if (reports.size() == 64)
            reports.erase(reports.begin());
        reports.push_back(std::move(report));
        return id;
    }
    bool ExplosionRings::advance(double now)
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
    void ExplosionRings::clear()
    {
        active.clear();
        reports.clear();
        update_error.clear();
        started = expired = failed = 0;
        random_.seed(1); // Client stream, independent of server and model RNG.
        // Do not reuse client identities across world unload.
    }
} // namespace OpenBlood2
