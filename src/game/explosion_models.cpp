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
        if (type == 18 || type == 23 || type == 24 || type == 25 || type == 28)
        {
            ExplosionModelSettings s;
            s.position = position;
            s.normal = normal;
            s.fade = 1;
            s.waveform = 1;
            s.random_rotation = 1;
            s.duration = .65f;
            s.alpha = .75f;
            s.scale1 = type == 28 ? DVector{10, 10, 10} : DVector{1, 1, 1};
            s.scale2 = type == 28 ? DVector{100, 100, 100} : DVector{35, 35, 35};
            s.model = "Models\\Explosions\\exp_sphere.abc";
            s.skin = type == 28 ? "Skins\\Explosions\\divineball.dtx" : "Skins\\Explosions\\rift.dtx";
            if (type == 18 || type == 23 || type == 24)
            {
                s.model = "Models\\Explosions\\blackhole.abc";
                s.skin = type == 18   ? "Skins\\Explosions\\blackhole.dtx"
                         : type == 24 ? "Skins\\Explosions\\leechball.dtx"
                                      : "Skins\\Explosions\\rift.dtx";
                s.scale1 = type == 18   ? DVector{.5f, .5f, .5f}
                           : type == 23 ? DVector{.05f, .05f, 1.5f}
                                        : DVector{.5f, .5f, 1.5f};
                s.scale2 = type == 18 ? DVector{3, 3, 3} : type == 23 ? DVector{2, 2, 3.5f} : DVector{25, 25, 15};
                s.rotations = {0, 0, -.05f};
                s.duration = type == 18 ? 5.5f : type == 23 ? 2.f : 8.f;
                s.alpha = 1;
                s.random_rotation = 2;
                if (type != 18)
                {
                    s.waveform = 4;
                    s.fade = 5;
                }
            }
            output = std::move(s);
            return true;
        }
        if (type == 17 || type == 19 || type == 22)
        {
            ExplosionModelSettings s;
            s.position = position;
            s.normal = normal;
            s.scale1 = {100, 100, 100};
            s.scale2 = {1000, 1000, 1000};
            s.rotations = {.05f, 0, 0};
            s.duration = .5f;
            s.alpha = .5f;
            s.waveform = 1;
            s.fade = 1;
            s.random_rotation = 0;
            s.model = type != 17 ? "Models\\Explosions\\exp_sphere.abc" : "Models\\Explosions\\duel_sphere.abc";
            s.skin = type == 19   ? "Skins\\Explosions\\zealotball.dtx"
                     : type == 22 ? "Skins\\Explosions\\divineball.dtx"
                                  : "Skins\\Explosions\\leechball2.dtx";
            output = std::move(s);
            return true;
        }
        if (type == 5 || (type >= 9 && type <= 11))
        {
            ExplosionModelSettings s;
            s.normal = vector_scale(normal, 1 / std::sqrt(vector_dot(normal, normal)));
            s.position = vector_add(position, vector_scale(s.normal, type == 11 ? 15.f : 10.f));
            const float first = type == 5 ? 25.f : type == 11 ? 10.f : 50.f;
            const float last = type == 5 ? 250.f : type == 11 ? 150.f : 400.f;
            s.scale1 = {first, first, first};
            s.scale2 = {last, last, last};
            s.duration = type == 5 ? 1.f : .5f;
            s.alpha = type == 5 ? .5f : .75f;
            s.waveform = type == 11 ? (random() - random.min()) % 3 : 1;
            s.fade = 1;
            s.random_rotation = 1;
            if (type == 5)
            {
                s.model = "Models\\Explosions\\Exp_sphere_2.abc";
                s.skin = "Skins\\Explosions\\Explosion.dtx";
            }
            output = std::move(s);
            return true;
        }
        if (type == 7)
        {
            ExplosionModelSettings s;
            s.position = position;
            s.normal = vector_scale(normal, 1 / std::sqrt(vector_dot(normal, normal)));
            s.scale1 = {20, 20, 20};
            s.scale2 = {100, 100, 100};
            s.duration = .5f;
            s.alpha = .99f;
            s.waveform = 1;
            s.fade = 1;
            s.random_rotation = 1;
            s.skin = "Skins\\Explosions\\eballsmall.dtx";
            output = std::move(s);
            return true;
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
        if (type == 6 || type == 12 || (type >= 13 && type <= 16) || type == 21 || type == 26 || type == 27 ||
            (type >= 29 && type <= 31))
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
        ExplosionModelReport report{next_event_++, 0, type, std::string(source), {}, {}};
        std::vector<ExplosionModelSettings> settings;
        if (type == 4)
        {
            if (!physics_vector(position) || !physics_vector(normal) || vector_dot(normal, normal) < 1e-12f)
                report.error = "Invalid napalm explosion pose";
            else
            {
                ExplosionModelSettings s;
                s.normal = vector_scale(normal, 1 / std::sqrt(vector_dot(normal, normal)));
                s.position = vector_add(position, vector_scale(s.normal, 10));
                s.fade = 1;
                s.random_rotation = 1;
                s.model = "Models\\Explosions\\Exp_sphere_2.abc";
                s.scale1 = {20, 20, 20};
                s.scale2 = {400, 400, 400};
                s.duration = .75f;
                s.alpha = 1;
                s.waveform = 0;
                settings.push_back(s);
                s.scale1 = {35, 35, 35};
                s.scale2 = {350, 350, 350};
                s.alpha = .75f;
                s.waveform = 2;
                settings.push_back(s);
                s.scale1 = {50, 50, 50};
                s.scale2 = {450, 450, 450};
                s.duration = 1;
                s.alpha = .5f;
                s.waveform = 1;
                s.skin = "Skins\\Explosions\\Explosion.dtx";
                settings.push_back(s);
            }
        }
        else
        {
            std::optional<ExplosionModelSettings> setting;
            if (original_explosion_model(type, position, normal, random_, setting, report.error) && setting)
            {
                settings.push_back(*setting);
                if (type == 18 || type == 23)
                {
                    // Source reuses emCS: the first model's Z rotation remains.
                    setting->model = "Models\\Explosions\\exp_sphere.abc";
                    setting->skin =
                        type == 18 ? "Skins\\Explosions\\blackhole2.dtx" : "Skins\\Explosions\\riftball.dtx";
                    const float first = type == 18 ? 100.f : 20.f, last = type == 18 ? 800.f : 400.f;
                    setting->scale1 = {first, first, first};
                    setting->scale2 = {last, last, last};
                    setting->duration = .5f;
                    setting->alpha = .5f;
                    setting->waveform = 1;
                    setting->fade = 1;
                    setting->random_rotation = 0;
                    settings.push_back(*setting);
                }
            }
        }
        std::vector<CExplosionModelFX> prepared;
        if (report.error.empty() && (active.size() + settings.size() > 128 || settings.size() > UINT64_MAX - next_id_))
            report.error = "Explosion model capacity exhausted";
        for (const auto& setting : settings)
        {
            if (!report.error.empty())
                break;
            CExplosionModelFX fx;
            if (!fx.Init(setting, report.error) || !fx.CreateObject(models, resources, now, random_, report.error))
                break;
            prepared.push_back(std::move(fx));
        }
        if (report.error.empty())
            for (auto& fx : prepared)
            {
                fx.id = next_id_++;
                fx.event = report.id;
                if (!report.model)
                    report.model = fx.id;
                report.models.push_back(fx.id);
                active.push_back(std::move(fx));
                ++started;
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
