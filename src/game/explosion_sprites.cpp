#include "game/explosion_sprites.h"
#include "engine/collision.h"
#include <algorithm>
#include <cmath>
#include <numbers>
#include <random>
namespace OpenBlood2
{
    bool original_explosion_sprites(DDWORD type, DVector position, DVector normal,
                                    std::vector<ExplosionSpriteSettings>& out, std::string& unavailable,
                                    std::string& error, std::minstd_rand* random)
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
        else if (type == 4)
        {
            add(30, .75f, .75f, 1.5f, 1, 0, 0, "Sprites\\napalm.spr");
            add(25, .5f, 2.5f, 5, .85f, 1, 0, "Sprites\\smokepuff2.spr");
            unavailable = "light illumination";
        }
        else if (type == 5 || (type >= 9 && type <= 11))
        {
            if (type == 5)
                add(20, .4f, .4f, 1.5f, 1, 0, 0, "Sprites\\fireball2.spr");
            else
                add(type == 11 ? 15.f : 25.f, type == 11 ? .15f : .5f, type == 11 ? .5f : .75f, 1.5f, 1, 0, 0,
                    "Sprites\\howitzerexp.spr");
            if (type == 10)
            {
                std::minstd_rand fallback(1);
                auto& rng = random ? *random : fallback;
                auto draw = [&]() { return .6f + .3f * float(rng() - rng.min()) / float(rng.max() - rng.min()); };
                out.back().scale2 = {draw(), draw(), 0};
            }
            if (type != 11)
                add(25, .5f, 2.5f, 5, .85f, 1, 0, "Sprites\\smokepuff2.spr");
            unavailable = "light illumination";
        }
        else if (type >= 13 && type <= 15)
        {
            if (type == 14)
                add(0, .25f, .4f, .25f, 1, 0, 0, "Sprites\\debrisimp.spr");
            else
            {
                if (type == 15)
                    add(10, .25f, .75f, .5f, .75f, 0, 0, "Sprites\\explodesmall.spr");
                add(15, .25f, 1, 3, .85f, 1, 0, "Sprites\\smokepuff2.spr");
            }
            unavailable = "light illumination";
        }
        else if (type == 16 || type == 17)
        {
            if (type == 16)
            {
                add(10, .25f, .75f, .5f, .75f, 0, 0, "Sprites\\explodesmall.spr");
                add(15, .25f, 1, 3, .85f, 1, 0, "Sprites\\smokepuff2.spr");
            }
            else
                add(0, .25f, 1.5f, .75f, 1, 1, 2, "Sprites\\explode128.spr");
            unavailable = "light illumination";
        }
        else if (type == 6)
        {
            add(0, .25f, .25f, .5f, 1, 0, 1, "Sprites\\debrisimp.spr");
            add(20, .25f, 1.5f, 5, .85f, 1, 0, "Sprites\\smokepuff2.spr");
            unavailable = "light illumination";
        }
        else if (type == 7)
        {
            // Independent X/Y scale draws, before the random align=2 roll.
            std::minstd_rand fallback(1);
            auto& rng = random ? *random : fallback;
            auto draw = [&](float lo, float hi)
            { return lo + (hi - lo) * float(rng() - rng.min()) / float(rng.max() - rng.min()); };
            add(0, 0, 0, 1, 1, 0, 2, "Sprites\\TeslaImp.spr");
            auto& s = out.back();
            s.scale1 = {draw(.25f, .5f), draw(.25f, .5f), 0};
            s.scale2 = {draw(.5f, .75f), draw(.5f, .75f), 0};
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
        std::minstd_rand random(report.id); // Portable per-event cosmetic stream.
        std::vector<ExplosionSpriteSettings> settings;
        if (original_explosion_sprites(type, pos, normal, settings, report.unavailable, report.error, &random))
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
                const float roll = setting.align == 2 ? 2 * std::numbers::pi_v<float> * float(random() - random.min()) /
                                                            float(random.max() - random.min())
                                                      : 0;
                if (!fx.Init(setting, failure) || !fx.CreateObject(store, now, failure, roll))
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
                              update_error = fx.error;
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
        update_error.clear();
        started = expired = failed = 0;
        // IDs are not reused, including across world unload.
    }
} // namespace OpenBlood2
