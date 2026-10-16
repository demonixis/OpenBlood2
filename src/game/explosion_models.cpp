#include "game/explosion_models.h"
#include "engine/collision.h"
#include <algorithm>
#include <cmath>
namespace OpenBlood2
{
    bool original_explosion_model(DDWORD type, DVector position, DVector normal, std::minstd_rand& random,
                                  std::optional<ExplosionModelSettings>& output, std::string& error)
    {
        output.reset();
        error.clear();
        if (!physics_vector(position) || !physics_vector(normal) || vector_dot(normal, normal) < 1e-12f)
        {
            error = "Explosion requires a finite position and nonzero normal";
            return false;
        }
        if (type == 20)
        {
            ExplosionModelSettings s;
            s.position = position;
            s.normal = normal;
            s.scale1 = {200, 200, 200};
            s.scale2 = {1500, 1500, 1500};
            s.rotations = {.05f, 0, 0};
            s.duration = .5f;
            s.alpha = .5f;
            s.waveform = 1;
            s.fade = 1;
            s.random_rotation = 0;
            s.model = "Models\\Explosions\\exp_sphere.abc";
            s.skin = "Skins\\Explosions\\bmothball.dtx";
            output = std::move(s);
            return true;
        }
        if (type == 6)
            return true; // Original napalm fireball has no model component.
        if (type > 3 && type != 8)
        {
            error = "Explosion model recipe is unavailable";
            return false;
        }
        const float length = std::sqrt(vector_dot(normal, normal));
        normal = {normal.x / length, normal.y / length, normal.z / length};
        ExplosionModelSettings s;
        const float offset = type == 8 ? 30.f : 10.f;
        s.position = {position.x + normal.x * offset, position.y + normal.y * offset, position.z + normal.z * offset};
        s.normal = normal;
        const float first = type == 0 ? 25.f : type == 2 ? 100.f : 50.f;
        const float last = type == 0 ? 200.f : type == 2 ? 600.f : type == 8 ? 1000.f : 400.f;
        s.scale1 = {first, first, first};
        s.scale2 = {last, last, last};
        s.duration = type == 8 ? 3.f : .5f;
        s.alpha = .75f;
        s.waveform = type == 8 ? 0 : (random() - std::minstd_rand::min()) % 3;
        s.fade = 1;
        s.random_rotation = 1;
        if (type == 8)
        {
            s.model = "Models\\Explosions\\duel_sphere.abc";
            s.skin = "Spritetextures\\waterblue.dtx";
        }
        output = std::move(s);
        return true;
    }
    std::uint64_t ExplosionModels::publish(DDWORD type, DVector position, DVector normal, std::string_view source,
                                           ModelStore& models, const Resources& resources, double now,
                                           std::string& error)
    {
        error.clear();
        if (source.size() > 100 || source.find('\0') != std::string_view::npos || next_event_ == UINT64_MAX)
        {
            error = "Invalid explosion model source/event";
            return 0;
        }
        ExplosionModelReport report{next_event_++, 0, type, std::string(source), {}};
        std::optional<ExplosionModelSettings> setting;
        if (original_explosion_model(type, position, normal, random_, setting, report.error) && setting)
        {
            CExplosionModelFX fx;
            if (active.size() >= 128 || next_id_ == UINT64_MAX)
                report.error = "Explosion model capacity exhausted";
            else if (fx.Init(*setting, report.error) && fx.CreateObject(models, resources, now, random_, report.error))
            {
                report.model = fx.id = next_id_++;
                fx.event = report.id;
                active.push_back(std::move(fx));
                ++started;
            }
        }
        if (!report.error.empty())
            ++failed;
        error = report.error;
        const auto id = report.id;
        if (reports.size() == 64)
            reports.erase(reports.begin());
        reports.push_back(std::move(report));
        return id;
    }
    bool ExplosionModels::advance(double now)
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
    void ExplosionModels::clear()
    {
        active.clear();
        reports.clear();
        update_error.clear();
        started = expired = failed = 0;
        random_.seed(1); // Reset level visual RNG, never identity tokens.
    }
} // namespace OpenBlood2
