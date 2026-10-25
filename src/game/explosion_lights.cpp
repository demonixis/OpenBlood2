#include "game/explosion_lights.h"
#include "engine/collision.h"
#include <algorithm>
#include <cmath>
namespace OpenBlood2
{
    bool original_explosion_lights(DDWORD type, DVector position, DVector normal,
                                   std::vector<ExplosionLightSettings>& out, std::string& error)
    {
        out.clear();
        error.clear();
        if (!physics_vector(position) || !physics_vector(normal) || vector_dot(normal, normal) < 1e-12f)
        {
            error = "Explosion requires a finite position and nonzero normal";
            return false;
        }
        if (type >= 13 && type <= 15)
        {
            ExplosionLightSettings s;
            s.position = vector_add(position, vector_scale(normal, 10 / std::sqrt(vector_dot(normal, normal))));
            s.color1 = {1, .5f, 0};
            s.color2 = {1, 1, 1};
            s.duration = type == 13 ? 1.25f : type == 14 ? .75f : 1.f;
            s.radius1 = type == 13 ? 100.f : type == 14 ? 150.f : 75.f;
            s.radius2 = 0;
            out.push_back(s);
            if (type == 13)
            {
                s.color1 = {.2f, .2f, .2f};
                s.color2 = {.5f, .5f, .5f};
                s.duration = 25;
                s.radius1 = s.radius2 = 50;
                out.push_back(s);
            }
            return true;
        }
        if (type == 16 || type == 17 || type == 22)
        {
            ExplosionLightSettings s;
            s.position = vector_add(
                position, vector_scale(normal, (type == 16 ? 10.f : 15.f) /
                                                   (type == 22 ? 1.f : std::sqrt(vector_dot(normal, normal)))));
            s.color1 = s.color2 = type == 22 ? DVector{1, 0, 0} : DVector{1, 1, 0};
            s.duration = 1;
            s.radius1 = type == 16 ? 75.f : 300.f;
            s.radius2 = type == 16 ? 0.f : 150.f;
            out.push_back(s);
            return true;
        }
        if (type == 20)
        {
            ExplosionLightSettings s;
            s.position = vector_add(position, vector_scale(normal, 15)); // Raw source normal, even on a ray miss.
            s.color1 = s.color2 = {1, 1, 1};
            s.duration = 1;
            s.radius1 = 400;
            s.radius2 = 100;
            out.push_back(s);
            return true;
        }
        if (type == 27)
        {
            ExplosionLightSettings s;
            s.position = vector_add(position, vector_scale(normal, 10 / std::sqrt(vector_dot(normal, normal))));
            s.color1 = {1, .5f, 0};
            s.color2 = {1, 1, 1};
            s.duration = .75f;
            s.radius1 = 50;
            s.radius2 = 0;
            out.push_back(s);
            s.color1 = {.2f, .2f, .2f};
            s.color2 = {.5f, .5f, .5f};
            s.duration = 15;
            s.radius1 = 25; // Source writes Radius1 twice; Radius2 stays zero.
            out.push_back(s);
            return true;
        }
        if (type > 4 && type != 6 && type != 8)
        {
            error = "Explosion light recipe is unavailable";
            return false;
        }
        const float length = std::sqrt(vector_dot(normal, normal));
        const float offset = (type == 8 ? 30.f : 10.f) / length;
        ExplosionLightSettings s;
        s.position = {position.x + normal.x * offset, position.y + normal.y * offset, position.z + normal.z * offset};
        if (type == 8)
        {
            s.color1 = {.75f, .75f, 1};
            s.color2 = {.5f, .5f, .5f};
            s.duration = 3;
            s.radius1 = 100;
            s.radius2 = 350;
            out.push_back(s);
            return true;
        }
        s.color1 = {1, .5f, 0};
        s.color2 = {1, 1, 1};
        s.duration = 1.25f;
        s.radius1 = type == 4 ? 300.f : type == 0 || type == 6 ? 150.f : type == 1 ? 200.f : 250.f;
        s.radius2 = 0;
        out.push_back(s);
        s.color1 = {.2f, .2f, .2f};
        s.color2 = {.5f, .5f, .5f};
        s.duration = 25;
        s.radius1 = type == 0 || type == 6 ? 50.f : type == 1 ? 75.f : 100.f;
        // Source writes Radius1 twice: Radius2 remains zero from the preceding flash.
        out.push_back(s);
        return true;
    }
    std::uint64_t ExplosionLights::publish(DDWORD type, DVector position, DVector normal, std::string_view source,
                                           double now, std::string& error)
    {
        error.clear();
        if (source.size() > 100 || source.find('\0') != std::string_view::npos || next_event_ == UINT64_MAX)
        {
            error = "Invalid explosion light source/event";
            return 0;
        }
        ExplosionLightReport report{next_event_++, type, std::string(source), {}, {}};
        std::vector<ExplosionLightSettings> settings;
        if (original_explosion_lights(type, position, normal, settings, report.error))
            for (const auto& setting : settings)
            {
                CExplosionLightFX fx;
                std::string failure;
                if (active.size() >= 256 || next_id_ == UINT64_MAX)
                    failure = "Explosion light capacity exhausted";
                else if (fx.Init(setting, failure) && fx.CreateObject(now, failure))
                {
                    fx.id = next_id_++;
                    fx.event = report.id;
                    report.lights.push_back(fx.id);
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
    bool ExplosionLights::advance(double now)
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
    void ExplosionLights::clear()
    {
        active.clear();
        reports.clear();
        update_error.clear();
        started = expired = failed = 0;
        // Do not reuse client identities across world unload.
    }
} // namespace OpenBlood2
