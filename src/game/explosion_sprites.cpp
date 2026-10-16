#include "game/explosion_sprites.h"
#include "engine/collision.h"
#include <algorithm>
#include <cmath>
namespace OpenBlood2
{
    bool original_explosion_sprites(DDWORD type, DVector position, DVector normal,
                                    std::vector<ExplosionSpriteSettings>& out, std::string& unavailable,
                                    std::string& error)
    {
        out.clear();
        unavailable.clear();
        error.clear();
        if (!physics_vector(position) || !physics_vector(normal) || vector_dot(normal, normal) < 1e-12f)
        {
            error = "Explosion requires a finite position and nonzero normal";
            return false;
        }
        const float length = std::sqrt(vector_dot(normal, normal));
        normal = {normal.x / length, normal.y / length, normal.z / length}; // Original CExplosionFX::Init.
        auto add = [&](float offset, float first, float last, float duration, float alpha, DBYTE wave, DBYTE align,
                       const char* path)
        {
            ExplosionSpriteSettings s;
            s.normal = normal;
            s.position = {position.x + normal.x * offset, position.y + normal.y * offset,
                          position.z + normal.z * offset};
            s.scale1 = {first, first, 0};
            s.scale2 = {last, last, 0};
            s.duration = duration;
            s.alpha = alpha;
            s.waveform = wave;
            s.fade = 1;
            s.align = align;
            s.resource = path;
            out.push_back(std::move(s));
        };
        if (type <= 3)
        {
            const float fire = type == 0 ? .25f : type == 2 ? .75f : .5f;
            add(25, fire, fire, 1.5f, 1, 0, 0, "Sprites\\fireball2.spr");
            add(25, type == 0 ? .25f : .5f,
                type == 0   ? 1.5f
                : type == 1 ? 2.f
                            : 2.5f,
                5, .85f, 1, 0, "Sprites\\smokepuff2.spr");
            unavailable = "light illumination";
        }
        else if (type == 6)
        {
            add(0, .25f, .25f, .5f, 1, 0, 1, "Sprites\\debrisimp.spr");
            add(20, .25f, 1.5f, 5, .85f, 1, 0, "Sprites\\smokepuff2.spr");
            unavailable = "light illumination";
        }
        else if (type == 8)
        {
            add(30, .1f, .75f, 3, .75f, 0, 0, "Sprites\\TesAltLoop.spr");
            unavailable = "light illumination";
        }
        else
        {
            error = "Explosion preset has no supported sprite branch";
            unavailable = "complete composite";
            return false;
        }
        return true;
    }
    std::uint64_t ExplosionSprites::publish(DDWORD type, DVector pos, DVector normal, std::string_view source,
                                            SpriteStore& store, double now, std::string& error)
    {
        error.clear();
        if (source.size() > 100 || source.find('\0') != std::string_view::npos || next_event_ == UINT64_MAX)
        {
            error = "Invalid explosion source/event identity";
            return 0;
        }
        ExplosionSpriteReport report;
        report.id = next_event_++;
        report.type = type;
        report.source = source;
        std::vector<ExplosionSpriteSettings> settings;
        if (original_explosion_sprites(type, pos, normal, settings, report.unavailable, report.error))
            for (const auto& setting : settings)
            {
                if (active.size() >= 256 || next_id_ == UINT64_MAX)
                {
                    report.error = "Explosion sprite capacity exhausted";
                    ++failed;
                    break;
                }
                CExplosionSpriteFX fx;
                std::string failure;
                if (!fx.Init(setting, failure) || !fx.CreateObject(store, now, failure))
                {
                    ++failed;
                    if (!report.error.empty())
                        report.error += "; ";
                    report.error += setting.resource.value_or("default") + ": " + failure;
                    continue; // Source CreateSFX failure does not cancel sibling requests.
                }
                fx.id = next_id_++;
                fx.event = report.id;
                report.sprites.push_back(fx.id);
                active.push_back(std::move(fx));
                ++started;
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
    bool ExplosionSprites::advance(double now)
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
                          }
                          return true;
                      });
        return valid;
    }
    void ExplosionSprites::clear()
    {
        active.clear();
        reports.clear();
        started = expired = failed = 0;
        // IDs are not reused, including across world unload.
    }
} // namespace OpenBlood2
